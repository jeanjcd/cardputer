> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/security.md).

# Seguridad

Esta página trata de lo que M5-Mesh-Hello v0.1.0 protege, lo que no protege y cómo verificar una versión. Para informar de una vulnerabilidad, sigue [SECURITY.md](../../SECURITY.md) (en inglés). No abras una issue pública.

## Modelo de amenazas

| Adversario | Capacidad | Principales preocupaciones |
|---|---|---|
| **Atacante remoto por LoRa** | Puede transmitir tramas arbitrarias en la frecuencia de la región a distancia | Corrupción de memoria o cuelgues por tramas mal formadas, saltarse el cifrado, obligar al dispositivo a transmitir (abuso del aire), agotamiento del almacenamiento |
| **Nodo Meshtastic malicioso o suplantador** | Puede enviar tramas Meshtastic bien formadas con cualquier número de nodo `from`, nombres y clave | Suplantar a otros nodos, sustituir la clave de un contacto, inundar con solicitudes de NodeInfo, hacernos enviar saludos masivos |
| **Acceso físico** | Tiene el dispositivo o su chip de flash | Leer la clave privada, leer los contactos guardados y la configuración, instalar firmware modificado |
| **Cadena de suministro** | Manipula las dependencias, la CI o los binarios publicados | Código malicioso en la compilación o en `bin/` |

## Protecciones

### Entrada de radio no confiable

- **Cada byte que llega de la radio es no confiable.** El analizador del encabezado, el decodificador de protobuf, el descifrado y la lógica de la aplicación comprueban cada longitud e índice frente a búferes de tamaño fijo. Se rechazan las tramas de más de 255 bytes, los payloads de más de 239 bytes, los campos de protobuf que se salen del final, los campos `payload` demasiado grandes y las claves públicas que no tienen 32 bytes.
- **Sin reservas de heap en la ruta de RX.** Todos los búferes tienen tamaño fijo, y la estructura decodificada grande es estática.
- **Fuzzing y sanitizers.** `test/test_fuzz` pasa 200000 tramas aleatorias y mutadas por toda la ruta de recepción. La CI ejecuta todas las pruebas nativas, incluida la de fuzzing, con AddressSanitizer y UndefinedBehaviorSanitizer.
- **Análisis estático.** CodeQL (`security-extended`) se ejecuta en cada push y pull request y cada semana. El job falla ante cualquier hallazgo.

### Criptografía

- Los DM usan **X25519 + SHA-256 + AES-256-CCM** con una etiqueta de 8 bytes, como en Meshtastic. Las etiquetas se comparan en **tiempo constante**, y un descifrado fallido borra el búfer de texto en claro, así que nunca se usan datos no autenticados.
- Se rechazan las claves X25519 débiles de los contactos (las que dan un secreto compartido no válido).
- **Los secretos se borran** con `secureZero()` tras su uso: claves compartidas, la salida bruta de X25519, los key schedules, el bloque contador de CTR y el registro de clave leído de la NVS.
- La clave privada **nunca se muestra, se imprime ni se registra**. La interfaz muestra solo los 4 primeros bytes de la clave pública.
- **La generación de claves usa entropía de hardware.** La fuente de entropía por ruido de RF del ESP32-S3 se activa (`bootloader_random_enable`) mientras se genera la clave, porque el Wi-Fi y el Bluetooth están apagados. El generador de ID de paquete también se siembra con entropía de hardware en cada arranque, así que los bloques contadores de CTR y los nonces de CCM no se repiten entre reinicios, y cada DM lleva un `extraNonce` aleatorio nuevo.
- El par de claves guardado se comprueba al cargarlo (`pub == X25519(priv)`). Un registro dañado nunca se usa a medias.

### Comportamiento de la radio y antiabuso

- **El Wi-Fi y el Bluetooth están apagados.** La única interfaz remota es LoRa.
- **El dispositivo nunca retransmite** el tráfico de otros nodos (`CLIENT_MUTE`).
- **Las respuestas automáticas tienen límite de frecuencia**, por nodo y globalmente, para que números `from` falsificados no puedan mantener el dispositivo en el aire:
  - Respuestas de NodeInfo: una vez cada 12 h por nodo y una vez cada 30 s en total.
  - NAK + NodeInfo para DM PKI que no podemos descifrar: una vez cada 10 min por nodo y una vez cada 30 s en total.
  - Solicitudes de NodeInfo cuando envías un DM a un nodo sin clave: una vez cada 60 s por nodo.
  - Nuestras propias difusiones de NodeInfo: como máximo una cada 30 s, más una cada 3 h.
- **Ritmo de transmisión**: una trama a la vez, separadas al menos 1.5 s, una cola de 6 tramas y el presupuesto de tiempo en el aire del ciclo de trabajo de la región.
- **Saludo automático**: desactivado al arrancar, saluda a cada nodo como máximo una vez (persistente), como máximo un saludo cada 30 s, con una cola de 10.
- **Supresión de duplicados** de los últimos 64 pares `(from, id)`.
- **Almacenamiento acotado**: blobs NVS de tamaño fijo, como máximo 50 contactos guardados (ranuras reutilizadas), como máximo 500 ID saludados. Una avalancha de nodos falsos no puede hacer crecer el almacenamiento.

### Identidad y claves

- **Gana la primera clave**: se conserva la primera clave pública recibida para un número de nodo, y se ignora un NodeInfo posterior con otra clave. Las claves de los contactos de DM se guardan de forma persistente, así que esto sobrevive a los reinicios.
- **Se rechazan los DM heredados**: un DM de texto cifrado solo con la clave del canal público se descarta, así que un DM que aparece en una conversación siempre se autenticó mediante PKI con la clave guardada del remitente.

### Cadena de suministro

- Las bibliotecas y la plataforma están **fijadas a versiones exactas** en `platformio.ini`. La biblioteca criptográfica está incluida en el repositorio (`lib/crypto/`, MIT).
- Las GitHub Actions están **fijadas al SHA completo de un commit**, los workflows se ejecutan con `contents: read`, el checkout no conserva credenciales y Dependabot propone actualizaciones de las actions.
- Las imágenes de la versión vienen con **`bin/SHA256SUMS`**. La CI las vuelve a verificar en cada ejecución.

## Lo que NO está protegido

- **La clave privada se puede leer con acceso físico.** El cifrado de la flash y el secure boot están desactivados, porque el firmware se instala con M5Launcher, que no puede usarlos. Cualquiera que pueda volcar la flash puede leer el par de claves de la NVS (clave `idkey`) y suplantar al nodo en los DM. También se pueden leer los contactos guardados, la lista de saludados y la configuración. Si pierdes el dispositivo, considera su identidad comprometida.
- **El canal público lo puede leer cualquiera.** Usa la clave por defecto bien conocida de Meshtastic, por diseño, así que cada mensaje que envías en la pantalla Canal, y cada NodeInfo (nombres, clave pública, modelo de hardware), es público.
- **Sin firmas de paquetes (XEdDSA) en v1.** Las tramas de difusión, incluidos los textos de canal y los NodeInfo, no están autenticadas. Cualquiera puede enviar un mensaje de canal o un NodeInfo que diga tener cualquier número de nodo. La política por defecto de Meshtastic también acepta paquetes sin firmar.
- **Suplantación antes de conocer la clave.** Los números de nodo no están vinculados a claves. Mientras no hayamos guardado la clave real de un nodo, un atacante puede enviar un NodeInfo con ese número de nodo y su propia clave, y la regla "gana la primera clave" conserva entonces la clave del atacante. Los DM "para" ese nodo irían al atacante, y los DM del nodo real se rechazarían. Es una propiedad del protocolo oficial.
- **Los metadatos son visibles.** Los números de nodo, los nombres, el modelo de hardware, el momento y el tamaño de los paquetes y quién envía a quién (los campos `to`/`from` del encabezado no están cifrados) son visibles para cualquier receptor. Tu ID de nodo se deriva de la dirección MAC del dispositivo.
- **El contenido de los DM está cifrado de extremo a extremo, pero la entrega no está garantizada.** No hay reintentos, y un atacante puede interferir la frecuencia (jamming) o eliminar los ACK.
- **La denegación de servicio en la capa física** (jamming) queda fuera del alcance.
- **El historial de chat** está solo en RAM y desaparece al apagar. No está protegido frente a alguien que tenga el dispositivo desbloqueado en sus manos.

## Verificar una versión

Las imágenes de la versión y sus sumas de comprobación están en [`bin/`](../../bin/):

```sh
cd bin
sha256sum -c SHA256SUMS
```

Las dos líneas deben decir `OK`. En Windows sin `sha256sum`:

```bat
certutil -hashfile M5-Mesh-Hello.bin SHA256
```

Compara el resultado con la línea de ese archivo en `SHA256SUMS`.

Las sumas de comprobación demuestran que los archivos coinciden con lo que se incluyó en el repositorio. Para comprobar que un binario corresponde al código fuente, vuelve a compilarlo (`python -m platformio run -e cardputer`) con las herramientas fijadas y compáralo. La CI también compila el firmware desde el código fuente en cada push y publica el `firmware.bin` que compila, con su propia suma de comprobación, como artefacto del workflow. No se garantiza que las compilaciones sean reproducibles bit a bit entre máquinas.

## Informar de una vulnerabilidad

Usa el informe privado de vulnerabilidades de GitHub, como se describe en [SECURITY.md](../../SECURITY.md) (en inglés). Incluye la versión, la región y, si es posible, una trama que reproduzca el problema (volcado hexadecimal).
