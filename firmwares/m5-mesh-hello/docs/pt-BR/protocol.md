> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/protocol.md).

# Referência do protocolo

Esta página descreve exatamente o que o M5-Mesh-Hello v0.1.0 coloca no ar e o que ele aceita. É uma referência para implementadores. Tudo aqui deriva do código em [`src/mesh/`](../../src/mesh/) e foi conferido contra vetores de referência gerados por [`tools/gen_vectors.py`](../../tools/gen_vectors.py) (Python `cryptography`).

O firmware implementa um subconjunto pequeno e interoperável do protocolo [Meshtastic](https://meshtastic.org). Fontes oficiais que ele espelha:

- Rádio, regiões e frequência: [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp), [`RadioInterface.h`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.h)
- Criptografia: [`src/mesh/CryptoEngine.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/CryptoEngine.cpp), [`src/mesh/aes-ccm.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/aes-ccm.cpp)
- Roteamento, ACKs e decisões de PKI: [`src/mesh/Router.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Router.cpp), [`src/mesh/ReliableRouter.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/ReliableRouter.cpp)
- Canais e hash de canal: [`src/mesh/Channels.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Channels.cpp)
- Banco de dados de nós e tratamento de chaves: [`src/mesh/NodeDB.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/NodeDB.cpp)
- Protobufs: [`meshtastic/mesh.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto), [`meshtastic/portnums.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/portnums.proto)

## Camada física

LoRa, Meshtastic **LongFast**: BW 250 kHz, SF 11, CR 4/5, sync word `0x2B`, preâmbulo de 16 símbolos, cabeçalho explícito, CRC do payload ligado. A frequência central é calculada a partir da região. Veja [frequencies.md](frequencies.md#como-a-frequência-é-calculada). Um quadro LoRa tem no máximo 255 bytes: um cabeçalho de 16 bytes mais até 239 bytes de payload criptografado.

## Cabeçalho no ar (16 bytes, little-endian)

| Offset | Tamanho | Campo | Observações |
|---|---|---|---|
| 0 | 4 | `to` | Número do nó de destino; `0xFFFFFFFF` = broadcast |
| 4 | 4 | `from` | Número do nó remetente |
| 8 | 4 | `id` | ID do pacote |
| 12 | 1 | `flags` | Veja abaixo |
| 13 | 1 | `channel` | Hash do canal (`0x08` para o canal padrão), ou `0x00` para PKI |
| 14 | 1 | `next_hop` | Enviamos 0 |
| 15 | 1 | `relay_node` | Enviamos o byte menos significativo do nosso número de nó |

`flags`:

| Bits | Máscara | Campo | Enviamos |
|---|---|---|---|
| 0–2 | `0x07` | `hop_limit` | 3 |
| 3 | `0x08` | `want_ack` | 1 para textos diretos PKI, 0 nos demais |
| 4 | `0x10` | `via_mqtt` | 0 |
| 5–7 | `0xE0` (`>> 5`) | `hop_start` | 3 |

Código: [`header.cpp`](../../src/mesh/header.cpp), [`packet.cpp`](../../src/mesh/packet.cpp) `fillHeader`.

## Criptografia do canal (AES-128-CTR)

Todos os quadros, exceto as mensagens diretas PKI, são criptografados com a chave padrão do canal:

- **Chave** (AES-128; é nisso que se expande a abreviação de PSK `AQ==`, índice 1):
  `d4 f1 bb 3a 20 29 07 59 f0 bc ff ab cf 4e 69 01`
- **Nome do canal**: `LongFast`.
- **Hash do canal** (byte 13 do cabeçalho): XOR de todos os bytes do nome, combinado por XOR com todos os bytes da chave = **`0x08`**.
- **Bloco contador** (16 bytes):

  | Bytes | Conteúdo |
  |---|---|
  | 0–7 | `id` do pacote como u64 little-endian (os bytes 4–7 são zero) |
  | 8–11 | `from` como u32 little-endian |
  | 12–15 | contador de bloco, 32 bits **big-endian**, começando em 0 |

- O texto claro é um protobuf `Data` codificado. Criptografar e decifrar são a mesma operação de XOR.

Código: [`crypto.cpp`](../../src/mesh/crypto.cpp) `ctrCrypt`, [`region.cpp`](../../src/mesh/region.cpp) `kDefaultKey` / `channelHash`.

## Mensagens diretas PKI (X25519 + SHA-256 + AES-256-CCM)

O firmware Meshtastic atual envia DMs de texto apenas com criptografia de chave pública (PKI). O M5-Mesh-Hello faz o mesmo.

1. **Par de chaves**: X25519. A chave privada são 32 bytes aleatórios do RNG de hardware, com clamping conforme a RFC 7748 (`k[0] &= 248; k[31] &= 127; k[31] |= 64`). Chave pública = X25519(priv, 9). É gerada uma única vez e guardada na NVS.
2. **Chave compartilhada**: `key = SHA-256(X25519(our_priv, peer_pub))`, 32 bytes. Chaves de pares que produzem um segredo compartilhado todo zero ou inválido de outra forma (pontos fracos) são rejeitadas.
3. **Cifra**: AES-256-CCM (RFC 3610), **tamanho da tag M = 8**, campo de tamanho L = 2, **nonce de 13 bytes**, sem dados associados.
4. **Nonce** (13 bytes):

   | Bytes | Conteúdo |
   |---|---|
   | 0–3 | `id` do pacote, u32 little-endian |
   | 4–7 | `extraNonce`, u32 little-endian |
   | 8–11 | `from`, u32 little-endian |
   | 12 | `0x00` |

   Esse layout parece estranho, mas é o que o firmware oficial faz. O Meshtastic monta um buffer de 16 bytes como `id (u64 LE) | from (u32 LE) | 0` e depois copia `extraNonce` por cima dos bytes 4–7, que são a metade alta do id u64 e são sempre zero. O CCM usa os 13 primeiros bytes. Uma implementação precisa espelhar isso exatamente.
5. **Formato no ar** do payload após o cabeçalho de 16 bytes: `ciphertext | tag (8 bytes) | extraNonce (4 bytes, LE)`. O overhead é de 12 bytes, então o maior `Data` em texto claro tem 239 − 12 = 227 bytes.
6. **Cabeçalho**: `channel = 0x00`, `to` = o número do nó destinatário, `want_ack = 1`.
7. `extraNonce` é um valor aleatório de 32 bits novo a cada mensagem.

Na recepção, um quadro é tratado como PKI quando `channel == 0x00` **e** `to` é o nosso número de nó. É preciso ter a chave pública do remetente guardada. Se ela for desconhecida, o quadro não pode ser decifrado (veja [Tratamento de chave desconhecida](#tratamento-de-chave-desconhecida)). As tags são comparadas em tempo constante e, em caso de falha, o buffer de texto claro é apagado e o quadro é descartado.

Código: [`crypto.cpp`](../../src/mesh/crypto.cpp) `x25519Shared` / `pkiEncrypt` / `pkiDecrypt`, [`aes_ccm.cpp`](../../src/mesh/aes_ccm.cpp).

## Qual chave é usada para quê

| Quadro | Criptografia | Byte `channel` |
|---|---|---|
| Texto para o canal público (broadcast) | Chave do canal (CTR) | `0x08` |
| Texto para um nó (DM) | **PKI** (CCM) | `0x00` |
| NodeInfo (broadcast ou direcionado) | Chave do canal | `0x08` |
| ACK / NAK de roteamento | Chave do canal | `0x08` |

Na recepção:

- Quadros com hash de canal `0x08` são decifrados com a chave padrão. Qualquer outro hash de canal, exceto `0x00`, é ignorado, mas ainda conta em `rx`.
- **DMs legadas são rejeitadas**: um quadro `TEXT_MESSAGE_APP` criptografado com a chave do canal e endereçado a um único nó (não broadcast) é descartado, como no firmware oficial. DMs de texto precisam ser PKI.
- Quadros com `from` igual ao nosso próprio número de nó ou a `0xFFFFFFFF` são ignorados.

## Subconjunto de protobuf

Codificador/decodificador escrito à mão ([`pb.cpp`](../../src/mesh/pb.cpp)); campos desconhecidos são pulados; entrada malformada ou truncada é rejeitada.

**`Data`** (o payload decifrado):

| Campo | Nº | Tipo | Uso |
|---|---|---|---|
| `portnum` | 1 | varint | `1` TEXT_MESSAGE_APP, `4` NODEINFO_APP, `5` ROUTING_APP; os demais são reconhecidos apenas como atividade |
| `payload` | 2 | bytes | ≤ 233 bytes aceitos |
| `want_response` | 3 | varint (bool) | Ligado no nosso NodeInfo da inicialização e nos pedidos de NodeInfo |
| `dest` | 4 | fixed32 | Decodificado, não usado |
| `source` | 5 | fixed32 | Decodificado, não usado |
| `request_id` | 6 | fixed32 | Em ACK/NAK: o ID do pacote que está sendo confirmado |
| `reply_id` | 7 | fixed32 | Decodificado, não usado |

**`User`** (payload de NODEINFO_APP):

| Campo | Nº | Tipo | Enviamos |
|---|---|---|---|
| `id` | 1 | string | `!%08x` do nosso número de nó |
| `long_name` | 2 | string | O nosso nome longo, cortado em 24 bytes numa fronteira UTF-8 |
| `short_name` | 3 | string | O nosso nome curto (≤ 4 bytes) |
| `hw_model` | 5 | varint | `112` (M5STACK_CARDPUTER_ADV) |
| `role` | 7 | varint | `1` (CLIENT_MUTE) |
| `public_key` | 8 | bytes | A nossa chave pública X25519 de 32 bytes |

Um `User` recebido cujo `public_key` esteja presente mas não tenha exatamente 32 bytes é rejeitado por inteiro. Os campos 4 (`macaddr`, obsoleto) e 6 (`is_licensed`) são pulados.

**`Routing`** (payload de ROUTING_APP):

| Campo | Nº | Tipo | Uso |
|---|---|---|---|
| `error_reason` | 3 | varint (enum) | `0` NONE = ACK; qualquer outro = NAK. Enviamos `35` PKI_UNKNOWN_PUBKEY |

## ACK e NAK

- **Envio de ACK**: para toda DM PKI endereçada a nós com `want_ack`, enviamos ao remetente um quadro `ROUTING_APP` com `request_id` = o ID do pacote da DM e `Routing{error_reason = NONE}` codificado explicitamente como os bytes `18 00`. Ele é criptografado com a chave do canal, limite de saltos 3, sem `want_ack`. Se a mesma DM chegar de novo (uma retransmissão porque o nosso ACK se perdeu), confirmamos de novo. Para isso, os últimos 8 pares `(from, id)` confirmados ficam guardados.
- **Recepção**: um quadro ROUTING_APP endereçado a nós com `request_id` diferente de zero é um ACK se `error_reason` estiver ausente ou for 0. O proto3 omite valores zero, então uma mensagem `Routing` vazia também significa NONE. A mensagem correspondente muda de enviada (`.`) para confirmada (`v`). Um `error_reason` diferente de zero é um NAK: a mensagem é marcada como falha (`x`).
- Nunca retransmitimos uma DM automaticamente.

## Tratamento de chave desconhecida

- **Envio para um nó sem chave**: a DM fica retida (estado `?`) e enviamos o nosso NodeInfo **diretamente** para aquele nó com `want_response = 1` (no máximo uma vez a cada 60 s por nó). Quando o NodeInfo dele com a chave chega, as DMs retidas são liberadas. O pedido é repetido no máximo uma vez a cada 60 s enquanto houver DMs retidas; uma DM retida falha (`x`) depois de 10 minutos, ou antes, se a fila de 10 DMs estiver cheia e uma DM para um nó com chave precisar da vaga.
- **Recepção de uma DM PKI de um nó sem chave**: enviamos um NAK `Routing{error_reason = 35 (PKI_UNKNOWN_PUBKEY)}` com `request_id` = o ID do pacote dele, mais o nosso NodeInfo com `want_response = 1`. Limites: uma vez a cada 10 minutos por nó e uma vez a cada 30 s no total. Quadros não decodificáveis **não** entram no cache de duplicatas, para que a nova tentativa do remetente possa ser decifrada quando a chave tiver chegado.

## NodeInfo

- Difundimos o nosso NodeInfo na inicialização (com `want_response`), a cada 3 horas, depois de uma mudança de nome e a pedido na tela Config. As difusões manuais e por renomeação têm intervalo mínimo de 30 s.
- Respondemos a um NodeInfo recebido que tenha `want_response` (broadcast ou endereçado a nós) com o nosso NodeInfo enviado diretamente para aquele nó, sem `want_response`. Limites: uma vez a cada 12 h por nó e uma vez a cada 30 s no total, para que remetentes falsificados não consigam nos manter no ar.

## A primeira chave vence

A primeira chave pública de 32 bytes recebida para um número de nó é guardada e nunca substituída, como no NodeDB do Meshtastic. Um `User` posterior para aquele nó só é aplicado se trouxer a **mesma** chave. Se a chave for diferente ou estiver ausente, a atualização inteira é ignorada, inclusive os nomes. As chaves dos contatos de DM são persistidas (até 50) para que a regra sobreviva a reinicializações. A nossa própria chave é gerada uma vez e nunca muda, pelo mesmo motivo.

O receptor não confere um número de nó contra a chave dele. Antes de a chave de um nó ser conhecida, qualquer um pode enviar um NodeInfo alegando aquele número de nó com a própria chave, e essa chave vai vencer. Isso é uma propriedade do protocolo oficial; veja [security.md](security.md).

## Número do nó

O nosso número de nó são os **últimos 4 bytes do endereço MAC de fábrica**, big-endian (`mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5]`), como faz o firmware oficial. Se o MAC não puder ser lido ou der um valor reservado (`< 4` ou `0xFFFFFFFF`), é usado no lugar um hash FNV-1a da chave pública. A string do ID do nó é `!` seguido do número em 8 dígitos hexadecimais minúsculos.

## IDs de pacote

`id = (rnd << 10) | (counter & 0x3FF)`, nunca 0. `rnd` é um valor de 32 bits de um gerador xorshift64* semeado a partir do RNG de hardware a cada inicialização, então os IDs (e, portanto, os blocos contadores do CTR e os nonces do CCM) não se repetem entre reinicializações. `counter` é incrementado a cada ID. O firmware oficial usa o mesmo esquema.

## Supressão de duplicatas

Um anel com os últimos **64** pares `(from, id)`. Um quadro cujo par esteja no cache é ignorado, exceto que uma DM duplicada que já confirmamos é confirmada de novo. Um par só é registrado depois que o quadro foi tratado com sucesso. Quadros que falharam ao decifrar ou decodificar não são registrados.

## Limite de saltos e retransmissão

Todo quadro que originamos tem `hop_limit = hop_start = 3`, o padrão do Meshtastic. **Nunca retransmitimos** quadros de outros nós: o nosso NodeInfo declara o papel `CLIENT_MUTE`, e o firmware não tem caminho de redifusão.

## Não implementado na v1

Assinaturas de pacote XEdDSA (a política padrão do firmware oficial aceita pacotes sem assinatura), canais além do primário padrão, PSKs personalizadas e canais AES-256-CTR, presets além do LongFast, posição, telemetria, traceroute, store-and-forward, MQTT. Veja [roadmap.md](roadmap.md).
