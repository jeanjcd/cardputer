> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/architecture.md).

# Arquitectura

Cómo está organizado el firmware, cómo lo recorre una trama, qué guarda y cuánta memoria usa. Describe la v0.1.0. Los diagramas siguientes son bloques de código y se mantienen en inglés, como el código fuente.

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

En resumen: `src/mesh/` es el núcleo portable del protocolo (sin cabeceras de Arduino/ESP-IDF, compilado y probado de forma nativa); `src/hw/` es el hardware (radio SX1262, almacenamiento NVS, identidad persistente); `src/app/` es la lógica de la aplicación (`App`) y la interfaz (`Ui`); `lib/crypto/` es un subconjunto incluido de la biblioteca rweather/Crypto (MIT); `test/` contiene las suites de pruebas nativas de Unity; `tools/` contiene los scripts auxiliares.

### `src/mesh/` (núcleo portable)

| Módulo | Responsabilidad |
|---|---|
| [`header`](../../src/mesh/header.h) | Lectura/escritura del encabezado de 16 bytes en el aire |
| [`pb`](../../src/mesh/pb.h) | Subconjunto de protobuf: `Data`, `User`, `Routing` |
| [`crypto`](../../src/mesh/crypto.h), [`aes_ccm`](../../src/mesh/aes_ccm.h) | AES-128-CTR, X25519 + SHA-256, AES-256-CCM; [`secure_zero.h`](../../src/mesh/secure_zero.h) borra los secretos |
| [`packet`](../../src/mesh/packet.h) | Construye las tramas salientes (texto, DM, NodeInfo, ACK/NAK), analiza y descifra las entrantes (`parseFrame`), ID de paquete, `DedupCache` |
| [`region`](../../src/mesh/region.h) | Tabla de regiones, fórmula de la frecuencia, límite de potencia, hash del canal, tiempo en el aire LoRa, `AirtimeBudget` |
| [`nodedb`](../../src/mesh/nodedb.h) | Nodos conocidos (RAM) y contactos persistentes (NVS), gana la primera clave |
| [`chat`](../../src/mesh/chat.h) | Historial de mensajes en un pool compartido |
| [`greeter`](../../src/mesh/greeter.h) | Cola del saludo automático, intervalo y conjunto persistente de saludados |
| [`outbox`](../../src/mesh/outbox.h) | DM salientes, incluidos los que esperan una clave |
| [`ratelimit`](../../src/mesh/ratelimit.h) | Limitadores por nodo y globales para las respuestas automáticas |
| [`settings`](../../src/mesh/settings.h) | Configuración del usuario, validación, codificación del blob, composición del saludo |
| [`text`](../../src/mesh/text.h) | Utilidades UTF-8 con límites, conversión a CP437 para la pantalla |
| [`hwmodel`](../../src/mesh/hwmodel.h) | ID de modelo de hardware → nombre y familia del chip |
| [`kv`](../../src/mesh/kv.h) | Almacén abstracto de blobs clave/valor (NVS en el dispositivo, un map en las pruebas) |

### `src/hw/` (hardware)

| Módulo | Responsabilidad |
|---|---|
| [`radio`](../../src/hw/radio.h) | Conmutador de antena PI4IOE, SX1262 mediante RadioLib, RX por interrupción, TX bloqueante, resintonización, potencia |
| [`nvs_kv`](../../src/hw/nvs_kv.h) | `Kv` sobre la NVS del ESP32 (Arduino `Preferences`, namespace `meshhello`), lecturas de longitud exacta |
| [`identity`](../../src/hw/identity.h) | Carga o crea el par de claves X25519, número de nodo a partir de la MAC, entropía de hardware |

### `src/app/` (aplicación)

| Módulo | Responsabilidad |
|---|---|
| [`app`](../../src/app/app.h) | Es dueño de todo el estado. Procesamiento de RX, planificación, cola de TX, presupuesto de ciclo de trabajo, acciones de configuración, avisos. |
| [`ui`](../../src/app/ui.h) | Cuatro pestañas en la pantalla de 240 × 135, gestión del teclado, lienzo fuera de pantalla |

La interfaz lee el estado solo a través de los accesores const de `App` y actúa mediante sus métodos de acción (`sendText`, `cycleRegion`, `toggleAutoGreet`, ...). `App` incrementa un contador de versión con cada cambio visible. La interfaz se redibuja cuando cambia (como mucho cada 50 ms), y al menos una vez por segundo para las antigüedades y los avisos.

## Flujo de datos

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

En palabras: una trama recibida pasa por `Radio::poll` (que actualiza `rx` y el RSSI), por el análisis del encabezado (descarta las tramas que vienen de nosotros mismos o de la dirección de difusión), por la caché de duplicados y por `parseFrame`, que descifra con PKI o CTR. Las tramas PKI sin clave que no se pueden decodificar generan un NAK 35 más un NodeInfo. Un NodeInfo actualiza la NodeDb (gana la primera clave), alimenta al Greeter y al Outbox y puede generar una respuesta con límite de frecuencia; los textos van al ChatStore (con ACK si se pide); ACK/NAK cambian el estado del mensaje a `v` o `x`. En la salida, `App::schedule` solo llena la cola de TX cuando está vacía, por orden de prioridad: difusión de NodeInfo pendiente, luego DM del Outbox, luego saludos. Los ACK y NAK se insertan al principio de la cola; las respuestas y solicitudes de NodeInfo y los textos públicos se añaden directamente al final.

Puntos clave:

- **Sin heap en la ruta de RX.** La estructura `Inbound` decodificada es un búfer estático, y todos los búferes tienen tamaño fijo con límites explícitos.
- **Deduplicación después del análisis.** Un par solo se registra una vez procesada la trama, así que un DM PKI que llega antes que la clave del remitente todavía se puede leer cuando se retransmite.
- **Una transmisión a la vez.** `schedule()` solo rellena la cola cuando está vacía. `transmitOne()` envía como máximo una trama por vuelta del bucle, al menos **1.5 s** después del final de la anterior, y solo si el presupuesto de ciclo de trabajo de la región lo permite. Si no, la trama en cabeza (y todo lo que hay detrás) espera, y la barra de estado muestra `duty limit`.
- **TX bloqueante.** `Radio::send` se bloquea hasta terminar la TX (hasta unos 2.2 s para 255 bytes), así que durante ese tiempo no se procesan las teclas.

## Persistencia (NVS)

Todo se guarda en la partición NVS del ESP32 bajo el namespace **`meshhello`**, como blobs de tamaño fijo. Una lectura solo tiene éxito si el blob guardado tiene exactamente la longitud esperada. Los bytes de versión y la validación de campos rechazan cualquier otra cosa, y se usan los valores por defecto correspondientes.

| Clave | Tamaño | Disposición | Se escribe cuando |
|---|---|---|---|
| `idkey` | 65 B | `[ver=1][priv 32][pub 32]` | Una vez, en el primer arranque (o si el registro falta o está dañado). Se valida al cargar: `pub` debe ser igual a X25519(`priv`, 9). |
| `settings` | 235 B | `[ver=1][region id u8][txPower i8][flags u8: bit0 custom greeting][greeting 201][longName 25][shortName 5]`; cadenas UTF-8 rellenadas con NUL | Cambio de potencia, nombre o saludo; cambio de región (con espera de 2 s o al salir de Config) |
| `greeted` | 2003 B | `[ver=1][count u16 LE][500 × node number u32 LE]` | Cada vez que se saluda a un nodo (automática o manualmente) |
| `peer0` … `peer49` | 89 B cada uno | `[ver=1][node number u32 LE][seq u32 LE][longName 40][shortName 8][public key 32]` | Cuando intercambiamos un DM con un nodo con clave (una vez por contacto y arranque); cuando los 50 están ocupados, se reutiliza la ranura escrita hace más tiempo |
| `region` | 1 B | ID de región | **Solo lectura**, como alternativa heredada de una compilación de diagnóstico antigua cuando falta `settings` |

ID de región (persistentes): `0` ANZ, `1` BR_902, `2` US, `3` EU_868, `4` RU, `5` KR, `6` UA_868, `7` MY_919, `8` SG_923, `9` PH_868, `10` PH_915, `11` IN, `12` NZ_865, `13` NP_865, `14` KZ_863, `15` JP, `16` TW, `17` TH.

El historial de chat, la lista de nodos (salvo los contactos guardados), los contadores de no leídos, el estado del saludo automático y el presupuesto de tiempo en el aire están **solo en RAM**.

## Presupuesto de RAM

Tamaños estáticos aproximados en el ESP32-S3 (32 bits):

| Estructura | Tamaño | Notas |
|---|---|---|
| `ChatStore` | ~21 KB | 96 ranuras × 224 B (texto de 200 bytes + estado + contacto + secuencia) |
| `NodeDb` | ~15 KB | 100 entradas × 128 B + 50 registros de ranuras persistentes (las claves persistentes también se fijan en RAM: 50 × 32 B) |
| Búfer de líneas de la UI | ~9 KB | 210 líneas partidas × 44 B para dibujar el historial |
| `Outbox` | ~2.2 KB | 10 × 216 B |
| `Greeter` | ~2 KB | 500 ID saludados + 10 en cola |
| Cola de TX | ~1.6 KB | 6 × 264 B |
| `DedupCache` | 0.5 KB | 64 × (from, id) |
| Limitadores de frecuencia | ~0.9 KB | 3 × 32 ranuras |
| Lienzo de la pantalla | ~63 KB (heap) | 240 × 135 × 16 bits, reservado una vez al inicio; si la reserva falla, la UI dibuja directamente en la pantalla |

El blob `greeted` (2 KB) se monta en la pila mientras se carga o se guarda.

## Límites

| Límite | Valor | Origen |
|---|---|---|
| Tamaño de trama | 255 B (16 de encabezado + 239 de payload) | `Radio::kMaxFrame`, `packet.cpp` |
| Texto por mensaje | 199 bytes | `Settings::kMaxGreeting`, `ChatMsg::text[200]` |
| `Data.payload` aceptado | 233 B | `MeshData::payload` |
| Nombre largo / corto | 24 / 4 bytes | `Settings` |
| Nodos en RAM | 100 | `NodeDb::kMaxNodes` |
| Contactos persistentes | 50 | `NodeDb::kMaxPeers` |
| Mensajes por conversación / total | 30 / 96 | `ChatStore` |
| Outbox (en cola + esperando clave) | 10 | `Outbox::kMax` |
| Cola de TX | 6 tramas | `App::kTxQueue` |
| Conjunto de saludados / cola de saludos | 500 / 10 | `Greeter` |
| Caché de duplicados | 64 pares | `DedupCache` |
| Memoria de reconfirmación (re-ACK) | 8 DM, como máximo 2 re-ACK cada uno | `AckMemory` |
| Separación entre nuestras tramas | 1.5 s | `kTxGapMs` |
| Intervalo del saludo automático | 30 s | `Greeter` |
| Período de NodeInfo | 3 h | `kNodeInfoPeriodMs` |
| Separación de NodeInfo manual / por cambio de nombre | 30 s | `kManualNodeInfoMs` |
| Solicitud de NodeInfo a un destino de DM sin clave | 1 cada 60 s por nodo | `kNodeInfoRequestMs` |
| Respuestas de NodeInfo | 1 cada 12 h por nodo, 1 cada 30 s en total | `niReplies_` |
| NAK + NodeInfo para PKI con clave desconocida | 1 cada 10 min por nodo, 1 cada 30 s en total | `unknownKey_` |
| Espera para guardar la configuración (región) | 2 s | `App::tick` |
| Duración del aviso | 2.5 s | `kToastMs` |
| Rearme de RX sin actividad | 10 min | `kRxRearmMs` |
| Reintento del Greeter tras un fallo de NVS | 60 s | `Greeter::kRetryBackoffMs` |
