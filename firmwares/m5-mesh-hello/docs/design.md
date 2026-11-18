# M5-Mesh-Hello: design

Firmware para o **Cardputer-Adv + Cap LoRa-1262 (oficial M5)** que entra na rede **Meshtastic** como um nó comum chamado **4stro-031**, mostra os nós ao redor e permite conversar com eles. Tem dois modos de saudação: manual, escolhendo o nó na lista, e automático, cumprimentando cada nó novo uma única vez.

## Intenção

Jean está explorando o Cardputer por diversão e aprendizado. Este é o segundo firmware próprio, depois do M5-Keyboard-Macro. A graça é "descobrir quem está por perto na rede LoRa e dizer oi" com um aparelho de bolso, sem celular e sem o firmware oficial do Meshtastic. Se funcionar bem, pode interessar à comunidade.

## Objetivos

1. Receber e decodificar pacotes Meshtastic do canal público padrão (LongFast, chave `AQ==`).
2. **Lista de nós**: nome longo e curto, ID `!xxxxxxxx`, modelo do hardware e família do chip quando conhecida, RSSI/SNR, quanto tempo desde que foi ouvido, se tem mensagem não lida e se já foi cumprimentado.
3. **Conversa por nó**: mensagens diretas (DM) enviadas e recebidas, agrupadas por nó, com texto livre digitado no teclado. A saudação vem pré-preenchida como sugestão.
4. **Canal público**: ver as mensagens do canal LongFast e poder enviar mensagens nele.
5. **Saudação automática**: quando ligada, manda uma DM de saudação para cada nó novo, **uma vez por nó, para sempre**. A lista de nós cumprimentados sobrevive a reinícios.
6. Anunciar-se na rede como **4stro-031** (NodeInfo com chave pública), para que os outros vejam o nó e consigam responder.

## Fora do escopo (v1)

- Repassar pacotes de outros nós (roteamento mesh). O nó se declara `CLIENT_MUTE`.
- GPS e posição: o GNSS do cap fica para a v2.
- Canais além do primário padrão, canais com AEAD, assinaturas XEdDSA, presets diferentes de LongFast, MQTT.
- Configurar a rede pelo app do celular (sem Bluetooth).

## Anti-objetivos (falhas mesmo que "funcione")

- **Virar spammer**: saudação repetida para o mesmo nó, ou rajadas que ocupam o canal.
- **Prejudicar a rede**: pacotes malformados, ou flood com hop limit alto.
- **Perder a identidade**: os nós Meshtastic guardam a primeira chave pública que ouvem de cada nó ("first key wins"). Se a nossa chave mudar, ninguém mais consegue mandar DM para o 4stro-031. A chave é gerada uma vez e nunca mais muda.

## Restrições e fatos verificados

### Hardware (Cap LoRa-1262 no Cardputer-Adv), fonte: docs.m5stack.com

| Sinal | GPIO |
|---|---|
| NSS | G5 |
| DIO1/IRQ | G4 |
| RST | G3 |
| BUSY | G6 |
| SCK / MOSI / MISO | G40 / G14 / G39 (**compartilhado com o microSD**, cujo CS é o G12) |
| Chave de antena (FM8625H) | **P0 do expansor PI4IOE5V6408**, I²C 0x43 em G8/G9 (barramento compartilhado com o teclado TCA8418 e o IMU). P0 precisa ir para **nível alto** |
| GNSS (v2) | ATGM336H, UART 115200, ESP RX=G15 / TX=G13 |

- O exemplo oficial usa `radio.begin(freq, bw, sf, cr, sync, power, preamble, 3.0, true)`, ou seja, TCXO de 3,0 V e regulador LDO, e também `setCurrentLimit(140)`.
- O exemplo oficial detecta o cap com `m5::PI4IOE5V6408_Class ioe(0x43, 400000, &m5::In_I2C)`.
- Faixa do cap: 868–923 MHz. Potência máxima: +22 dBm. Antena RP-SMA. **Nunca transmitir sem antena.**
- TX LoRa interfere no GPS, o que importa para a v2.

### Rádio Meshtastic, fonte: firmware/master e docs 2.8

- **LongFast**: BW 250 kHz, SF 11, CR 4/5, sync word **0x2B**, preâmbulo **16**, CRC ligado, cabeçalho explícito.
- Frequência: `freqStart + bw/2000 + (djb2("LongFast") % numCanais) * bw/1000`. Conferido por script: hash = 130429955.
  - **ANZ** (915–928): **919,875 MHz**. É o que a comunidade brasileira usa hoje (meshbrasil.com).
  - **BR_902** (902–907,5): **903,875 MHz**. É a região oficial do Brasil no Meshtastic.
  - Um rádio só escuta uma frequência por vez. **Padrão: ANZ**, porque é onde a comunidade está. BR_902 fica como opção nas configurações.
- Potência: as duas regiões permitem até 30 dBm, e o cap chega a 22 dBm. **Padrão: 22 dBm**, ajustável.

### Protocolo, fonte: firmware/master (RadioInterface.h, CryptoEngine, Router, Channels, NodeDB) e protobufs

- **Cabeçalho on-air**, 16 bytes, little-endian: `to(4) from(4) id(4) flags(1) channel(1) next_hop(1) relay_node(1)`.
  - Campos de `flags`: hop_limit `&0x07`, want_ack `0x08`, via_mqtt `0x10`, hop_start `(>>5)&7`.
  - O payload tem no máximo 239 bytes. Broadcast é `0xFFFFFFFF`.
- **Canal padrão**: chave AES-128 `d4f1bb3a20290759f0bcffabcf4e6901` e hash do canal **0x08** (conferido). Cifra **AES-CTR**: o nonce é `id(u64 LE) | from(u32 LE) | contador(4 bytes, começa em 0)`, e o payload cifrado é o protobuf `Data`.
- **DM de texto exige PKI.** No firmware atual, o remetente se recusa a mandar DM de texto pela chave do canal, e o receptor rejeita "legacy DM". Por isso implementamos:
  - X25519 (par de chaves nosso, gerado uma vez e guardado na NVS);
  - chave de sessão: `SHA-256(X25519(nossa_priv, pub_dele))`;
  - **AES-256-CCM** com tag de 8 bytes, L=2 e nonce de 13 bytes, montado assim: `id_low32(4 LE) | extraNonce(4 LE) | from(4 LE) | 0x00` (offset estranho, mas é o que o código faz e precisa ser espelhado exatamente);
  - formato no ar: `cifrado | tag(8) | extraNonce(4)`, com o byte `channel` do cabeçalho = **0x00**.
- Pacotes que vão **pela chave do canal** mesmo quando são direcionados a um nó: NODEINFO, POSITION, ROUTING (ACK) e TRACEROUTE.
- Para mandar DM para um nó, **precisamos da chave pública dele**, que vem no NodeInfo dele. Para ele nos responder, **ele precisa da nossa**, que vai no nosso NodeInfo.
- **Número do nó**: últimos 4 bytes do MAC, como o firmware oficial faz. O receptor não confere esse número contra a chave.
- Assinaturas XEdDSA: a política padrão **aceita pacotes sem assinatura**. Não assinamos na v1.
- Protobuf (subconjunto escrito à mão, sem nanopb):
  - `Data{portnum=1, payload=2, want_response=3, dest=4, source=5, request_id=6, reply_id=7, emoji=8, bitfield=9}`;
  - `User{id=1, long_name=2 (≤24 B), short_name=3, hw_model=5, is_licensed=6, role=7, public_key=8}`;
  - `Routing{error_reason=3}`;
  - portas: TEXT=1, POSITION=3, NODEINFO=4, ROUTING=5, TELEMETRY=67, TRACEROUTE=70.
- Nosso `hw_model` = **112 (M5STACK_CARDPUTER_ADV)**. Nosso role = `CLIENT_MUTE (1)`, porque não repassamos pacotes.
- ID de pacote: `(random << 10) | (contador & 0x3FF)`. ACK: responder um `Routing{error_reason=NONE}` com `request_id` igual ao id recebido.

## Abordagens consideradas

- **Escolhida: firmware próprio com um "Meshtastic mínimo".** RadioLib para o SX1262, o protocolo e a criptografia implementados à mão, e a interface nossa. Cabe no espaço livre (1,69 MB).
- **Adaptar o firmware oficial do Meshtastic:** descartado. Passa de 2 MB e não cabe, e mexer na interface dele é muito mais difícil.
- **Meshtastic oficial com o app do celular:** descartado. Não é um firmware próprio e não tem a saudação automática.

## Decisões do Jean

- Nome do nó na rede: **4stro-031**.
- Lista de nós com escolha manual de quem cumprimentar, **e** um modo automático.
- No automático, a saudação vai para vários nós, **mas nunca duas vezes para o mesmo**.
- Mensagens de resposta aparecem numa conversa com aquele nó.

## Minhas escolhas (podem ser corrigidas)

- **Nome do firmware e da pasta:** `M5-Mesh-Hello` / `firmwares/m5-mesh-hello`.
- **Nome curto do nó:** `4S31`.
- **Texto da saudação:** "Saudações de 4stro-031! 👋 (Cardputer-Adv)". Editável nas configurações.
- **Saudação automática:**
  - começa **desligada** a cada boot;
  - cumprimenta só nós cujo NodeInfo com chave já foi recebido;
  - no máximo 1 saudação a cada 30 s (cada pacote LongFast ocupa o ar por cerca de 1 s);
  - fila máxima de 10 saudações;
  - a lista de cumprimentados fica na NVS, até 500 IDs.
- **Hop limit 3** (o padrão da rede) para DMs e mensagens no canal, `want_ack` ligado em DMs. Uma DM chega a ✓ quando o ACK volta.
- **NodeInfo próprio:**
  - enviado no boot com `want_response`, para os vizinhos mandarem os deles;
  - repetido a cada 3 h;
  - também enviado **direto** para um nó quando abrimos conversa com ele sem ter a chave dele (com `want_response`). Nesse caso, a mensagem fica na fila como "aguardando chave".
- **Banco de nós:**
  - até 100 nós na RAM, removendo o mais antigo quando lota;
  - as chaves públicas e nomes dos nós com quem conversamos ficam guardados na NVS (até 50), para as respostas funcionarem depois de reiniciar;
  - um nó só aceita a primeira chave pública recebida (first key wins, igual ao oficial).
- **Duplicados:** cache dos últimos 64 pares `(from, id)`.
- **Telas** (Tab troca de tela):
  1. **Nós**: lista navegável com `;`/`.`, Enter abre a conversa, `A` liga e desliga o automático;
  2. **Conversa**: histórico com a caixa de texto embaixo, Enter envia, Esc volta;
  3. **Canal público**;
  4. **Config**: região, potência, texto da saudação, mostrar nossa ID e chave.
  - A barra de status mostra a região/frequência, se o automático está ON/OFF, quantos pacotes foram recebidos e o RSSI do último.
- **Mensagens**: até 30 por conversa, só na RAM na v1.
- **Partições**: app ≤ 1,5 MB e sem uso de SPIFFS. Tudo o que precisa persistir fica na NVS.

## Riscos a validar cedo

1. **Bit-exatidão da criptografia:** a ordem do contador CTR e o nonce do CCM. Vetores de referência gerados no PC com Python (`cryptography`) e testados no aparelho.
2. **Inicialização do rádio no ADV:** o expansor I²C é compartilhado com o teclado, o TCXO é de 3,0 V e a chave de antena fica no P0. O primeiro marco é só receber e mostrar pacotes crus.
3. **Ter alguém no alcance:** sem nós Meshtastic por perto, nada aparece. Um modo de diagnóstico mostra até pacotes que não conseguimos decodificar (RSSI, hash do canal).
4. **Antena de 868 MHz em 919,875 MHz:** funciona, mas perde um pouco. Ver a seção de antena.

## Antena (análise para o melhor sinal)

- A antena do kit é sintonizada para 868 MHz. 919,875 MHz (ANZ) fica **~6% acima** e 903,875 MHz (BR_902) **~4% acima**. Fora da faixa sintonizada, o VSWR sobe. Com VSWR 2:1, cerca de 11% da potência volta, o que dá **~0,5 dB** de perda. Com VSWR 3:1, **~1,25 dB**. Não há medição pública dessa antena específica em 915 MHz, mas o esperado é perder **de ~0,5 a 2 dB**, o equivalente a uns 5–20% do alcance. **Funciona para longa distância**, só não é o ideal.
- **O que mais importa para o alcance, em ordem:**
  1. **Altura e visada.** Levantar o aparelho alguns metros, ou ir para uma janela ou laje, rende muito mais que qualquer antena.
  2. **Uma antena sintonizada em 915 MHz** (902–928). É barata e recupera esses dB. Se for comprar, escolher 915 MHz e não "wideband".
  3. Antena **na vertical**, a mesma polarização dos outros nós.
  4. **Potência no máximo** (22 dBm). As regiões permitem.
- Se for usar a antena de 868, **BR_902 (903,875 MHz) fica mais perto da sintonia dela**, mas a comunidade está em ANZ. Por isso a região é configurável.

## Testes

- Testes Unity no aparelho, como no M5-Keyboard-Macro:
  - cabeçalho (pack/unpack);
  - protobuf (encode/decode de Data, User, Routing);
  - djb2 e frequência, hash do canal;
  - AES-CTR, X25519, SHA-256 e AES-CCM contra vetores gerados no PC;
  - montagem completa de DM e NodeInfo;
  - regras da saudação (uma vez por nó, intervalo, fila).
- Teste de campo com Jean: ver nós reais, mandar DM para um nó com o app oficial e receber resposta.

## Para o builder decidir

- Biblioteca de cripto: rweather `Crypto` (a mesma do Meshtastic) ou mbedTLS do ESP-IDF. Precisa de X25519, SHA-256, AES-CTR e AES-CCM.
- Layout exato das telas, dentro das descritas acima e respeitando 240×135 (cerca de 19 colunas no tamanho 2, 40 no tamanho 1).
- Organização dos módulos (rádio, protocolo, cripto, banco de nós, saudação, UI), mantendo a lógica testável sem hardware, como no firmware anterior.
