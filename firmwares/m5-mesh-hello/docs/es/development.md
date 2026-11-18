> Traducido del inglés; en caso de duda, prevalece la [versión en inglés](../en/development.md).

# Desarrollo

Cómo compilar, probar y publicar M5-Mesh-Hello. Las convenciones de código, el estilo de los commits y la lista de comprobación de los pull requests están en [CONTRIBUTING.md](../../CONTRIBUTING.md) (en inglés). Todos los comandos se ejecutan desde la carpeta `firmwares/m5-mesh-hello/`.

## Herramientas

| Herramienta | Versión | Notas |
|---|---|---|
| Python | 3.10+ (la CI usa 3.12) | |
| PlatformIO Core | **6.2.0** (fijada en la CI) | `python -m pip install "platformio==6.2.0"`, y luego ejecútalo como `python -m platformio ...` (o `pio ...` si está en el `PATH`) |
| Plataforma | `espressif32@8.0.0` (Arduino) | La descarga PlatformIO |
| Bibliotecas | M5Cardputer 1.1.1, M5Unified 0.3.0, M5GFX 0.3.0, RadioLib 7.8.1 | Fijadas en [`platformio.ini`](../../platformio.ini) |
| Criptografía | Subconjunto de rweather/Crypto | Incluido en [`lib/crypto/`](../../lib/crypto/) (MIT) |
| Compilador del host para las pruebas nativas | GCC con C++17 | **Windows**: [WinLibs](https://winlibs.com/) (`winget install BrechtSanders.WinLibs.POSIX.UCRT`, añade su `mingw64/bin` al `PATH`). **Linux**: `gcc`/`g++` (p. ej. `build-essential`). **macOS**: Xcode Command Line Tools o GCC de Homebrew. |

En Windows con Git Bash, por ejemplo:

```sh
PATH="/c/Users/<you>/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH" \
  python -m platformio test -e native
```

## Compilación

```sh
python -m platformio run -e cardputer      # -> .pio/build/cardputer/firmware.bin
```

Instala el resultado con M5Launcher (cópialo a la tarjeta SD) o grábalo con `python -m platformio run -e cardputer -t upload`. Conecta la antena antes de que se ejecute el firmware.

## Entornos

| Entorno | Propósito |
|---|---|
| `cardputer` | Firmware del dispositivo (ESP32-S3, Arduino) |
| `native` | Pruebas nativas de Unity en el PC. Compila solo `src/mesh/` (el núcleo portable) más la prueba. |
| `native_sanitize` | `native` con `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g`. Las opciones de enlazado las añade [`tools/sanitize_link.py`](../../tools/sanitize_link.py). |

## Pruebas nativas

```sh
python -m platformio test -e native                       # all suites
python -m platformio test -e native -f test_region        # one suite
```

(El primer comando ejecuta todas las suites; el segundo, solo una.)

| Suite | Cubre |
|---|---|
| `test_codec` | Codificación/decodificación del encabezado y de protobuf |
| `test_crypto` | AES-CTR, X25519, SHA-256, AES-CCM frente a vectores de referencia |
| `test_packet` | Construcción y análisis de tramas, PKI, ACK/NAK, deduplicación |
| `test_region` | djb2, frecuencias de todas las regiones, hash del canal, límite de potencia, tiempo en el aire, presupuesto de ciclo de trabajo |
| `test_logic` | NodeDb, ChatStore, Greeter, Outbox |
| `test_app_logic` | Configuración (valores por defecto, ida y vuelta del blob, validación, clave heredada), composición del saludo, conversión a CP437 para la pantalla, utilidades de edición de texto, nombres de modelos de hardware, limitadores de frecuencia |
| `test_fuzz` | Robustez de la ruta de recepción al estilo fuzzing (ver más abajo) |
| `test_docs_regions` | La tabla de regiones de [`docs/en/frequencies.md`](../en/frequencies.md) coincide con `src/mesh/region.*` |

### Sanitizers (solo Linux/macOS)

```sh
python -m platformio test -e native_sanitize
```

ASan/UBSan necesitan los runtimes de los sanitizers, que GCC/Clang proporcionan en Linux y macOS. MinGW/WinLibs en Windows no tiene ninguno, así que este entorno no enlaza allí: usa WSL o confía en la CI, que lo ejecuta en cada push y pull request con `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1` y `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

### Prueba de fuzzing

`test/test_fuzz` pasa **200000** entradas con semilla fija por `headerParse`, `pbDecodeData`/`pbDecodeUser`/`pbDecodeRoutingError`, `pkiDecrypt` y `parseFrame`. Las entradas son tramas aleatorias y mutaciones de tramas válidas: inversión de bits, sobrescritura de bytes, inserciones, eliminaciones, truncamiento, añadidos. Comprueba que nada se cuelga, que no se escribe nada fuera de las estructuras de salida (canarios) y que todo resultado aceptado es coherente. Cada entrada está en un búfer de heap de tamaño exacto, así que ASan detecta las lecturas fuera de límites. Define `M5MH_FUZZ_ITERS` en el entorno para cambiar el número de iteraciones. La semilla es fija, así que las ejecuciones son reproducibles.

### Verificación de la documentación

`test/test_docs_regions` analiza la tabla entre `<!-- regions:begin -->` y `<!-- regions:end -->` de `docs/en/frequencies.md` y compara cada fila con el código: nombre y orden, límites de banda, frecuencia LongFast, límite de potencia, ciclo de trabajo e indicador de experimental. Si cambias la tabla de regiones en el código, actualiza el documento en el mismo commit. La prueba se ejecuta en la suite `native`, así que la CI falla cuando no coinciden. La prueba busca el archivo relativo a la carpeta del proyecto, con una alternativa relativa a la ruta de su propio código fuente. Solo se comprueba la versión en inglés; las traducciones de `docs/pt-BR/`, `docs/es/` y `docs/ru/` no tienen los marcadores.

### Verificación de enlaces

```sh
python tools/check_links.py
```

[`tools/check_links.py`](../../tools/check_links.py) comprueba cada enlace relativo de `README*.md` y `docs/**/*.md`: el archivo o carpeta de destino debe existir, y un `#fragmento` debe corresponder a un encabezado del archivo Markdown de destino (anclas al estilo de GitHub). Los enlaces externos `http(s)` no se comprueban. La CI lo ejecuta en el job de pruebas nativas.

## Regenerar los vectores de prueba

[`test/vectors.h`](../../test/vectors.h) es un archivo generado. No lo edites a mano.

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

El generador es determinista. La salida debe ser idéntica byte a byte, salvo que hayas cambiado el generador a propósito.

## No ejecutes pruebas en el dispositivo

**No ejecutes nunca `pio test` contra el Cardputer cuando M5Launcher esté instalado.** El ejecutor de pruebas en dispositivo de PlatformIO graba su propia imagen sobre toda la zona de aplicaciones, lo que sobrescribe M5Launcher y todas las aplicaciones instaladas con él. Este proyecto solo hace pruebas nativas. El comportamiento en el dispositivo se verifica compilando, instalando con M5Launcher y probando en campo.

## CI

[`.github/workflows/m5-mesh-hello.yml`](../../../../.github/workflows/m5-mesh-hello.yml) se ejecuta en cada push y pull request que toca esta carpeta o el workflow, y al lanzarlo manualmente. Permisos: `contents: read`. Todas las actions están fijadas al SHA completo de un commit.

| Job | Qué hace |
|---|---|
| Native tests | `python tools/check_links.py` (verificación de enlaces) y luego `pio test -e native` en ubuntu-latest (incluye la verificación de la documentación) |
| Native tests (ASan + UBSan) | `pio test -e native_sanitize` |
| Device build | Verifica las imágenes de `bin/` incluidas en el repositorio con `sha256sum -c SHA256SUMS`, ejecuta `pio run -e cardputer` y sube un artefacto con las imágenes de la versión, el `firmware.bin` compilado por la CI y las sumas de comprobación |

[`.github/workflows/codeql-m5-mesh-hello.yml`](../../../../.github/workflows/codeql-m5-mesh-hello.yml) ejecuta CodeQL (`c-cpp`, consultas `security-extended`, compilación manual de los programas de prueba nativos) con los mismos disparadores y cada semana. El repositorio es público, así que los resultados se suben a code scanning y aparecen en la pestaña **Security** del repositorio. También se guardan como artefacto SARIF (`codeql-results-m5-mesh-hello`), y **el job falla si hay cualquier hallazgo**. Solo se analiza la compilación de las pruebas nativas: el núcleo portable de `src/mesh/` y las pruebas. El código exclusivo del dispositivo en `src/app/` y `src/hw/` no forma parte de esa compilación.

[`.github/dependabot.yml`](../../../../.github/dependabot.yml) propone actualizaciones semanales de las GitHub Actions fijadas.

## Publicar una versión

1. Actualiza [`CHANGELOG.md`](../../CHANGELOG.md): mueve las entradas de `Unreleased` a una nueva versión con su fecha.
2. Ejecuta las pruebas nativas y la compilación para el dispositivo:
   ```sh
   python -m platformio test -e native
   python -m platformio run -e cardputer
   ```
3. Copia la imagen de la aplicación y construye la imagen completa de la flash (`<framework-arduinoespressif32>` es `~/.platformio/packages/framework-arduinoespressif32`):
   ```sh
   cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
   esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
     0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
     0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
   ```
4. Regenera las sumas de comprobación:
   ```sh
   cd bin && sha256sum *.bin > SHA256SUMS
   ```
5. Instala `bin/M5-Mesh-Hello.bin` con M5Launcher y haz una prueba rápida en el dispositivo (inicio de la radio, barra de estado, Config, identidad sin cambios tras reiniciar).
6. Haz commit, push y espera a que pase la CI (el job de compilación para el dispositivo vuelve a verificar `SHA256SUMS`).
7. Etiqueta la versión como `m5-mesh-hello-v<versión>` (los enlaces del CHANGELOG usan ese formato) y adjunta las dos imágenes y `SHA256SUMS` a la release de GitHub.

## Convenciones de código

Consulta [CONTRIBUTING.md](../../CONTRIBUTING.md#coding-conventions) (en inglés). En resumen: `src/mesh/` sigue siendo portable (sin cabeceras de Arduino/ESP-IDF), cada byte que llega de la radio es no confiable y se comprueban sus límites, no hay heap en la ruta de RX, solo se usan funciones de cadena con límites, los secretos se borran con `secureZero()` y nunca se muestran, cada función pública tiene un comentario de documentación de una línea, cada archivo tiene una cabecera SPDX y las dependencias están fijadas. Los cambios de documentación empiezan en inglés (`docs/en/`). Las traducciones vienen después.
