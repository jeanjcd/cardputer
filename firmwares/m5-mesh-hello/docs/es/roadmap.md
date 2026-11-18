> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/roadmap.md).

# Hoja de ruta y limitaciones conocidas

La v0.1.0 es un nodo Meshtastic deliberadamente pequeño. Esta página enumera lo que todavía no hace, ideas para una v2 y las limitaciones conocidas de la v1. Ninguna de las ideas es una promesa. Las contribuciones son bienvenidas (consulta [CONTRIBUTING.md](../../CONTRIBUTING.md), en inglés).

## Ideas para v2

| Idea | Notas |
|---|---|
| **GPS / posición** | Usar el GNSS ATGM336H del Cap (UART, ESP RX G15 / TX G13) para enviar `POSITION_APP` y mostrar la distancia a otros nodos. La TX LoRa interfiere con el receptor GNSS, así que hay que planificar ambos con cuidado. |
| **Más canales y PSK personalizadas** | Canales secundarios, claves personalizadas, canales AES-256-CTR (PSK de 32 bytes), nombres de canal distintos de `LongFast` e importación de URL/QR de canal. Hoy solo se admite el canal primario por defecto, y `ctrCrypt` es solo AES-128. |
| **Otros presets** | LongSlow, MediumFast, ShortFast y los demás presets de módem (la fórmula de la frecuencia ya tiene en cuenta el nombre del canal y el ancho de banda). |
| **Firma y verificación XEdDSA** | Firmar nuestros paquetes y verificar los paquetes firmados de otros, como hace el firmware de Meshtastic más reciente. |
| **Rol de repetidor / router** | Retransmitir opcionalmente los paquetes de otros nodos (gestión del límite de saltos, enrutamiento next-hop) en lugar de `CLIENT_MUTE`. |
| **Reintento de DM** | Retransmitir un DM unas cuantas veces cuando no llega ningún ACK, como hace el reliable router de Meshtastic. |
| **Fuente UTF-8** | Una fuente de pantalla con glifos de portugués, español y ruso y emojis, además de entrada no ASCII desde el teclado. |
| **Historial de chat persistente** | Conservar los mensajes entre reinicios (tarjeta SD o NVS, con límite). |
| **TX no bloqueante** | Transmisión por interrupción para que la interfaz siga respondiendo durante los hasta 2.2 s de tiempo en el aire. |
| **Telemetría / traceroute** | Mostrar la telemetría de otros nodos y responder o enviar traceroutes. |
| **Indicador de batería** | Leer el ADC de la batería, que la v1 no usa. Requiere cuidado: la fuente de entropía de RF usada para generar claves comparte hardware con el ADC, así que el ADC debe configurarse después de generar las claves. |
| **¿MeshCore?** | Compatibilidad con el protocolo MeshCore como malla alternativa. Por explorar. |
| **Migración de identidad** | Una migración versionada del registro de la clave, para que un futuro cambio de formato nunca regenere la clave. |
| **Olvidar la clave de un nodo** | Una acción para olvidar la clave de un contacto guardado, de modo que un contacto que restableció su dispositivo (clave nueva) pueda aceptarse de nuevo. Hoy una clave guardada nunca se puede sustituir. |

## Limitaciones conocidas de v1

| Limitación | Detalles |
|---|---|
| **Texto de 199 bytes** | Los mensajes, los saludos y la línea de entrada están limitados a 199 bytes de UTF-8. |
| **Glifos de la pantalla** | A la fuente CP437 le faltan algunas letras acentuadas (se muestran sin tilde, p. ej. `ã → a`) y todos los emojis (se muestran como `?`). El texto enviado nunca se modifica. Consulta la [guía de usuario](user-guide.md#limitaciones-de-la-pantalla). |
| **Teclado solo ASCII** | Solo se puede escribir ASCII imprimible. No se puede escribir el carácter `` ` `` (es Esc). |
| **La TX bloqueante congela la interfaz** | Mientras se transmite una trama (hasta unos 2.2 s para 255 bytes), no se leen las teclas y la pantalla no se actualiza. |
| **Sin repetición de teclas** | Mantener pulsada una tecla no la repite. |
| **Desplazamiento en las pantallas de texto** | El historial se desplaza con `Fn`+`;` / `Fn`+`.`, una línea por pulsación. |
| **Sin reintento de DM** | Un DM sin ACK se queda en `.`. |
| **Mensajes solo en RAM** | 30 por conversación, 96 en total, se pierden al apagar. |
| **100 nodos en RAM** | Cuando la lista está llena, primero se descarta el nodo sin clave oído hace más tiempo que no sea un contacto guardado; si no lo hay, el nodo oído hace más tiempo. Las claves de los contactos de DM guardados (hasta 50) se mantienen en una tabla aparte, así que sobreviven a la salida de la lista. |
| **Las claves de contactos guardados no se pueden sustituir** | Si un contacto guardado restablece su dispositivo y obtiene una clave nueva, se rechaza y los DM con él dejan de funcionar. Todavía no hay una acción "olvidar la clave del nodo". Consulta las [preguntas frecuentes de la guía](user-guide.md#preguntas-frecuentes). |
| **Conjunto de 500 saludados** | Tras 500 nodos saludados, el saludo automático deja de saludar a nodos nuevos, para que nadie reciba dos saludos. |
| **Un solo canal y preset** | Solo el canal primario por defecto, solo LongFast. |
| **Sin firmas** | XEdDSA no está implementado. Consulta [security.md](security.md#lo-que-no-está-protegido). |
| **Sin app de teléfono, Wi-Fi, BLE ni MQTT** | El dispositivo solo se configura desde su propio teclado. |
| **Extracción física de la clave** | El cifrado de la flash está desactivado con M5Launcher, así que la clave privada de la NVS se puede leer con acceso físico. |
| **Cambio de región perdido en 2 s** | Un cambio de región se guarda 2 s después del último cambio, o al salir de Config. Si apagas antes, se pierde. No se transmite nada hasta que se guarda. |
| **Sin detección automática del Cap en funcionamiento** | Si falta el Cap al arrancar (`SEM CAP LoRa`), hay que reiniciar el firmware después de solucionarlo. |
| **Regiones experimentales** | IN, NZ_865, NP_865, KZ_863, JP, TW y TH están fuera de la banda nominal de 868–923 MHz del Cap, y su rendimiento no se ha medido. |
| **Interoperabilidad de extremo a extremo aún no verificada en campo** | La inicialización de la radio, la TX, la persistencia de la identidad y la interfaz están verificadas en el dispositivo. El intercambio de DM y mensajes de canal con otros nodos Meshtastic está implementado y cubierto por pruebas con vectores compatibles con el firmware oficial, pero todavía no se ha confirmado en campo. |
