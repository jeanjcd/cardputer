> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/architecture.md).

# Arquitetura

Como o firmware é organizado, como um quadro passa por ele, o que ele guarda e quanta memória usa. Isto descreve a v0.1.0. Os diagramas abaixo são blocos de código e ficam em inglês, como no código-fonte.

## Mapa de módulos

```
src/
  main.cpp        setup(): M5Cardputer.begin → App::begin → Ui::begin
                  loop():  keyboard update → Ui::handleKeys → App::tick → Ui::render
  mesh/           portable protocol core: no Arduino/ESP-IDF headers, built and tested natively
  hw/             hardware: SX1262 radio, NVS storage, persistent identity
  app/            application logic (App) and user interface (Ui)
lib/crypto/       vendored rweather/Crypto subset (AES, CTR, SHA-256, Curve25519), MIT
test/             native Unity test suites
tools/            gen_vectors.py (reference vectors), sanitize_link.py (sanitizer link flags)
```

Em resumo: `src/mesh/` é o núcleo portátil do protocolo (sem headers Arduino/ESP-IDF, compilado e testado nativamente); `src/hw/` é o hardware (rádio SX1262, armazenamento NVS, identidade persistente); `src/app/` é a lógica do aplicativo (`App`) e a interface (`Ui`); `lib/crypto/` é um subconjunto incorporado da biblioteca rweather/Crypto (MIT); `test/` contém as suítes de teste nativas Unity; `tools/` contém os scripts auxiliares.

### `src/mesh/` (núcleo portátil)

| Módulo | Responsabilidade |
|---|---|
| [`header`](../../src/mesh/header.h) | Leitura/escrita do cabeçalho de 16 bytes no ar |
| [`pb`](../../src/mesh/pb.h) | Subconjunto de protobuf: `Data`, `User`, `Routing` |
| [`crypto`](../../src/mesh/crypto.h), [`aes_ccm`](../../src/mesh/aes_ccm.h) | AES-128-CTR, X25519 + SHA-256, AES-256-CCM; [`secure_zero.h`](../../src/mesh/secure_zero.h) apaga segredos |
| [`packet`](../../src/mesh/packet.h) | Monta os quadros de saída (texto, DM, NodeInfo, ACK/NAK), interpreta e decifra os quadros de entrada (`parseFrame`), IDs de pacote, `DedupCache` |
| [`region`](../../src/mesh/region.h) | Tabela de regiões, fórmula da frequência, limite de potência, hash do canal, tempo no ar LoRa, `AirtimeBudget` |
| [`nodedb`](../../src/mesh/nodedb.h) | Nós conhecidos (RAM) e contatos persistidos (NVS), a primeira chave vence |
| [`chat`](../../src/mesh/chat.h) | Histórico de mensagens num pool compartilhado |
| [`greeter`](../../src/mesh/greeter.h) | Fila da saudação automática, intervalo e conjunto persistido de cumprimentados |
| [`outbox`](../../src/mesh/outbox.h) | DMs de saída, inclusive as que esperam uma chave |
| [`ratelimit`](../../src/mesh/ratelimit.h) | Limitadores por nó e globais para respostas automáticas |
| [`settings`](../../src/mesh/settings.h) | Configurações do usuário, validação, codificação do blob, montagem da saudação |
| [`text`](../../src/mesh/text.h) | Funções UTF-8 com limites, conversão para exibição em CP437 |
| [`hwmodel`](../../src/mesh/hwmodel.h) | ID do modelo de hardware → nome e família do chip |
| [`kv`](../../src/mesh/kv.h) | Armazenamento abstrato de blobs chave/valor (NVS no aparelho, um map nos testes) |

### `src/hw/` (hardware)

| Módulo | Responsabilidade |
|---|---|
| [`radio`](../../src/hw/radio.h) | Chave de antena PI4IOE, SX1262 via RadioLib, RX por interrupção, TX bloqueante, ressintonia, potência |
| [`nvs_kv`](../../src/hw/nvs_kv.h) | `Kv` sobre a NVS do ESP32 (Arduino `Preferences`, namespace `meshhello`), leituras de tamanho exato |
| [`identity`](../../src/hw/identity.h) | Carrega ou cria o par de chaves X25519, número do nó a partir do MAC, entropia de hardware |

### `src/app/` (aplicativo)

| Módulo | Responsabilidade |
|---|---|
| [`app`](../../src/app/app.h) | Dono de todo o estado. Tratamento de RX, agendamento, fila de TX, orçamento de duty cycle, ações de configuração, avisos. |
| [`ui`](../../src/app/ui.h) | Quatro abas na tela de 240 × 135, tratamento do teclado, canvas fora da tela |

A interface lê o estado apenas pelos acessores const de `App` e age pelos métodos de ação dele (`sendText`, `cycleRegion`, `toggleAutoGreet`, ...). `App` incrementa um contador de versão a cada mudança visível. A interface redesenha quando ele muda (no máximo a cada 50 ms), e pelo menos uma vez por segundo por causa dos tempos e avisos.

## Fluxo de dados

```
                 SX1262 (DIO1 IRQ)
                        |
                 Radio::poll  ----------------------------- rx count, last RSSI
                        |
                 headerParse  -- drop: from == us / from == broadcast
                        |
                 DedupCache.contains(from, id)?  -- yes: re-ACK if it is a DM we ACKed; drop
                        |
                 parseFrame (decrypt: PKI if channel 0x00 and to == us, CTR if channel 0x08)
                        |
        +---------------+------------------+---------------------+
   undecodable     decoded, other port   NodeInfo / Text / Ack
   (PKI w/o key:        |                       |
    NAK 35 +       dedup.record           dedup.record, NodeDb.touch
    NodeInfo)      NodeDb.touch                 |
                                 +--------------+--------------+
                             NodeInfo         Text             Ack/NAK
                          NodeDb.applyUser  ChatStore.add     ChatStore: v / x
                          (first key wins)  ACK if want_ack
                          Greeter.onNodeWithKey
                          Outbox.onKeyArrived
                          NodeInfo reply (rate-limited)

   UI actions / timers
        |
   App::schedule (only when the TX queue is empty), in priority order:
        1. pending NodeInfo broadcast (boot, 3 h, rename, manual; 30 s spacing)
        2. Outbox.next  -> buildDirectText (PKI)
        3. Greeter.due  -> greeting DM
   ACKs and NAKs are inserted at the front of the queue (urgent); NodeInfo replies,
   NodeInfo requests and public texts are appended directly when they arise.
        |
   TX queue (6 frames)
        |
   App::transmitOne: >= 1.5 s since the last TX end, AirtimeBudget.allows(...)
        |
   Radio::send (blocking) -> back to RX; AirtimeBudget.record; message state ~ -> . or x
```

Em palavras: um quadro recebido passa pelo `Radio::poll` (que atualiza `rx` e o RSSI), pela leitura do cabeçalho (descarta quadros vindos de nós mesmos ou do endereço de broadcast), pelo cache de duplicatas e pelo `parseFrame`, que decifra com PKI ou CTR. Quadros não decodificáveis de PKI sem chave geram um NAK 35 mais NodeInfo. NodeInfo atualiza o NodeDb (a primeira chave vence), alimenta o Greeter e o Outbox e pode gerar uma resposta com limite de frequência; textos vão para o ChatStore (com ACK se pedido); ACK/NAK mudam o estado da mensagem para `v` ou `x`. Na saída, `App::schedule` só enche a fila de TX quando ela está vazia, em ordem de prioridade: difusão de NodeInfo pendente, depois DMs do Outbox, depois saudações. ACKs e NAKs entram na frente da fila; respostas e pedidos de NodeInfo e textos públicos entram direto no fim.

Pontos principais:

- **Sem heap no caminho de RX.** A struct `Inbound` decodificada é um buffer estático, e todos os buffers têm tamanho fixo com limites explícitos.
- **Dedup depois do parse.** Um par só é registrado depois que o quadro foi tratado, então uma DM PKI que chega antes da chave do remetente ainda pode ser lida quando for retransmitida.
- **Uma transmissão de cada vez.** `schedule()` só reabastece a fila quando ela está vazia. `transmitOne()` envia no máximo um quadro por volta do loop, pelo menos **1.5 s** depois do fim do anterior, e só se o orçamento de duty cycle da região permitir. Caso contrário, o quadro da frente (e tudo atrás dele) espera, e a barra de status mostra `duty limit`.
- **TX bloqueante.** `Radio::send` bloqueia até o fim do TX (até cerca de 2.2 s para 255 bytes), então as teclas não são tratadas nesse intervalo.

## Persistência (NVS)

Tudo é guardado na partição NVS do ESP32 sob o namespace **`meshhello`**, como blobs de tamanho fixo. Uma leitura só dá certo se o blob guardado tiver exatamente o tamanho esperado. Bytes de versão e validação de campos rejeitam qualquer outra coisa, e os valores padrão correspondentes são usados.

| Chave | Tamanho | Layout | Gravado quando |
|---|---|---|---|
| `idkey` | 65 B | `[ver=1][priv 32][pub 32]` | Uma vez, na primeira inicialização (ou se o registro estiver ausente/corrompido). Validado ao carregar: `pub` precisa ser igual a X25519(`priv`, 9). |
| `settings` | 235 B | `[ver=1][region id u8][txPower i8][flags u8: bit0 custom greeting][greeting 201][longName 25][shortName 5]`; strings UTF-8 completadas com NUL | Mudança de potência, nome ou saudação; mudança de região (com espera de 2 s ou ao sair da Config) |
| `greeted` | 2003 B | `[ver=1][count u16 LE][500 × node number u32 LE]` | Cada vez que um nó é cumprimentado (automática ou manualmente) |
| `peer0` … `peer49` | 89 B cada | `[ver=1][node number u32 LE][seq u32 LE][longName 40][shortName 8][public key 32]` | Quando trocamos uma DM com um nó com chave (uma vez por contato por inicialização); quando os 50 estão ocupados, o slot gravado há mais tempo é reutilizado |
| `region` | 1 B | ID da região | **Só leitura**, como alternativa vinda de uma versão de diagnóstico antiga quando `settings` não existe |

IDs de região (persistidos): `0` ANZ, `1` BR_902, `2` US, `3` EU_868, `4` RU, `5` KR, `6` UA_868, `7` MY_919, `8` SG_923, `9` PH_868, `10` PH_915, `11` IN, `12` NZ_865, `13` NP_865, `14` KZ_863, `15` JP, `16` TW, `17` TH.

O histórico de conversas, a lista de nós (fora os contatos salvos), os contadores de não lidas, o liga/desliga da saudação automática e o orçamento de tempo no ar ficam **só na RAM**.

## Orçamento de RAM

Tamanhos estáticos aproximados no ESP32-S3 (32 bits):

| Estrutura | Tamanho | Observações |
|---|---|---|
| `ChatStore` | ~21 KB | 96 slots × 224 B (texto de 200 bytes + estado + contato + sequência) |
| `NodeDb` | ~13 KB | 100 entradas × 128 B + 50 registros de slots persistidos |
| Buffer de linhas da UI | ~9 KB | 210 linhas quebradas × 44 B para desenhar o histórico |
| `Outbox` | ~2.2 KB | 10 × 216 B |
| `Greeter` | ~2 KB | 500 IDs cumprimentados + 10 na fila |
| Fila de TX | ~1.6 KB | 6 × 264 B |
| `DedupCache` | 0.5 KB | 64 × (from, id) |
| Limitadores de frequência | ~0.9 KB | 3 × 32 slots |
| Canvas da tela | ~63 KB (heap) | 240 × 135 × 16 bits, alocado uma vez no início; se a alocação falhar, a UI desenha direto na tela |

O blob `greeted` (2 KB) é montado na pilha enquanto é carregado ou salvo.

## Limites

| Limite | Valor | Origem |
|---|---|---|
| Tamanho do quadro | 255 B (16 de cabeçalho + 239 de payload) | `Radio::kMaxFrame`, `packet.cpp` |
| Texto por mensagem | 199 bytes | `Settings::kMaxGreeting`, `ChatMsg::text[200]` |
| `Data.payload` aceito | 233 B | `MeshData::payload` |
| Nome longo / curto | 24 / 4 bytes | `Settings` |
| Nós na RAM | 100 | `NodeDb::kMaxNodes` |
| Contatos persistidos | 50 | `NodeDb::kMaxPeers` |
| Mensagens por conversa / total | 30 / 96 | `ChatStore` |
| Outbox (na fila + esperando chave) | 10 | `Outbox::kMax` |
| Fila de TX | 6 quadros | `App::kTxQueue` |
| Conjunto de cumprimentados / fila de saudação | 500 / 10 | `Greeter` |
| Cache de duplicatas | 64 pares | `DedupCache` |
| Memória de reconfirmação (re-ACK) | 8 DMs | `App::ackFrom_` |
| Intervalo entre os nossos quadros | 1.5 s | `kTxGapMs` |
| Intervalo da saudação automática | 30 s | `Greeter` |
| Período do NodeInfo | 3 h | `kNodeInfoPeriodMs` |
| Intervalo do NodeInfo manual / por renomeação | 30 s | `kManualNodeInfoMs` |
| Pedido de NodeInfo a um destino de DM sem chave | 1 a cada 60 s por nó | `kNodeInfoRequestMs` |
| Respostas de NodeInfo | 1 a cada 12 h por nó, 1 a cada 30 s no total | `niReplies_` |
| NAK + NodeInfo para PKI com chave desconhecida | 1 a cada 10 min por nó, 1 a cada 30 s no total | `unknownKey_` |
| Espera para salvar configurações (região) | 2 s | `App::tick` |
| Duração do aviso | 2.5 s | `kToastMs` |
| Rearme do RX sem atividade | 10 min | `kRxRearmMs` |
| Nova tentativa do Greeter após falha na NVS | 60 s | `Greeter::kRetryBackoffMs` |
