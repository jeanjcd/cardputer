> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/frequencies.md).

# Regiones y frecuencias

Todo sobre las regiones de radio, las frecuencias, la potencia de transmisión y el ciclo de trabajo (duty cycle) en M5-Mesh-Hello v0.1.0. Todos los valores proceden de [`src/mesh/region.h`](../../src/mesh/region.h) y [`src/mesh/region.cpp`](../../src/mesh/region.cpp). Una prueba nativa, `test/test_docs_regions`, compara la tabla de regiones de la **versión en inglés** de este documento con el código en cada ejecución de la CI (consulta [Cómo se mantiene la tabla correcta](#cómo-se-mantiene-la-tabla-correcta)).

> **Aviso legal.** Usa siempre la región del país en el que te encuentras físicamente. Eres responsable de cumplir la normativa de radio local (frecuencia, potencia, ciclo de trabajo, ganancia de antena). Este documento es información técnica, no asesoramiento jurídico.

## Qué es una región de Meshtastic

Una radio LoRa escucha exactamente una frecuencia a la vez. Para que dos nodos Meshtastic se comuniquen, deben usar la misma frecuencia y los mismos parámetros de módem. Meshtastic define una **región** para cada zona regulatoria. Una región es una banda de frecuencias (inicio y fin), un límite de ciclo de trabajo, un límite de potencia y una separación entre canales. Un nodo toma la región, el nombre del canal (aquí `LongFast`) y el ancho de banda, y a partir de ellos calcula la frecuencia central real con una fórmula fija. Así, todos los nodos de la misma región con el canal por defecto acaban en la misma frecuencia sin ninguna coordinación.

M5-Mesh-Hello usa siempre el preset **LongFast** (ancho de banda 250 kHz, spreading factor 11, coding rate 4/5, sync word `0x2B`, preámbulo de 16 símbolos, encabezado explícito, CRC activado) en el canal primario por defecto llamado `LongFast`. La región es el único ajuste de radio que eliges. Cámbiala en la pantalla **Config** (`Região`; Enter recorre la lista). La región por defecto es **ANZ**.

## Regiones compatibles

Estas son las regiones cuya frecuencia LongFast cae dentro de la banda nominal del Cap LoRa-1262, de 868–923 MHz. Las regiones **experimentales** tienen una frecuencia LongFast en [860, 868) o (923, 925] MHz, justo fuera de la especificación del Cap. La pantalla Config las muestra en este orden.

La tabla de referencia, verificada por la CI, es la de la [versión en inglés](../en/frequencies.md#supported-regions). Esta es una copia traducida.

| Región | Países / uso | Banda (MHz) | LongFast (MHz) | Potencia máx. (dBm) | Duty cycle (%) | Experimental |
|---|---|---|---|---|---|---|
| ANZ | Australia, Nueva Zelanda; también la usa la comunidad brasileña | 915.0 – 928.0 | 919.875 | 30 | 100 | no |
| BR_902 | Brasil (región oficial de Meshtastic) | 902.0 – 907.5 | 903.875 | 30 | 100 | no |
| US | Estados Unidos (y otros países que usan 902–928 MHz) | 902.0 – 928.0 | 906.875 | 30 | 100 | no |
| EU_868 | Unión Europea (banda SRD de 868 MHz) | 869.4 – 869.65 | 869.525 | 27 | 10 | no |
| RU | Rusia | 868.7 – 869.2 | 869.075 | 20 | 100 | no |
| KR | Corea del Sur | 920.0 – 923.0 | 922.875 | 23 | 100 | no |
| UA_868 | Ucrania | 868.0 – 868.6 | 868.375 | 14 | 1 | no |
| MY_919 | Malasia | 919.0 – 924.0 | 922.875 | 27 | 100 | no |
| SG_923 | Singapur | 917.0 – 925.0 | 917.875 | 20 | 100 | no |
| PH_868 | Filipinas (868 MHz) | 868.0 – 869.4 | 868.125 | 14 | 100 | no |
| PH_915 | Filipinas (915 MHz) | 915.0 – 918.0 | 917.875 | 24 | 100 | no |
| IN | India | 865.0 – 867.0 | 865.875 | 30 | 100 | sí |
| NZ_865 | Nueva Zelanda (865 MHz) | 864.0 – 868.0 | 864.875 | 36 | 100 | sí |
| NP_865 | Nepal | 865.0 – 868.0 | 867.875 | 30 | 100 | sí |
| KZ_863 | Kazajistán | 863.0 – 868.0 | 866.875 | 30 | 100 | sí |
| JP | Japón | 920.5 – 923.5 | 923.375 | 13 | 100 | sí |
| TW | Taiwán | 920.0 – 925.0 | 923.875 | 27 | 100 | sí |
| TH | Tailandia | 920.0 – 925.0 | 923.875 | 27 | 10 | sí |

Notas:

- Un ciclo de trabajo del 100 % significa que el firmware no aplica ningún límite de tiempo en el aire.
- "Potencia máx." es el límite de la región copiado de Meshtastic. El Cap llega como máximo a 22 dBm, así que la potencia que realmente se usa es menor (consulta [Potencia de transmisión](#potencia-de-transmisión)).
- Todas las regiones de esta tabla tienen una separación entre canales de 0 MHz.
- Los límites de banda, los ciclos de trabajo, los límites de potencia y la separación están copiados de la tabla `RDEF` de [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp) de Meshtastic. Los nombres de países son el significado habitual de cada código de región. El proyecto oficial puede añadir o cambiar regiones en versiones posteriores.

## Regiones no compatibles

| Región(es) | Banda | Por qué no |
|---|---|---|
| Regiones de 433 MHz (por ejemplo `EU_433`, `UA_433`, `MY_433`, `PH_433`, `KZ_433`) | 433 MHz | La etapa de RF y la antena del Cap LoRa-1262 están diseñadas para 868–923 MHz. 433 MHz requiere otro hardware. |
| `CN` | 470–510 MHz | El mismo motivo: muy fuera de la banda del Cap. |
| `LORA_24` | 2.4 GHz | Requiere otro chip de radio (SX128x). El SX1262 no puede sintonizar ahí. |
| Cualquier región cuya frecuencia LongFast quede fuera de 860–925 MHz | — | No probada y fuera del margen aceptado para las regiones experimentales. |

## Cómo se calcula la frecuencia

El código sigue exactamente `RadioInterface::applyModemConfig()` de Meshtastic, incluida su aritmética de coma flotante de precisión simple ([`regionFreqMHz`](../../src/mesh/region.cpp)):

```
hash        = djb2("LongFast")                       // 130429955
numChannels = floor((freqEnd - freqStart) / (spacing + bw/1000))
slot        = hash % numChannels
freq        = freqStart + bw/2000 + slot * bw/1000   // MHz; bw in kHz (250)
```

- `djb2` es el hash clásico de cadenas: empieza con `h = 5381` y, para cada byte, `h = h * 33 + byte`, como valor de 32 bits sin signo. Para `"LongFast"` da **130429955**.
- `bw/2000` es la mitad del ancho de banda en MHz (0.125 MHz), así que el centro del canal queda medio ancho de banda por encima del inicio de su slot.
- Si `numChannels` fuera 0 (una banda más estrecha que un canal), el código devuelve `freqStart + bw/2000`. Ninguna región compatible se encuentra en este caso.

### Ejemplo: ANZ

| Paso | Valor |
|---|---|
| Banda | 915.0 – 928.0 MHz, separación 0 |
| `numChannels` | floor(13.0 / (0 + 0.25)) = **52** |
| `slot` | 130429955 mod 52 = **19** |
| `freq` | 915.0 + 0.125 + 19 × 0.25 = **919.875 MHz** |

El mismo método para otras regiones:

| Región | numChannels | slot | LongFast (MHz) |
|---|---|---|---|
| BR_902 | 22 | 7 | 903.875 |
| US | 104 | 19 | 906.875 |
| EU_868 | 1 | 0 | 869.525 |
| SG_923 | 32 | 3 | 917.875 |

### Hash del canal

Cada trama lleva en su encabezado un hash de canal de un byte para que los receptores elijan la clave correcta. Es el XOR de todos los bytes del nombre del canal, combinado mediante XOR con todos los bytes de la clave. Para `"LongFast"` con la clave por defecto es **`0x08`**. Consulta [protocol.md](protocol.md#cifrado-del-canal-aes-128-ctr).

## Potencia de transmisión

Fijas una potencia de 2 a 22 dBm en la pantalla Config (`Potência`, por defecto 22). La radio usa la **potencia efectiva** ([`effectiveTxPower`](../../src/mesh/region.cpp)):

```
limit     = region power limit, or 17 dBm if the region's limit is 0 ("unknown", as in Meshtastic)
effective = min(user setting, limit, 22)     and at least 2 dBm
```

Es decir: `limit` es el límite de potencia de la región, o 17 dBm si el límite de la región es 0 ("desconocido", como en Meshtastic); la potencia efectiva es el mínimo entre tu ajuste, `limit` y 22, y como mínimo 2 dBm.

22 dBm es el máximo del SX1262. Cuando la potencia efectiva es menor que tu ajuste, la pantalla Config muestra ambas, por ejemplo `22 dBm (efetiva 14)` ("efectiva 14") en UA_868 o PH_868, o `22 dBm (efetiva 20)` en RU y SG_923. El límite de potencia se vuelve a aplicar cada vez que cambias de región.

No se tiene en cuenta la ganancia de la antena. Si tu normativa local limita la PIRE/PRA (EIRP/ERP) y usas una antena de alta ganancia, reduce la potencia tú mismo.

## Ciclo de trabajo y presupuesto de tiempo en el aire

Algunas regiones limitan la fracción de tiempo durante la que un dispositivo puede transmitir: EU_868 y TH permiten el 10 %, UA_868 permite el 1 %. En esas regiones, el firmware lleva una cuenta móvil de una hora del tiempo en el aire ([`AirtimeBudget`](../../src/mesh/region.h)) y retiene las tramas cuando la siguiente superaría el presupuesto.

- **Presupuesto** = duty % de una hora: 10 % → **360 s por hora**, 1 % → **36 s por hora**.
- **Contabilidad**: 61 cubetas de un minuto. Una trama enviada durante el minuto *M* sigue contando hasta el minuto *M + 60*, así que cuenta durante **al menos 3600 s** (hasta 3660 s). Esto peca de prudente.
- **Antes de cada transmisión**, el firmware calcula el tiempo en el aire de la trama (redondeado hacia arriba a milisegundos enteros) y comprueba `used + airtime ≤ budget`. Si la comprobación falla, la trama y todo lo que está en cola detrás de ella **esperan**. No se descarta nada. La barra de estado muestra `duty limit` en rojo, y la pantalla Config muestra `Ar 1h: Xs de Ys (duty Z%)` ("tiempo en el aire en la última hora: X s de Y s").
- En las regiones con ciclo de trabajo del 100 % no hay presupuesto y la línea de Config no se muestra.

### Tiempo en el aire

El firmware usa la fórmula del Semtech SX126x ([`loraAirtimeUs`](../../src/mesh/region.cpp)) con los parámetros de LongFast: SF11, BW 250 kHz, CR 4/5, preámbulo 16, encabezado explícito, CRC activado, optimización para baja tasa de datos desactivada.

```
Tsym          = 2^SF / BW                              = 2048 / 250 kHz = 8.192 ms
preamble      = (16 + 4.25) × Tsym                     = 165.888 ms
payloadSymbols = 8 + ceil((8·L − 4·SF + 28 + 16) / (4·SF)) × CR   // L = whole frame length in bytes, CR = 5
               = 8 + ceil(8·L / 44) × 5
airtime       = preamble + payloadSymbols × Tsym
```

(`L` es la longitud total de la trama en bytes; `CR` = 5.)

Así, cada trama cuesta una parte fija de **≈ 231 ms** (L = 0), más 5 símbolos (41 ms) por cada 5.5 bytes, lo que da una media de **≈ 7.4 ms por byte**:

| Longitud de la trama L (encabezado + payload) | Tiempo en el aire |
|---|---|
| 0 B (mínimo teórico) | 231 ms |
| 22 B (texto público corto, p. ej. "hola") | 395 ms |
| 88 B (nuestro NodeInfo con los nombres por defecto) | 887 ms |
| 128 B | 1.21 s |
| 200 B | 1.75 s |
| 255 B (trama máxima) | 2.16 s |

Una trama típica dura alrededor de **1 s**. En EU_868, 360 s por hora equivalen a unas 400 tramas del tamaño de un NodeInfo por hora, mucho más de lo que este firmware envía en uso normal. En UA_868 (36 s por hora) caben unas 40 tramas así por hora.

## Regiones experimentales

El M5Stack Cap LoRa-1262 está especificado para **868–923 MHz**. El propio chip SX1262 sintoniza de **150 a 960 MHz**, así que la radio puede ajustarse a frecuencias algo fuera de la especificación del Cap. Su conmutador de RF, su filtrado y su adaptación de impedancia están diseñados para la banda nominal. Fuera de ella, cuenta con una potencia de transmisión más débil y peor sensibilidad. Cuánto peor no se ha medido.

El firmware incluye regiones cuya frecuencia LongFast está en [860, 868) MHz (IN, NZ_865, NP_865, KZ_863) o en (923, 925] MHz (JP, TW, TH) y las señala:

- En Config, la región muestra `(exp)`, por ejemplo `IN 865.875 (exp)`.
- Al seleccionar una, aparece el aviso `Fora da faixa do Cap (exp)` ("fuera de la banda del Cap (experimental)").
- Mientras haya una seleccionada, Config muestra `! Fora de 868-923 MHz do Cap (exp)` ("fuera de los 868–923 MHz del Cap").

Usa una región experimental solo si es la región correcta de tu país, y cuenta con un alcance reducido.

**Japón (JP).** Las normas japonesas de la banda de 920 MHz (ARIB STD-T108) pueden exigir detección de portadora (LBT) y límites de tiempo de transmisión; este firmware no implementa LBT y no se ha evaluado su conformidad — comprueba tú mismo los requisitos de 電波法/技適 antes de transmitir en Japón.

## Brasil

En Brasil hay dos regiones relevantes, y **no** se oyen entre sí:

| Región | LongFast | Situación |
|---|---|---|
| **ANZ** | 919.875 MHz | La que usa hoy la comunidad Meshtastic brasileña (por ejemplo [meshbrasil.com](https://meshbrasil.com)). **La región por defecto de este firmware.** |
| **BR_902** | 903.875 MHz | La región oficial de Meshtastic para Brasil (902–907.5 MHz). |

Las normas de Anatel (el regulador brasileño de telecomunicaciones) para equipos de radiación restringida (sin licencia) permiten dispositivos de espectro ensanchado, como LoRa, en **902–907.5 MHz y 915–928 MHz** dentro de la banda de 902–928 MHz. El tramo de 907.5–915 MHz no forma parte de esa atribución sin licencia. Las dos frecuencias anteriores caen dentro de tramos permitidos. **Verifica la normativa vigente de Anatel** antes de confiar en esto.

Si no oyes a nadie en ANZ, pregunta a los usuarios locales qué región usan, o prueba BR_902.

## Antenas por banda

Una antena está sintonizada para un rango de frecuencias. Fuera de él, la ROE (VSWR) aumenta y parte de la potencia se refleja en lugar de radiarse.

| Frecuencia de tu región | Mejor antena |
|---|---|
| 863–870 MHz (EU_868, RU, UA_868, PH_868, IN, NZ_865, NP_865, KZ_863) | Antena de 868 MHz |
| 902–928 MHz (ANZ, BR_902, US, KR, MY_919, SG_923, PH_915, JP, TW, TH) | Antena de 915 MHz |

La antena que viene con el Cap está sintonizada a 868 MHz. En las regiones de 902–928 MHz sigue funcionando, con una pérdida esperada de aproximadamente **0.5–2 dB**: 919.875 MHz está un 6 % por encima de su sintonía y 903.875 MHz un 4 % por encima. Con una ROE de 2:1 se refleja alrededor del 11 % de la potencia (≈ 0.5 dB). Con 3:1 la pérdida es ≈ 1.25 dB. Eso cuesta más o menos un 5–20 % del alcance. Si compras una antena para una región de 902–928 MHz, elige una sintonizada a 915 MHz en lugar de una de "banda ancha". Con la antena de 868 MHz, BR_902 (903.875 MHz) está algo más cerca de su sintonía que ANZ. Consulta [hardware.md](hardware.md#antena-y-alcance) para más consejos sobre alcance.

**Nunca transmitas sin una antena conectada.** El firmware transmite un NodeInfo justo después del arranque.

## Cómo se mantiene la tabla correcta

En la versión en inglés de este documento, [`docs/en/frequencies.md`](../en/frequencies.md), la tabla de regiones está entre los comentarios HTML `<!-- regions:begin -->` y `<!-- regions:end -->`. La prueba nativa [`test/test_docs_regions`](../../test/test_docs_regions/test_main.cpp) lee ese archivo, analiza cada fila de la tabla y la compara con `regionAt(i)` en el mismo orden: el nombre, los límites de banda, la frecuencia LongFast calculada por `regionFreqMHz`, el límite de potencia, el ciclo de trabajo y el indicador de experimental. Se ejecuta con cada `pio test -e native` y, por tanto, en la CI en cada push y pull request. Si cambias `region.cpp`, actualiza la tabla en inglés en el mismo commit (el orden de las columnas importa para la prueba) y después las traducciones. Las traducciones no las comprueba la prueba.
