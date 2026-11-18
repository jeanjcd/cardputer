> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/user-guide.md).

# Guía de usuario

Esta guía cubre M5-Mesh-Hello v0.1.0 en el M5Stack **Cardputer-Adv** con el **Cap LoRa-1262**. Explica cada pantalla, tecla, ajuste y mensaje de estado.

El texto en pantalla está en portugués. Esta guía lo cita exactamente como aparece, seguido de su significado en español entre paréntesis. La pantalla no tiene glifos acentuados para algunas letras, por lo que algunos textos aparecen sin tilde en el dispositivo (consulta [Limitaciones de la pantalla](#limitaciones-de-la-pantalla)).

Contenido:

- [Antes de empezar](#antes-de-empezar)
- [Primer arranque](#primer-arranque)
- [Disposición de la pantalla](#disposición-de-la-pantalla)
- [Teclas](#teclas)
- [Pantalla Nós (nodos)](#pantalla-nós-nodos)
- [Pantalla Conversa (chat)](#pantalla-conversa-chat)
- [Pantalla Canal (canal)](#pantalla-canal-canal)
- [Pantalla Config](#pantalla-config)
- [Saludo automático](#saludo-automático)
- [Marcadores de mensaje](#marcadores-de-mensaje)
- [Cuándo escucha y cuándo transmite el dispositivo](#cuándo-escucha-y-cuándo-transmite-el-dispositivo)
- [Por qué no aparecen nodos](#por-qué-no-aparecen-nodos)
- [Solución de problemas: todos los estados y mensajes](#solución-de-problemas-todos-los-estados-y-mensajes)
- [Limitaciones de la pantalla](#limitaciones-de-la-pantalla)
- [Límites](#límites)
- [Preguntas frecuentes](#preguntas-frecuentes)

## Antes de empezar

1. **Enrosca la antena en el Cap antes de encender.** El firmware transmite un anuncio NodeInfo menos de un segundo después del arranque. Transmitir sin antena puede dañar la radio.
2. Instala el firmware con M5Launcher (consulta el [inicio rápido del README](../../README.es.md#inicio-rápido) y [hardware.md](hardware.md#instalación-con-m5launcher)).
3. Averigua qué región usan los usuarios de Meshtastic cerca de ti. En Brasil, la comunidad usa **ANZ** (la región por defecto). Consulta [frequencies.md](frequencies.md).

## Primer arranque

- En el primer arranque, el dispositivo genera un par de claves X25519 con el generador de números aleatorios del hardware y lo guarda en la flash (NVS). El par de claves **no cambia nunca** después. Los nodos Meshtastic recuerdan la primera clave pública que oyen de cada nodo ("gana la primera clave"), así que una clave nueva les impediría enviarte mensajes directos.
- Tu **ID de nodo** (`!xxxxxxxx`) se deriva de la dirección MAC de fábrica del chip y también es permanente.
- Los nombres por defecto son **4stro-031** (largo) y **4S31** (corto). Puedes cambiarlos en la pantalla Config.
- El dispositivo arranca en la región guardada la última vez (ANZ en una instalación nueva), difunde su NodeInfo pidiendo a los vecinos el suyo y luego escucha.
- **El saludo automático está DESACTIVADO** después de cada arranque.

## Disposición de la pantalla

La pantalla de 240 × 135 muestra 40 columnas de texto (los textos aparecen tal como los dibuja la fuente, por ejemplo `auto-saudaçao`):

```
+----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |  <- barra de estado (o un aviso amarillo)
| Nós*  | Conversa |  Canal  |  Config   |  <- pestañas (* = algo sin leer)
|BASE Base Stati TBEAM          -97  2m* |
|JOAO Joao Movel HELTEC_V3     -112 15s +|  <- un nodo por línea
|a3f1 !9c2ea3f1  -                -   -  |
|                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |  <- ayuda de teclas / entrada de texto
+----------------------------------------+
```

### Barra de estado

`<región> <frecuencia> | AUTO:ON o AUTO:OFF | rx <tramas recibidas> | <RSSI de la última trama>`

- **Región y frecuencia** en MHz, por ejemplo `ANZ 919.875`. Si la radio no funciona, este campo muestra `SEM CAP LoRa` o `RADIO ERRO` (consulta [Solución de problemas](#solución-de-problemas-todos-los-estados-y-mensajes)).
- **`AUTO:ON` / `AUTO:OFF`**: estado del saludo automático.
- **`rx N`**: tramas LoRa recibidas desde el arranque. Cuenta cada trama que la radio decodifica, incluidas las que el firmware no puede leer (otros canales, mensajes directos de otras personas). Si sube, la radio está oyendo algo.
- **RSSI** de la última trama recibida, por ejemplo `-97dBm`. Muestra `-dBm` hasta que llega la primera trama, y `duty limit` mientras el presupuesto de tiempo en el aire de la región retiene las transmisiones.
- Si la línea no cabe en 40 columnas, se eliminan los espacios alrededor de `|`.
- La barra se pone **roja** cuando falta el Cap, la radio ha fallado o el límite de ciclo de trabajo está activo.

Los **avisos** (toasts) son mensajes cortos que sustituyen a la barra de estado durante 2.5 segundos sobre fondo amarillo, por ejemplo `Config salva` ("configuración guardada").

### Pestañas

`Nós` (nodos), `Conversa` (chat), `Canal` (canal), `Config`. La pestaña activa aparece resaltada. `Nós` pasa a `Nós*` cuando algún nodo tiene mensajes directos sin leer, y `Canal` pasa a `Canal*` cuando hay mensajes sin leer en el canal público.

## Teclas

| Tecla | Dónde | Acción |
|---|---|---|
| `Tab` | en todas partes | Siguiente pantalla: Nós → Conversa → Canal → Config → Nós |
| `;` | Nós, Config | Mueve la selección hacia arriba (la tecla `;` tiene la flecha arriba) |
| `.` | Nós, Config | Mueve la selección hacia abajo (la tecla `.` tiene la flecha abajo) |
| `Enter` | Nós | Abre la conversación con el nodo seleccionado |
| `Enter` | Conversa, Canal | Envía el texto escrito |
| `Enter` | Config | Edita el elemento seleccionado o lo ejecuta (`Região`, `Enviar NodeInfo`); durante la edición: guarda |
| `` ` `` (Esc) | Conversa, Canal, Config | Vuelve a Nós; durante la edición de un elemento de Config: cancela la edición |
| `Fn` + `;` | Conversa, Canal | Desplaza el historial hacia arriba (mensajes más antiguos) |
| `Fn` + `.` | Conversa, Canal | Desplaza el historial hacia abajo (mensajes más recientes) |
| `del` (Retroceso) | entrada de texto, edición en Config | Borra el último carácter |
| `A` (o `a`) | solo en Nós | Activa o desactiva el saludo automático |
| letras, dígitos, símbolos | entrada de texto | Escribir (solo ASCII imprimible) |

Notas:

- Las teclas no se repiten al mantenerlas pulsadas. Pulsa una vez por carácter o paso.
- En las pantallas de texto, `;` y `.` sin `Fn` se escriben como texto, y no se puede escribir el carácter `` ` `` porque es la tecla Esc. Las demás combinaciones con `Fn` se ignoran.
- Mientras editas `Potência` (potencia), solo se aceptan dígitos.

## Pantalla Nós (nodos)

La lista de nodos oídos desde el arranque, más los contactos guardados de sesiones anteriores. El nodo oído más recientemente está arriba. Se ven diez filas, y la lista se desplaza con la selección.

Cada fila, de izquierda a derecha:

| Columna | Ancho | Contenido |
|---|---|---|
| Nombre corto | 4 | El nombre corto del nodo, o los últimos 4 dígitos hexadecimales de su número de nodo si su NodeInfo aún no ha llegado |
| Nombre largo | 10 | El nombre largo (recortado a 10 caracteres), o `!xxxxxxxx` si se desconoce |
| Modelo de hardware | 13 | Por ejemplo `TBEAM`, `HELTEC_V3`, `CARDPUTER_ADV`; `HW#<n>` para modelos que no están en la tabla del firmware; `-` si aún no se sabe nada |
| RSSI | 4 | Intensidad de señal de la última trama oída de él, en dBm; `-` si no se ha oído desde el arranque |
| Antigüedad | 3 | Tiempo desde la última vez que se oyó: `15s`, `2m`, `1h`, `3d`; `-` si no se ha oído desde el arranque |
| `*` | 1 | Mensajes directos **sin leer** de este nodo |
| `+` | 1 | Ya **saludado** (por el saludo automático o enviando el saludo a mano); nunca se le volverá a saludar automáticamente |

Colores: **amarillo** = mensajes sin leer; **blanco** = se conoce su clave pública (puedes enviarle un DM enseguida); **gris oscuro** = aún sin clave (un DM para él espera la clave; consulta el marcador `?`).

Los contactos cargados de la flash al arrancar muestran `-` en RSSI y antigüedad hasta que se vuelven a oír.

Cuando la lista está vacía muestra `Nenhum nó ouvido ainda.` ("Aún no se ha oído ningún nodo.") y `Aguarde pacotes da rede Meshtastic.` ("Espera paquetes de la red Meshtastic."). Consulta [Por qué no aparecen nodos](#por-qué-no-aparecen-nodos).

Teclas: `;`/`.` seleccionan, `Enter` abre el chat, `A` activa o desactiva el saludo automático. La línea de ayuda dice `Enter:conversa A:auto-saudação Tab:tela` ("Enter: chat, A: saludo automático, Tab: pantalla").

## Pantalla Conversa (chat)

La conversación por mensaje directo (DM) con un nodo. Ábrela con `Enter` en la pantalla Nós.

- Sin un nodo elegido muestra `Escolha um nó na aba Nós (Enter).` ("Elige un nodo en la pestaña Nós (Enter).").
- **Línea de encabezado**: `!<id de nodo> <nombre largo> <modelo> <familia del chip>`, por ejemplo `!9c2ea3f1 Base Station TBEAM ESP32`. Termina con `(sem chave)` ("sin clave") en naranja si aún no se conoce la clave pública del nodo. Si el nodo ha salido de la lista de nodos (más de 100 nodos oídos), el encabezado muestra `!<id de nodo> (fora da lista)` ("fuera de la lista").
- **Historial**: 9 líneas, la más antigua arriba; los mensajes largos se parten en varias líneas. Los mensajes recibidos empiezan por `<`. Los tuyos empiezan por un marcador de estado y `>` (consulta [Marcadores de mensaje](#marcadores-de-mensaje)). El texto recibido es blanco, el tuyo cian y los mensajes fallidos rojos. Un `^` amarillo en la esquina superior derecha indica que te has desplazado hacia arriba. Una conversación vacía muestra `(sem mensagens)` ("sin mensajes").
- **Línea de entrada** abajo: `> texto_`. Al abrir un chat aparece **rellenada con tu texto de saludo** como sugerencia. Bórrala con `del` o simplemente envíala.
- `Enter` envía el texto como DM cifrado (PKI) y vacía la entrada. `` ` `` vuelve a Nós. `Fn`+`;` / `Fn`+`.` desplazan.

Enviar el saludo exactamente como aparece rellenado cuenta como saludar a ese nodo: se marca con `+` y el saludo automático nunca lo saludará.

Abrir un chat borra la marca de no leído de ese nodo.

## Pantalla Canal (canal)

El canal público: el canal primario por defecto de Meshtastic (`LongFast`, clave por defecto), que cualquier nodo con la configuración por defecto puede leer.

- 10 líneas de historial. Los mensajes recibidos se guardan como `<nombre corto>: <texto>`, o `<últimos 4 dígitos hexadecimales del id de nodo>: <texto>` si se desconoce el nombre del remitente.
- `Enter` envía la línea de entrada a todos los del canal. Los mensajes de canal no tienen acuse de recibo, así que los tuyos se quedan en `.` una vez enviados.
- `` ` `` vuelve a Nós. `Fn`+`;` / `Fn`+`.` desplazan. Ver la pantalla borra la marca `Canal*`.

## Pantalla Config

Seis elementos. Selecciona uno con `;`/`.` y pulsa `Enter`. La línea de ayuda dice `Enter:editar/acionar  ;/. move` ("Enter: editar/ejecutar, ;/. mover"). Durante la edición, el indicador dice `<elemento>: Enter salva, ` cancela` ("Enter guarda, ` cancela") encima de una línea de entrada.

| Elemento (en pantalla) | Significado | Qué hace `Enter` |
|---|---|---|
| `Região` | Región | Pasa a la siguiente región de la lista (al final vuelve al principio) |
| `Potência` | Potencia de TX | Edita la potencia en dBm |
| `Nome longo` | Nombre largo | Edita el nombre largo |
| `Nome curto` | Nombre corto | Edita el nombre corto |
| `Saudação` | Saludo | Edita el texto del saludo |
| `Enviar NodeInfo` | Enviar NodeInfo | Difunde tu NodeInfo ahora |

### Região (región)

Muestra el nombre de la región y la frecuencia LongFast, por ejemplo `ANZ 919.875`. Cada `Enter` pasa a la siguiente región en el orden de la [tabla de regiones](frequencies.md#regiones-compatibles) y **resintoniza la radio de inmediato**. También vuelve a aplicar el límite de potencia de la región. La nueva región se guarda 2 segundos después del último cambio, o al salir de la pantalla Config (aviso `Config salva`). Si apagas el dispositivo dentro de esos 2 segundos, el cambio se pierde.

- Las regiones experimentales muestran `(exp)`, por ejemplo `IN 865.875 (exp)`. Al seleccionar una aparece el aviso `Fora da faixa do Cap (exp)` ("fuera de la banda del Cap (experimental)"). Mientras haya una seleccionada, la pantalla muestra la advertencia `! Fora de 868-923 MHz do Cap (exp)` ("fuera de los 868–923 MHz del Cap"). Estas frecuencias están fuera de la banda nominal del Cap, así que el alcance puede ser menor. Úsalas solo si es la región de tu país.
- En las regiones con límite de ciclo de trabajo (EU_868, UA_868, TH), una línea adicional muestra el tiempo en el aire usado en la última hora: `Ar 1h: 12s de 360s (duty 10%)` ("tiempo en el aire 1 h: 12 s de 360 s").
- Todos los nodos deben estar en la misma región para oírse. Consulta [frequencies.md](frequencies.md).

### Potência (potencia de TX)

La potencia de transmisión deseada, 2–22 dBm (por defecto 22). Escribe 1 o 2 dígitos y pulsa `Enter`. Un valor no válido muestra el aviso `Potência: 2 a 22 dBm` ("potencia: de 2 a 22 dBm") y deja el editor abierto.

La radio usa la **potencia efectiva** = min(tu ajuste, límite de la región, 22 dBm). Cuando la potencia efectiva es menor, el elemento muestra ambas, por ejemplo `22 dBm (efetiva 14)` ("efectiva 14"). El cambio se aplica de inmediato y se guarda (`Config salva`).

### Nome longo / Nome curto (nombre largo / corto)

Los nombres de tu nodo tal como los ven otros usuarios de Meshtastic. Nombre largo: 1–24 bytes. Nombre corto: 1–4 bytes (se recomiendan letras/dígitos ASCII). Con el teclado solo se puede escribir ASCII. Una entrada no válida muestra `Nome longo: 1 a 24 bytes` ("nombre largo: de 1 a 24 bytes") o `Nome curto: 1 a 4 letras` ("nombre corto: de 1 a 4 letras"; en realidad el límite es de 1–4 bytes).

Después de guardar un nombre, el dispositivo **difunde un nuevo NodeInfo** para que los demás nodos lo conozcan. Las difusiones se separan al menos 30 s; si la última fue hace menos de 30 s, la nueva sale cuando se cumplen los 30 s. **Tu ID de nodo y tu clave no cambian**: los demás nodos te siguen reconociendo y pueden seguir enviándote DM. Solo cambia el nombre mostrado. Los demás nodos actualizan el nombre cuando reciben el nuevo NodeInfo.

### Saudação (saludo)

El texto que usa el saludo automático y que aparece rellenado en cada chat nuevo, de hasta 199 bytes.

- Por defecto se compone a partir de tu nombre largo: `Saudações de <nombre largo>! 👋 (Cardputer-Adv)` ("¡Saludos de <nombre largo>! 👋"). Sigue a tu nombre largo cuando renombras el nodo.
- Edítalo para poner tu propio texto, que entonces se envía exactamente como lo escribiste.
- Guarda un texto **vacío** (o exactamente el texto por defecto) para volver al saludo compuesto por defecto.
- Un texto no válido muestra `Saudação inválida` ("saludo no válido").

El editor muestra el saludo con las sustituciones de visualización (por ejemplo `?` en lugar del emoji), pero el texto guardado no cambia salvo que lo edites. Si editas el saludo por defecto, el emoji se conserva mientras no lo borres.

### Enviar NodeInfo

Difunde tu NodeInfo (nombres y clave pública) en el canal público de inmediato. Úsalo para que los nodos cercanos conozcan tu clave, por ejemplo antes de que alguien intente enviarte un DM.

Las difusiones están limitadas a **una cada 30 segundos**. Este límite se comparte con el anuncio del arranque y con los cambios de nombre. Si pulsas `Enter` antes, aparece el aviso `NodeInfo em Ns` ("NodeInfo en N s") y la difusión sale automáticamente cuando se cumple el plazo. Volver a pulsar no pone en cola una segunda.

### Línea de identidad

Debajo de los elementos, en verde: `ID !xxxxxxxx  chave aabbccdd`.

- `ID !xxxxxxxx` es tu ID de nodo. Así te ven los demás en sus listas de nodos.
- `chave aabbccdd` ("clave") es una huella de tu clave **pública**: sus 4 primeros bytes en hexadecimal. Te permite comprobar que tu clave no ha cambiado tras un reinicio o una actualización. La clave privada nunca se muestra.
- Si no se pudo guardar la clave en la flash, esta zona muestra `ERRO: chave não gravada na NVS!` ("ERROR: ¡clave no guardada en la NVS!") en rojo. Consulta [Solución de problemas](#solución-de-problemas-todos-los-estados-y-mensajes).

Salir de la pantalla Config (`Tab` o `` ` ``) guarda un cambio de región pendiente. Pulsar `Tab` mientras editas un elemento descarta la edición.

## Saludo automático

El saludo automático envía tu saludo como DM a cada nodo nuevo, **una vez por nodo, para siempre**.

- **Actívalo o desactívalo con `A` en la pantalla Nós.** La barra de estado muestra `AUTO:ON` o `AUTO:OFF`. No hay ningún elemento en Config para esto.
- Está **DESACTIVADO después de cada arranque**, a propósito, para que el dispositivo nunca empiece a saludar a la gente por su cuenta.
- Solo saluda a nodos cuya **clave pública se conoce**, es decir, nodos de los que se ha recibido un NodeInfo con clave. Un DM necesita la clave. Los nodos en gris no se saludan hasta que llega su clave.
- Al activarlo, también se ponen en cola los nodos con clave ya oídos desde el arranque, los más recientes primero. Después, un nodo se pone en cola cuando llega su NodeInfo.
- Como máximo **un saludo cada 30 segundos**. El primer saludo no se retrasa.
- La cola admite como máximo **10 nodos**. Los nodos que llegan con la cola llena no se ponen en cola en ese momento. Vuelven a ponerse en cola la próxima vez que se oiga su NodeInfo (los nodos suelen volver a anunciarse cada 3 horas).
- **Nunca dos veces**: la lista de saludados se guarda en la flash (hasta 500 ID de nodo) y sobrevive a reinicios y actualizaciones. Enviar a mano el saludo exacto desde la pantalla de chat también añade el nodo a la lista. Cuando la lista llega a 500 nodos, el saludo automático deja de saludar a nodos nuevos en lugar de olvidar a los antiguos.
- Los nodos saludados muestran `+` en la lista de nodos.
- Desactivar el saludo automático vacía la cola. Los nodos ya saludados siguen marcados como saludados.

Los saludos aparecen en la conversación de cada nodo como cualquier DM que envíes, con los [marcadores](#marcadores-de-mensaje) habituales.

## Marcadores de mensaje

Tus mensajes empiezan por un marcador de estado seguido de `>`. Los recibidos empiezan por `<`.

| Marcador | Estado | Significado |
|---|---|---|
| `~` | en cola | Esperando en la cola de transmisión (o al presupuesto del ciclo de trabajo) |
| `.` | enviado | La radio lo transmitió. Para un DM, el acuse de recibo no ha llegado (todavía). Los mensajes del canal público se quedan en `.` porque el canal no tiene acuses de recibo. |
| `v` | confirmado | Ha vuelto el ACK del destinatario: el DM se entregó y se descifró |
| `?` | esperando clave | Se desconoce la clave pública del destinatario. El DM queda retenido hasta que llegue la clave (ver más abajo). |
| `x` | fallido | La transmisión falló (`Falha no TX`: la radio informó de un error al enviar, o no estaba lista), el destinatario respondió con un error (NAK) o faltaba la clave en el momento del envío |

**El estado `?`.** Un DM solo se puede cifrar con la clave pública del destinatario, que llega en su NodeInfo. Cuando envías a un nodo sin clave:

1. El mensaje se guarda con `?` y aparece el aviso `Aguardando chave do no` ("esperando la clave del nodo").
2. El dispositivo envía **su propio NodeInfo directamente a ese nodo, pidiéndole su NodeInfo a cambio** (como máximo una vez por minuto y por nodo).
3. Cuando llega el NodeInfo del nodo, todos los mensajes `?` dirigidos a él pasan a `~` y se envían.

Si el nodo está fuera de alcance o no responde, el mensaje se queda en `?`. Reenviarlo o pulsar `Enviar NodeInfo` puede ayudar cuando el nodo esté al alcance.

**Sin reintento automático.** Si un DM se queda en `.` (sin ACK), el firmware no lo vuelve a enviar. Escríbelo de nuevo si hace falta.

## Cuándo escucha y cuándo transmite el dispositivo

**La radio siempre está escuchando**, salvo cuando transmite una trama propia. Nunca retransmite paquetes de otros nodos: el dispositivo se anuncia con el rol `CLIENT_MUTE` y no redifunde nada de lo que oye. Todas sus tramas usan límite de saltos (hop limit) 3, así que otros nodos de la malla pueden retransmitirlas, como es habitual en Meshtastic.

Solo transmite en estos casos:

| Cuándo | Qué | Límite |
|---|---|---|
| Al arrancar | Difusión de NodeInfo **con want_response** (los vecinos responden con su NodeInfo) | Una vez por arranque |
| Cada 3 horas | Difusión de NodeInfo | 3 h después de la difusión de NodeInfo anterior |
| Cambias el nombre largo o corto | Difusión de NodeInfo | Como máximo una cada 30 s (compartido con el arranque y con "Enviar NodeInfo") |
| Seleccionas `Enviar NodeInfo` | Difusión de NodeInfo | Como máximo una cada 30 s |
| Envías un mensaje | Texto público (Canal) o DM cifrado con want_ack (Conversa) | Cuando pulsas Enter |
| Envías un DM a un nodo sin clave | NodeInfo enviado directamente a ese nodo con want_response | Como máximo una vez por minuto y por nodo |
| El saludo automático está ACTIVADO | DM de saludo a un nodo cuya clave acaba de conocerse | Uno cada 30 s, una sola vez por nodo |
| Recibes un DM que pide ACK | ACK al remitente (y otra vez si el remitente retransmite el mismo DM) | Una vez por DM recibido |
| Un nodo pide nuestro NodeInfo (want_response) | NodeInfo a ese nodo | Como máximo una vez cada 30 s en total **y** una vez cada 12 h por nodo |
| Un nodo nos envía un DM cifrado pero no tenemos su clave | NAK (`PKI_UNKNOWN_PUBKEY`) más nuestro NodeInfo con want_response, para que su siguiente intento se pueda descifrar | Como máximo una vez cada 30 s en total y una vez cada 10 min por nodo |

Reglas que se aplican a toda transmisión:

- **Una trama a la vez**, con al menos **1.5 s** entre el final de una trama y el inicio de la siguiente. Como máximo 6 tramas esperan en la cola de transmisión.
- Cada trama LoRa ocupa el aire durante unos **0.4–2.2 s** según su longitud, normalmente alrededor de **1 s** (consulta [tiempo en el aire](frequencies.md#tiempo-en-el-aire)).
- En las regiones con ciclo de trabajo, las tramas esperan al [presupuesto de tiempo en el aire](frequencies.md#ciclo-de-trabajo-y-presupuesto-de-tiempo-en-el-aire).
- El Wi-Fi y el Bluetooth nunca se encienden.
- Mientras se transmite una trama, el dispositivo no reacciona a las teclas (hasta unos 2.2 s).

## Por qué no aparecen nodos

La lista de nodos solo se llena cuando el dispositivo oye tramas Meshtastic que puede decodificar. Si `rx` se queda en 0, no se está recibiendo nada. Si `rx` sube pero la lista sigue vacía, llegan tramas que no se pueden decodificar (otro canal u otra clave) o que son mensajes directos de otras personas.

Comprueba, en este orden:

1. **Misma región y frecuencia.** Todos los nodos deben usar la misma región. En Brasil la comunidad usa **ANZ (919.875 MHz)**, y la región oficial es **BR_902 (903.875 MHz)**. Las dos no se oyen entre sí. Pregunta a los usuarios locales, o prueba la otra región y espera. Consulta [frequencies.md](frequencies.md).
2. **Mismo preset.** Este firmware solo habla **LongFast**, el preset por defecto de Meshtastic. Los nodos configurados con otro preset (LongSlow, MediumFast, ShortFast...) usan otros parámetros de módem y son invisibles.
3. **Canal público por defecto.** Los nombres de nodo, las claves públicas y los mensajes de canal solo se decodifican en el **canal primario por defecto** (`LongFast` con la clave por defecto `AQ==`). Los nodos en un canal privado o con clave personalizada se oyen (`rx` sube), pero no se puede decodificar nada. Los DM entre otros nodos están cifrados para ellos y tampoco puedes leerlos.
4. **Alcance, altura y antena.** El alcance de LoRa depende sobre todo de la altura y de la línea de visión. En interiores, a nivel del suelo y rodeado de edificios, puede que no oigas a nadie. Acércate a una ventana, un balcón o una azotea. Comprueba que la antena esté enroscada y en vertical. Consulta [hardware.md](hardware.md#antena-y-alcance).
5. **Dale tiempo.** Los nodos Meshtastic están callados la mayor parte del tiempo. Con la configuración por defecto, un nodo envía telemetría aproximadamente cada **30 minutos**, su NodeInfo aproximadamente cada **3 horas** y su posición periódicamente si tiene GPS y el uso compartido de posición activado. El anuncio del arranque pide a los vecinos que respondan con su NodeInfo, pero solo responden los nodos que lo oyen. Deja el dispositivo encendido una hora o más en un buen sitio.
6. **Anúnciate.** Usa `Enviar NodeInfo` para que los demás te conozcan. Recuerda el límite de 30 s.

Un nodo aparece en la lista en cuanto se oye cualquier trama decodificable suya: telemetría, posición, texto o NodeInfo. Su nombre y modelo aparecen, y se pone en blanco, solo cuando llega su NodeInfo.

## Solución de problemas: todos los estados y mensajes

### Barra de estado

| Se muestra | Significado | Qué hacer |
|---|---|---|
| `SEM CAP LoRa` (rojo) | "Sin Cap LoRa": el expansor de E/S del Cap no respondió al arrancar, o no se pudo configurar su salida del conmutador de antena | Apaga, comprueba que el Cap esté bien encajado en el conector del Cardputer-Adv y vuelve a encender. El firmware no lo reintenta mientras funciona. |
| `RADIO ERRO` (rojo) | "Error de radio": se encontró el Cap, pero el SX1262 no se inicializó | Apaga y vuelve a encender. Si persiste, el módulo de radio puede estar averiado u otro firmware lo dejó en mal estado. Vuelve a grabar el firmware e inténtalo de nuevo. |
| `duty limit` (rojo) | Se ha agotado el presupuesto de tiempo en el aire de la región para la última hora, y hay tramas esperando | No se pierde nada. Las tramas salen a medida que se libera presupuesto, dentro de la hora. Config muestra `Ar 1h: ...`. Solo EU_868, UA_868 y TH tienen presupuesto. |
| `-dBm` | No se ha recibido ninguna trama desde el arranque | Consulta [Por qué no aparecen nodos](#por-qué-no-aparecen-nodos). |

Cuando la radio no funciona, los intentos de transmitir también producen el aviso `Falha no TX` (la radio no está lista).

### Avisos (amarillos, 2.5 s)

| Aviso | Significado | Qué hacer |
|---|---|---|
| `Config salva` | "Configuración guardada" en la flash | Nada. |
| `ERRO ao gravar config` | "Error al guardar la configuración": falló la escritura en la flash | El ajuste está activo hasta reiniciar, pero puede perderse. Inténtalo de nuevo. Si sigue fallando, la partición NVS puede estar llena o dañada. |
| `ERRO: chave nao gravada!` (al arrancar) | "Error: clave no guardada": se generó un par de claves nuevo, pero no se pudo almacenar | La clave cambiará en el próximo arranque, y los nodos que guardaron la clave antigua rechazarán la nueva. Revisa la flash/NVS (reinstala). También aparece en Config como `ERRO: chave não gravada na NVS!`. |
| `ERRO no radio` | "Error en la radio" al cambiar la región o la potencia | Apaga y vuelve a encender. Puede que la radio no esté en la frecuencia mostrada. |
| `Fora da faixa do Cap (exp)` | "Fuera de la banda del Cap (experimental)": has seleccionado una región experimental | Correcto si es la región de tu país. Espera un alcance reducido. |
| `Falha no TX` | "Fallo de TX": la radio informó de un error al enviar, o la radio no está lista (por ejemplo, Cap no detectado) | El mensaje se marca con `x`. Comprueba si la barra de estado muestra `SEM CAP LoRa` / `RADIO ERRO` y apaga y vuelve a encender. |
| `Texto longo demais` | "Texto demasiado largo" (más de 199 bytes) | Acorta el mensaje. La entrada por teclado ya se detiene en 199 bytes. |
| `Fila de TX cheia` | "Cola de TX llena" (6 tramas esperando) al enviar al canal | Espera unos segundos y vuelve a enviar. En las regiones con ciclo de trabajo, el presupuesto puede estar reteniendo tramas. |
| `Fila de mensagens cheia` | "Cola de mensajes llena": ya hay 10 DM esperando (por ejemplo, varios mensajes `?`) | Espera a que lleguen las claves o a que salgan los mensajes. El DM no se guardó. |
| `Aguardando chave do no` | "Esperando la clave del nodo": DM guardado con `?`, solicitud de NodeInfo enviada | Espera el NodeInfo del nodo. Consulta [el estado `?`](#marcadores-de-mensaje). |
| `NodeInfo em Ns` | "NodeInfo en N s": se pidió una difusión de NodeInfo menos de 30 s después de la anterior | Nada. Sale automáticamente. |
| `Potência: 2 a 22 dBm` | "Potencia: de 2 a 22 dBm": valor no válido | Escribe un número de 2 a 22. |
| `Nome longo: 1 a 24 bytes` | Nombre largo vacío o demasiado largo | Escribe de 1 a 24 bytes. |
| `Nome curto: 1 a 4 letras` | "Nombre corto: de 1 a 4 letras": vacío o demasiado largo (el límite es de 1–4 bytes) | Escribe de 1 a 4 bytes (se recomiendan letras/dígitos ASCII). |
| `Saudação inválida` | "Saludo no válido" (por ejemplo, caracteres de control) | Vuelve a escribirlo. Guarda un texto vacío para restaurar el saludo por defecto. |

### Otras situaciones

| Situación | Explicación |
|---|---|
| Un DM se queda en `?` | No ha llegado la clave del nodo. Consulta [el estado `?`](#marcadores-de-mensaje). |
| Un DM se queda en `.` | Se transmitió, pero no volvió ningún ACK. El destinatario puede estar fuera de alcance, o el ACK se perdió. No hay reintento automático. |
| Un DM muestra `x` justo después de pasar de `?` a `~` | La clave del nodo desapareció antes del envío (por ejemplo, el nodo fue expulsado de una lista llena). Vuelve a enviarlo. |
| Nodo gris sin nombre | Se oyeron tramas suyas, pero no su NodeInfo. Espera, o abre un chat y envía un DM: eso solicita su NodeInfo. |
| El encabezado muestra `(fora da lista)` | Ese nodo fue expulsado de la lista de 100 nodos. Sus mensajes permanecen hasta que los desplazan otros más nuevos. |
| Teclas ignoradas un momento | Se está transmitiendo una trama (hasta unos 2.2 s). |
| Mensajes perdidos tras reiniciar | El historial de chat solo está en RAM. Se conservan los nombres y claves de los contactos de DM (hasta 50), la lista de saludados y la configuración. |

## Limitaciones de la pantalla

La pantalla usa una fuente integrada de 8 bits (página de códigos 437). El texto se convierte **solo para mostrarlo**:

- Las letras que existen en la fuente se muestran con tilde, por ejemplo `é ç ô á ü ñ`.
- Las letras acentuadas sin glifo se muestran como la letra base, por ejemplo `ã → a`, `õ → o`, `À → A`, `Ê → E`.
- Las comillas tipográficas y las rayas se convierten en `'`, `"` y `-`.
- Los emojis y cualquier otro carácter sin glifo se muestran como `?`. Los caracteres de unión invisibles y los selectores de variación se descartan.

**Lo que se envía y se guarda nunca se modifica.** Los mensajes salen exactamente como se escribieron (UTF-8), y los recibidos se conservan intactos. El propio teclado solo escribe ASCII imprimible.

## Límites

| Elemento | Límite |
|---|---|
| Texto del mensaje | **199 bytes** (UTF-8; las letras acentuadas ocupan 2 bytes y los emojis 4) |
| Nombre largo / nombre corto | 1–24 bytes / 1–4 bytes (se recomiendan letras/dígitos ASCII) |
| Saludo | 199 bytes |
| Mensajes por conversación | 30 (se descartan los más antiguos); 96 en total entre todas las conversaciones; solo en RAM |
| Nodos en la lista | 100 (se descarta el oído hace más tiempo) |
| Contactos guardados en la flash (nombre + clave) | 50 (nodos con los que intercambiaste DM) |
| Lista de saludados | 500 nodos |
| DM en espera (en cola o `?`) | 10 |
| Cola de transmisión | 6 tramas |
| Cola del saludo automático | 10 nodos |
| Potencia de TX | 2–22 dBm, limitada al máximo de la región |
| Reintentos de DM | Ninguno |

## Preguntas frecuentes

**¿Capta señales cuando alguien transmite?**
Sí, siempre que el remitente esté en la misma región/frecuencia y en el preset LongFast, y dentro del alcance. La radio escucha todo el tiempo, salvo durante sus propias transmisiones cortas. Cada trama recibida incrementa `rx` y actualiza el RSSI de la barra de estado, aunque el firmware no pueda decodificarla. Solo las tramas del canal público por defecto, más los DM dirigidos a ti, se decodifican como nodos y mensajes.

**¿Está transmitiendo mi dispositivo ahora mismo?**
Solo en las situaciones enumeradas en [Cuándo escucha y cuándo transmite el dispositivo](#cuándo-escucha-y-cuándo-transmite-el-dispositivo): el NodeInfo del arranque, cada 3 horas, después de un cambio de nombre o de `Enviar NodeInfo`, tus propios mensajes, el saludo automático (si está ACTIVADO), los ACK y las respuestas limitadas a solicitudes de NodeInfo. Nunca retransmite el tráfico de otras personas. No hay indicador de TX. Mientras se envía una trama, el teclado deja de responder brevemente. En las regiones con ciclo de trabajo, Config muestra el tiempo en el aire usado en la última hora.

**¿Qué es la región ANZ?**
ANZ es la región de Meshtastic para Australia y Nueva Zelanda (915–928 MHz). La frecuencia LongFast allí es 919.875 MHz. La comunidad Meshtastic brasileña usa ANZ, por eso es la región por defecto de este firmware. La región oficial de Meshtastic para Brasil es BR_902 (903.875 MHz). Usa la región del lugar donde estás; consulta [frequencies.md](frequencies.md#brasil).

**¿Dónde se activa y desactiva el saludo automático?**
En la pantalla **Nós**: pulsa `A`. La barra de estado muestra `AUTO:ON` o `AUTO:OFF`. Siempre está DESACTIVADO tras un reinicio. No hay ningún elemento en Config para esto.

**¿Cómo fuerzo el envío de mi NodeInfo?**
Config → `Enviar NodeInfo` → `Enter`. Sale de inmediato, o cuando se cumple lo que quede del límite de 30 s (aviso `NodeInfo em Ns`).

**¿Puedo renombrar el nodo?**
Sí. Config → `Nome longo` (1–24 bytes) y `Nome curto` (1–4 bytes, se recomiendan letras/dígitos ASCII). Los nombres nuevos se guardan y se anuncian con un NodeInfo nuevo. Tu ID de nodo y tu clave no cambian, así que los demás nodos te siguen reconociendo y pueden seguir enviándote DM.

**¿Puedo usarlo con mi teléfono o con la app de Meshtastic?**
No. El Bluetooth y el Wi-Fi están apagados, y el dispositivo solo se configura desde su propio teclado. Sí puede comunicarse por LoRa con teléfonos conectados a otros nodos Meshtastic.

**¿Leerán otros mis mensajes?**
Los mensajes del canal público los puede leer cualquiera con la configuración por defecto de Meshtastic, por diseño. Los DM están cifrados de extremo a extremo con la clave pública del destinatario. Consulta [security.md](security.md).

**¿Retransmite mensajes de otras personas?**
No. Es un nodo `CLIENT_MUTE` y nunca redifunde paquetes de otros nodos.

**¿Por qué el saludo muestra un `?` en la pantalla?**
El emoji 👋 no tiene glifo en la fuente de la pantalla. Se envía correctamente. Consulta [Limitaciones de la pantalla](#limitaciones-de-la-pantalla).
