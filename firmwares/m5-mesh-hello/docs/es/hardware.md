> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/hardware.md).

# Hardware

M5-Mesh-Hello funciona en una combinación concreta:

- **M5Stack Cardputer-Adv** (ESP32-S3, pantalla de 240 × 135, teclado detrás de un controlador TCA8418).
- **M5Stack Cap LoRa-1262**: transceptor LoRa Semtech SX1262, banda nominal 868–923 MHz, hasta +22 dBm, conector de antena RP-SMA. También incluye un receptor GNSS ATGM336H, que la v1 no usa.
- Una **antena** en el conector RP-SMA del Cap. La antena del kit está sintonizada a 868 MHz (consulta [Antena y alcance](#antena-y-alcance)).

El Cardputer original (no Adv) y otros módulos LoRa no son compatibles: el mapa de pines, el expansor de E/S y el conmutador de antena son distintos.

> **Nunca transmitas sin antena.** El firmware envía su NodeInfo menos de un segundo después del arranque, así que enrosca la antena antes de encender el dispositivo.

## Mapa de pines

Según [`src/hw/radio.cpp`](../../src/hw/radio.cpp) y la documentación de M5Stack para el Cap LoRa-1262:

| Señal | Pin del ESP32-S3 | Notas |
|---|---|---|
| SX1262 NSS (chip select) | G5 | |
| SX1262 DIO1 (IRQ) | G4 | Interrupción al recibir un paquete; también se consulta su nivel (polling) |
| SX1262 RESET | G3 | |
| SX1262 BUSY | G6 | |
| SPI SCK | G40 | Compartido con la ranura microSD |
| SPI MISO | G39 | Compartido con la ranura microSD |
| SPI MOSI | G14 | Compartido con la ranura microSD |
| microSD CS | G12 | Se pone en nivel **alto** (deseleccionada) antes de iniciar la radio |
| I²C SDA / SCL (bus interno) | G8 / G9 | Compartido por el controlador del teclado, la IMU y el expansor de E/S del Cap |
| Expansor de E/S PI4IOE5V6408 | dirección I²C `0x43` | El puerto **P0** controla el conmutador de RF de la antena |
| UART del GNSS (ATGM336H) | ESP RX = G15, ESP TX = G13, 115200 baudios | **No se usa en v1** |

El bus LoRa/SD usa el periférico SPI2 (`FSPI`) del ESP32-S3. La pantalla usa SPI3.

## Conmutador de antena (PI4IOE5V6408, P0)

El conmutador de RF del Cap se habilita mediante el puerto P0 de un expansor de E/S PI4IOE5V6408 en el bus I²C interno (dirección `0x43`, 400 kHz). Al arrancar, el firmware:

1. inicializa el expansor;
2. configura P0 como salida, desactiva su modo de alta impedancia y lo pone en nivel **alto**;
3. vuelve a leer el registro de salida.

El Cap solo se considera **encontrado** si todos los pasos tienen éxito y P0 se lee en nivel alto. Si no, la barra de estado muestra `SEM CAP LoRa`. El firmware intenta igualmente iniciar el SX1262, pero sin el conmutador habilitado no se puede enviar ni recibir nada útil. La conmutación entre transmisión y recepción dentro del Cap la controla el pin DIO2 del SX1262 (el comportamiento por defecto de RadioLib, como en el ejemplo oficial de M5Stack).

`M5Cardputer.begin()` debe ejecutarse antes de iniciar la radio, porque es lo que activa el bus I²C interno. El expansor comparte ese bus con el controlador del teclado y la IMU.

## Configuración de la radio

| Parámetro | Valor |
|---|---|
| TCXO | **3.0 V**, regulador en modo **LDO** (como en el ejemplo oficial de M5Stack) |
| Protección contra sobrecorriente | 140 mA |
| Ganancia de RX | Reforzada (boosted), como hace Meshtastic, para mejorar la sensibilidad |
| Módem | LoRa, BW 250 kHz, SF 11, CR 4/5 (Meshtastic LongFast) |
| Sync word | `0x2B` (Meshtastic) |
| Preámbulo | 16 símbolos |
| Encabezado / CRC | Encabezado explícito, CRC del payload activado |
| Potencia de TX | 2–22 dBm, limitada por región (consulta [frequencies.md](frequencies.md#potencia-de-transmisión)) |
| Frecuencia | Calculada a partir de la región (consulta [frequencies.md](frequencies.md)) |

La recepción funciona por interrupción en DIO1. Como red de seguridad, el firmware también comprueba el nivel de DIO1 en cada vuelta del bucle y rearma la recepción tras 10 minutos sin ninguna RX ni TX. La transmisión es bloqueante: el bucle principal, y por tanto la interfaz, se detiene hasta que la trama ha salido (hasta unos 2.2 s para una trama de longitud máxima).

## Buses compartidos

- **SPI con la ranura microSD.** El CS de la tarjeta SD (G12) se mantiene en nivel alto para que la tarjeta ignore el bus mientras la radio lo usa. La v1 no usa la tarjeta SD durante el funcionamiento. M5Launcher la usa solo para instalar el firmware.
- **I²C con el teclado.** El expansor del conmutador de antena está en el mismo bus I²C interno que el controlador de teclado TCA8418. El firmware escribe en el expansor una sola vez, al arrancar.

## GNSS (no se usa en v1)

El Cap tiene un receptor GNSS ATGM336H en UART (ESP RX = G15, TX = G13, 115200 baudios). La v1 no inicia la UART y no envía posición. El uso compartido de posición está en la [hoja de ruta](roadmap.md). M5Stack advierte que las transmisiones LoRa interfieren con el receptor GNSS, algo que una v2 tendrá que tener en cuenta.

## Antena y alcance

La antena que viene con el Cap está sintonizada a **868 MHz**. En una región de 902–928 MHz sigue funcionando, con una pequeña pérdida:

- 919.875 MHz (ANZ) está un 6 % por encima de su frecuencia de sintonía, y 903.875 MHz (BR_902) un 4 % por encima. Lejos de su sintonía, la ROE (VSWR) de la antena aumenta.
- Con una ROE de 2:1 se refleja alrededor del 11 % de la potencia, una pérdida de ≈ **0.5 dB**. Con 3:1, ≈ **1.25 dB**.
- No existe ninguna medición pública de esta antena a 915 MHz. Cuenta con una pérdida de aproximadamente **0.5–2 dB**, que equivale más o menos a un 5–20 % del alcance. Sirve para largas distancias, pero no es lo ideal.

Lo que más importa para el alcance, por orden:

1. **Altura y línea de visión.** Elevar el dispositivo unos metros, o acercarse a una ventana, un balcón o una azotea, aporta mucho más que cualquier cambio de antena. Los edificios, las colinas y los cuerpos bloquean las señales de 900 MHz.
2. **Una antena sintonizada para tu banda.** Para las regiones de 902–928 MHz (ANZ, BR_902, US, ...), compra una antena sintonizada a **915 MHz**, no una de "banda ancha". Para las regiones de 863–870 MHz (EU_868, RU, ...), la antena de 868 MHz del kit es la adecuada.
3. **Orientación vertical**, la misma polarización que los demás nodos.
4. **La potencia máxima permitida.** El ajuste por defecto es 22 dBm. La región puede reducir la potencia efectiva.

Con la antena de 868 MHz del kit, BR_902 está algo más cerca de la sintonía de la antena que ANZ, pero la comunidad brasileña está en ANZ. Consulta [frequencies.md](frequencies.md#antenas-por-banda).

**Nunca transmitas sin una antena conectada.** Transmitir hacia un conector abierto refleja la potencia de vuelta al amplificador de potencia y puede dañarlo.

## Instalación con M5Launcher

Las imágenes de la versión están en [`bin/`](../../bin/):

| Archivo | Uso |
|---|---|
| `M5-Mesh-Hello.bin` | La imagen de la aplicación. Instálala con **M5Launcher** desde la tarjeta SD. |
| `M5-Mesh-Hello-merged.bin` | Una imagen completa de la flash (bootloader + tabla de particiones + boot_app0 + app) para grabar en el offset `0x0` con esptool o un grabador web. **Reemplaza todo el contenido de la flash, incluidos M5Launcher y sus aplicaciones instaladas**. Úsala solo si no usas M5Launcher. |
| `SHA256SUMS` | Sumas de comprobación SHA-256 de ambas imágenes |

Pasos:

1. Verifica la descarga: `sha256sum -c SHA256SUMS` dentro de `bin/` (en Windows: `certutil -hashfile M5-Mesh-Hello.bin SHA256` y compara). Consulta [security.md](security.md#verificar-una-versión).
2. Copia `M5-Mesh-Hello.bin` a la tarjeta microSD.
3. En M5Launcher, elige la tarjeta SD, selecciona el archivo e instálalo.
4. Enrosca la antena y después inicia la aplicación.

Notas sobre particiones:

- M5Launcher crea una partición de aplicación del tamaño del binario que instala. La v0.1.0 ocupa unos 590 KiB (603744 bytes). El firmware no usa ninguna partición SPIFFS/LittleFS, y todo lo que guarda va a la **NVS** (namespace `meshhello`, consulta [architecture.md](architecture.md#persistencia-nvs)).
- Para instalar una versión más nueva, primero **elimina la antigua** en M5Launcher (**PMan → Remove**) y luego instala el binario nuevo. Así se libera la partición y la nueva imagen recibe una del tamaño correcto.
- La configuración, el par de claves, la lista de saludados y los contactos guardados están en la NVS y normalmente sobreviven a la reinstalación de la aplicación. Que M5Launcher borre o no la NVS depende de la versión del Launcher y de las opciones usadas.

**No ejecutes `pio test` en un dispositivo con M5Launcher instalado.** El ejecutor de pruebas en dispositivo de PlatformIO graba su propia imagen y sobrescribe M5Launcher y sus aplicaciones. Todas las pruebas de este proyecto se ejecutan de forma nativa en el PC (consulta [development.md](development.md)).

## Nota sobre la pantalla

Con el M5GFX actual, la rotación 0 ya es horizontal (240 × 135) en el Cardputer-Adv. El firmware nunca llama a `setRotation(1)`.
