> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/protocol.md).

# Referencia del protocolo

Esta página describe exactamente lo que M5-Mesh-Hello v0.1.0 transmite y lo que acepta. Es una referencia para implementadores. Todo lo que aparece aquí se deriva del código de [`src/mesh/`](../../src/mesh/) y se ha comprobado con vectores de referencia generados por [`tools/gen_vectors.py`](../../tools/gen_vectors.py) (Python `cryptography`).

El firmware implementa un subconjunto pequeño e interoperable del protocolo [Meshtastic](https://meshtastic.org). Fuentes oficiales que replica:

- Radio, regiones y frecuencia: [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp), [`RadioInterface.h`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.h)
- Cifrado: [`src/mesh/CryptoEngine.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/CryptoEngine.cpp), [`src/mesh/aes-ccm.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/aes-ccm.cpp)
- Enrutamiento, ACK y decisiones de PKI: [`src/mesh/Router.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Router.cpp), [`src/mesh/ReliableRouter.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/ReliableRouter.cpp)
- Canales y hash de canal: [`src/mesh/Channels.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/Channels.cpp)
- Base de datos de nodos y gestión de claves: [`src/mesh/NodeDB.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/NodeDB.cpp)
- Protobufs: [`meshtastic/mesh.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/mesh.proto), [`meshtastic/portnums.proto`](https://github.com/meshtastic/protobufs/blob/master/meshtastic/portnums.proto)

## Capa física

LoRa, Meshtastic **LongFast**: BW 250 kHz, SF 11, CR 4/5, sync word `0x2B`, preámbulo de 16 símbolos, encabezado explícito, CRC del payload activado. La frecuencia central se calcula a partir de la región. Consulta [frequencies.md](frequencies.md#cómo-se-calcula-la-frecuencia). Una trama LoRa tiene como máximo 255 bytes: un encabezado de 16 bytes más hasta 239 bytes de payload cifrado.

## Encabezado en el aire (16 bytes, little-endian)

| Offset | Tamaño | Campo | Notas |
|---|---|---|---|
| 0 | 4 | `to` | Número de nodo de destino; `0xFFFFFFFF` = difusión (broadcast) |
| 4 | 4 | `from` | Número de nodo del remitente |
| 8 | 4 | `id` | ID del paquete |
| 12 | 1 | `flags` | Ver más abajo |
| 13 | 1 | `channel` | Hash del canal (`0x08` para el canal por defecto), o `0x00` para PKI |
| 14 | 1 | `next_hop` | Enviamos 0 |
| 15 | 1 | `relay_node` | Enviamos el byte bajo de nuestro número de nodo |

`flags`:

| Bits | Máscara | Campo | Enviamos |
|---|---|---|---|
| 0–2 | `0x07` | `hop_limit` | 3 |
| 3 | `0x08` | `want_ack` | 1 para textos directos PKI, 0 en los demás casos |
| 4 | `0x10` | `via_mqtt` | 0 |
| 5–7 | `0xE0` (`>> 5`) | `hop_start` | 3 |

Código: [`header.cpp`](../../src/mesh/header.cpp), [`packet.cpp`](../../src/mesh/packet.cpp) `fillHeader`.

## Cifrado del canal (AES-128-CTR)

Todas las tramas, salvo los mensajes directos PKI, se cifran con la clave por defecto del canal:

- **Clave** (AES-128; es en lo que se expande la abreviatura de PSK `AQ==`, índice 1):
  `d4 f1 bb 3a 20 29 07 59 f0 bc ff ab cf 4e 69 01`
- **Nombre del canal**: `LongFast`.
- **Hash del canal** (byte 13 del encabezado): XOR de todos los bytes del nombre, combinado mediante XOR con todos los bytes de la clave = **`0x08`**.
- **Bloque contador** (16 bytes):

  | Bytes | Contenido |
  |---|---|
  | 0–7 | `id` del paquete como u64 little-endian (los bytes 4–7 son cero) |
  | 8–11 | `from` como u32 little-endian |
  | 12–15 | contador de bloque, 32 bits **big-endian**, empezando en 0 |

- El texto en claro es un protobuf `Data` codificado. Cifrar y descifrar son la misma operación XOR.

Código: [`crypto.cpp`](../../src/mesh/crypto.cpp) `ctrCrypt`, [`region.cpp`](../../src/mesh/region.cpp) `kDefaultKey` / `channelHash`.

## Mensajes directos PKI (X25519 + SHA-256 + AES-256-CCM)

El firmware actual de Meshtastic envía los DM de texto solo con cifrado de clave pública (PKI). M5-Mesh-Hello hace lo mismo.

1. **Par de claves**: X25519. La clave privada son 32 bytes aleatorios del RNG de hardware, con clamping según la RFC 7748 (`k[0] &= 248; k[31] &= 127; k[31] |= 64`). Clave pública = X25519(priv, 9). Se genera una sola vez y se guarda en la NVS.
2. **Clave compartida**: `key = SHA-256(X25519(our_priv, peer_pub))`, 32 bytes. Se rechazan las claves de pares que producen un secreto compartido todo a cero o no válido por otro motivo (puntos débiles).
3. **Cifrado**: AES-256-CCM (RFC 3610), **longitud de etiqueta M = 8**, campo de longitud L = 2, **nonce de 13 bytes**, sin datos asociados.
4. **Nonce** (13 bytes):

   | Bytes | Contenido |
   |---|---|
   | 0–3 | `id` del paquete, u32 little-endian |
   | 4–7 | `extraNonce`, u32 little-endian |
   | 8–11 | `from`, u32 little-endian |
   | 12 | `0x00` |

   Esta disposición parece extraña, pero es lo que hace el firmware oficial. Meshtastic construye un búfer de 16 bytes como `id (u64 LE) | from (u32 LE) | 0` y luego copia `extraNonce` sobre los bytes 4–7, que son la mitad alta del id u64 y siempre valen cero. CCM usa los 13 primeros bytes. Una implementación debe replicar esto exactamente.
5. **Formato en el aire** del payload después del encabezado de 16 bytes: `ciphertext | tag (8 bytes) | extraNonce (4 bytes, LE)`. La sobrecarga es de 12 bytes, así que el `Data` en claro más grande tiene 239 − 12 = 227 bytes.
6. **Encabezado**: `channel = 0x00`, `to` = el número de nodo del destinatario, `want_ack = 1`.
7. `extraNonce` es un valor aleatorio de 32 bits nuevo para cada mensaje.

En recepción, una trama se trata como PKI cuando `channel == 0x00` **y** `to` es nuestro número de nodo. Se necesita la clave pública guardada del remitente. Si se desconoce, la trama no se puede descifrar (consulta [Gestión de claves desconocidas](#gestión-de-claves-desconocidas)). Las etiquetas se comparan en tiempo constante y, si falla, el búfer de texto en claro se borra y la trama se descarta.

Código: [`crypto.cpp`](../../src/mesh/crypto.cpp) `x25519Shared` / `pkiEncrypt` / `pkiDecrypt`, [`aes_ccm.cpp`](../../src/mesh/aes_ccm.cpp).

## Qué clave se usa para qué

| Trama | Cifrado | Byte `channel` |
|---|---|---|
| Texto al canal público (difusión) | Clave del canal (CTR) | `0x08` |
| Texto a un nodo (DM) | **PKI** (CCM) | `0x00` |
| NodeInfo (difusión o dirigido) | Clave del canal | `0x08` |
| ACK / NAK de enrutamiento | Clave del canal | `0x08` |

En recepción:

- Las tramas con hash de canal `0x08` se descifran con la clave por defecto. Cualquier otro hash de canal, salvo `0x00`, se ignora, aunque sigue contando en `rx`.
- **Se rechazan los DM heredados**: una trama `TEXT_MESSAGE_APP` cifrada con la clave del canal y dirigida a un solo nodo (no difusión) se descarta, como en el firmware oficial. Los DM de texto deben ser PKI.
- Se ignoran las tramas cuyo `from` es igual a nuestro propio número de nodo o a `0xFFFFFFFF`.

## Subconjunto de protobuf

Codificador/decodificador escrito a mano ([`pb.cpp`](../../src/mesh/pb.cpp)); los campos desconocidos se omiten; las entradas mal formadas o truncadas se rechazan.

**`Data`** (el payload descifrado):

| Campo | N.º | Tipo | Uso |
|---|---|---|---|
| `portnum` | 1 | varint | `1` TEXT_MESSAGE_APP, `4` NODEINFO_APP, `5` ROUTING_APP; los demás solo se reconocen como actividad |
| `payload` | 2 | bytes | Se aceptan ≤ 233 bytes |
| `want_response` | 3 | varint (bool) | Activado en nuestro NodeInfo de arranque y en las solicitudes de NodeInfo |
| `dest` | 4 | fixed32 | Se decodifica, no se usa |
| `source` | 5 | fixed32 | Se decodifica, no se usa |
| `request_id` | 6 | fixed32 | En ACK/NAK: el ID del paquete que se confirma |
| `reply_id` | 7 | fixed32 | Se decodifica, no se usa |

**`User`** (payload de NODEINFO_APP):

| Campo | N.º | Tipo | Enviamos |
|---|---|---|---|
| `id` | 1 | string | `!%08x` de nuestro número de nodo |
| `long_name` | 2 | string | Nuestro nombre largo, recortado a 24 bytes en un límite UTF-8 |
| `short_name` | 3 | string | Nuestro nombre corto (≤ 4 bytes) |
| `hw_model` | 5 | varint | `112` (M5STACK_CARDPUTER_ADV) |
| `role` | 7 | varint | `1` (CLIENT_MUTE) |
| `public_key` | 8 | bytes | Nuestra clave pública X25519 de 32 bytes |

Un `User` recibido cuyo `public_key` está presente pero no tiene exactamente 32 bytes se rechaza por completo. Los campos 4 (`macaddr`, obsoleto) y 6 (`is_licensed`) se omiten.

**`Routing`** (payload de ROUTING_APP):

| Campo | N.º | Tipo | Uso |
|---|---|---|---|
| `error_reason` | 3 | varint (enum) | `0` NONE = ACK; cualquier otro = NAK. Enviamos `35` PKI_UNKNOWN_PUBKEY |

## ACK y NAK

- **Envío de un ACK**: para cada DM PKI dirigido a nosotros con `want_ack`, enviamos al remitente una trama `ROUTING_APP` con `request_id` = el ID de paquete del DM y `Routing{error_reason = NONE}` codificado explícitamente como los bytes `18 00`. Va cifrada con la clave del canal, límite de saltos 3, sin `want_ack`. Si el mismo DM llega otra vez (una retransmisión porque nuestro ACK se perdió), lo volvemos a confirmar. Para ello se recuerdan los últimos 8 pares `(from, id)` confirmados.
- **Recepción**: una trama ROUTING_APP dirigida a nosotros con un `request_id` distinto de cero es un ACK si `error_reason` no está presente o vale 0. Proto3 omite los valores cero, así que un mensaje `Routing` vacío también significa NONE. El mensaje correspondiente pasa de enviado (`.`) a confirmado (`v`). Un `error_reason` distinto de cero es un NAK: el mensaje se marca como fallido (`x`).
- Nunca retransmitimos un DM automáticamente.

## Gestión de claves desconocidas

- **Envío a un nodo sin clave**: el DM queda retenido (estado `?`) y enviamos nuestro NodeInfo **directamente** a ese nodo con `want_response = 1` (como máximo una vez cada 60 s por nodo). Cuando llega su NodeInfo con clave, se liberan los DM retenidos.
- **Recepción de un DM PKI de un nodo sin clave**: enviamos un NAK `Routing{error_reason = 35 (PKI_UNKNOWN_PUBKEY)}` con `request_id` = su ID de paquete, más nuestro NodeInfo con `want_response = 1`. Límites: una vez cada 10 minutos por nodo y una vez cada 30 s en total. Las tramas que no se pueden decodificar **no** se añaden a la caché de duplicados, para que el reintento del remitente se pueda descifrar una vez llegada la clave.

## NodeInfo

- Difundimos nuestro NodeInfo al arrancar (con `want_response`), cada 3 horas, después de un cambio de nombre y a petición desde la pantalla Config. Las difusiones manuales y por cambio de nombre se separan al menos 30 s.
- Respondemos a un NodeInfo recibido que tenga `want_response` (difundido o dirigido a nosotros) con nuestro NodeInfo enviado directamente a ese nodo, sin `want_response`. Límites: una vez cada 12 h por nodo y una vez cada 30 s en total, para que remitentes falsificados no puedan mantenernos en el aire.

## Gana la primera clave

La primera clave pública de 32 bytes recibida para un número de nodo se guarda y nunca se sustituye, como en la NodeDB de Meshtastic. Un `User` posterior para ese nodo solo se aplica si trae la **misma** clave. Si la clave es distinta o falta, se ignora toda la actualización, incluidos los nombres. Las claves de los contactos de DM se guardan de forma persistente (hasta 50) para que la regla sobreviva a los reinicios. Nuestra propia clave se genera una vez y nunca cambia por el mismo motivo.

El receptor no comprueba el número de nodo frente a su clave. Antes de que se conozca la clave de un nodo, cualquiera puede enviar un NodeInfo que diga tener ese número de nodo con su propia clave, y esa clave ganará. Es una propiedad del protocolo oficial; consulta [security.md](security.md).

## Número de nodo

Nuestro número de nodo son los **últimos 4 bytes de la dirección MAC de fábrica**, big-endian (`mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5]`), como hace el firmware oficial. Si no se puede leer la MAC o da un valor reservado (`< 4` o `0xFFFFFFFF`), se usa en su lugar un hash FNV-1a de la clave pública. La cadena del ID de nodo es `!` seguido del número en 8 dígitos hexadecimales en minúscula.

## ID de paquete

`id = (rnd << 10) | (counter & 0x3FF)`, nunca 0. `rnd` es un valor de 32 bits de un generador xorshift64* sembrado con el RNG de hardware en cada arranque, así que los ID (y por tanto los bloques contadores de CTR y los nonces de CCM) no se repiten entre reinicios. `counter` se incrementa con cada ID. El firmware oficial usa el mismo esquema.

## Supresión de duplicados

Un anillo con los últimos **64** pares `(from, id)`. Una trama cuyo par está en la caché se ignora, salvo que un DM duplicado que ya confirmamos se vuelve a confirmar. Un par solo se registra después de procesar la trama con éxito. Las tramas que no se pudieron descifrar o decodificar no se registran.

## Límite de saltos y retransmisión

Toda trama que originamos tiene `hop_limit = hop_start = 3`, el valor por defecto de Meshtastic. **Nunca retransmitimos** tramas de otros nodos: nuestro NodeInfo declara el rol `CLIENT_MUTE` y el firmware no tiene ninguna ruta de redifusión.

## No implementado en v1

Firmas de paquetes XEdDSA (la política por defecto del firmware oficial acepta paquetes sin firmar), canales distintos del primario por defecto, PSK personalizadas y canales AES-256-CTR, presets distintos de LongFast, posición, telemetría, traceroute, store-and-forward, MQTT. Consulta [roadmap.md](roadmap.md).
