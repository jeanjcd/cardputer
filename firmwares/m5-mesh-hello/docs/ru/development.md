> Перевод с английского; в случае расхождений действует [английская версия](../en/development.md).

# Разработка

Как собирать, тестировать и выпускать M5-Mesh-Hello. Соглашения по коду, стиль коммитов и чек-лист для pull request описаны в [CONTRIBUTING.md](../../CONTRIBUTING.md) (на английском). Все команды выполняются из папки `firmwares/m5-mesh-hello/`.

## Инструменты

| Инструмент | Версия | Примечания |
|---|---|---|
| Python | 3.10+ (в CI 3.12) | |
| PlatformIO Core | **6.2.0** (зафиксирована в CI) | `python -m pip install "platformio==6.2.0"`, затем запускайте как `python -m platformio ...` (или `pio ...`, если он в `PATH`) |
| Платформа | `espressif32@8.0.0` (Arduino) | Загружается PlatformIO |
| Библиотеки | M5Cardputer 1.1.1, M5Unified 0.3.0, M5GFX 0.3.0, RadioLib 7.8.1 | Зафиксированы в [`platformio.ini`](../../platformio.ini) |
| Криптография | Подмножество rweather/Crypto | Встроено в [`lib/crypto/`](../../lib/crypto/) (MIT) |
| Компилятор хоста для нативных тестов | GCC с C++17 | **Windows**: [WinLibs](https://winlibs.com/) (`winget install BrechtSanders.WinLibs.POSIX.UCRT`, добавьте его `mingw64/bin` в `PATH`). **Linux**: `gcc`/`g++` (например, `build-essential`). **macOS**: Xcode Command Line Tools или GCC из Homebrew. |

Например, в Windows с Git Bash:

```sh
PATH="/c/Users/<you>/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH" \
  python -m platformio test -e native
```

## Сборка

```sh
python -m platformio run -e cardputer      # -> .pio/build/cardputer/firmware.bin
```

Установите результат через M5Launcher (скопируйте на карту SD) или прошейте командой `python -m platformio run -e cardputer -t upload`. Подключите антенну до запуска прошивки.

## Окружения

| Окружение | Назначение |
|---|---|
| `cardputer` | Прошивка устройства (ESP32-S3, Arduino) |
| `native` | Нативные тесты Unity на ПК. Собирается только `src/mesh/` (переносимое ядро) плюс сам тест. |
| `native_sanitize` | `native` с `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g`. Флаги компоновки добавляет [`tools/sanitize_link.py`](../../tools/sanitize_link.py). |

## Нативные тесты

```sh
python -m platformio test -e native                       # all suites
python -m platformio test -e native -f test_region        # one suite
```

(Первая команда запускает все наборы, вторая — один.)

| Набор | Что проверяет |
|---|---|
| `test_codec` | Кодирование/декодирование заголовка и protobuf |
| `test_crypto` | AES-CTR, X25519, SHA-256, AES-CCM на эталонных векторах |
| `test_packet` | Сборка и разбор кадров, PKI, ACK/NAK, дедупликация |
| `test_region` | djb2, частоты всех регионов, хеш канала, ограничение мощности, время в эфире, бюджет рабочего цикла |
| `test_logic` | NodeDb, ChatStore, Greeter, Outbox |
| `test_app_logic` | Настройки (значения по умолчанию, сохранение и загрузка blob, проверка, устаревший ключ), составление приветствия, преобразование в CP437 для дисплея, функции редактирования текста, названия моделей оборудования, ограничители частоты |
| `test_fuzz` | Устойчивость пути приёма в стиле фаззинга (см. ниже) |
| `test_docs_regions` | Таблица регионов в [`docs/en/frequencies.md`](../en/frequencies.md) совпадает с `src/mesh/region.*` |

### Санитайзеры (только Linux/macOS)

```sh
python -m platformio test -e native_sanitize
```

ASan/UBSan нужны библиотеки времени выполнения санитайзеров, которые GCC/Clang предоставляют в Linux и macOS. В MinGW/WinLibs под Windows их нет, поэтому там это окружение не компонуется: используйте WSL или положитесь на CI, где оно запускается при каждом push и pull request с `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1` и `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

### Фаззинг-тест

`test/test_fuzz` прогоняет **200000** входов с фиксированным зерном через `headerParse`, `pbDecodeData`/`pbDecodeUser`/`pbDecodeRoutingError`, `pkiDecrypt` и `parseFrame`. Входы — случайные кадры и мутации корректных: инверсия битов, перезапись байтов, вставки, удаления, обрезка, дописывание. Тест проверяет, что ничего не падает, ничего не пишется за пределы выходных структур (канарейки) и каждый принятый результат непротиворечив. Каждый вход лежит в буфере в куче точного размера, поэтому ASan ловит чтение за границами. Задайте `M5MH_FUZZ_ITERS` в окружении, чтобы изменить число итераций. Зерно фиксировано, поэтому прогоны воспроизводимы.

### Проверка документации

`test/test_docs_regions` разбирает таблицу между `<!-- regions:begin -->` и `<!-- regions:end -->` в `docs/en/frequencies.md` и сравнивает каждую строку с кодом: имя и порядок, границы диапазона, частоту LongFast, лимит мощности, рабочий цикл и признак экспериментального региона. Если вы меняете таблицу регионов в коде, обновите документ в том же коммите. Тест входит в набор `native`, поэтому CI падает при расхождении. Тест ищет файл относительно папки проекта, а запасной путь — относительно пути к собственному исходнику. Проверяется только английская версия; в переводах (`docs/pt-BR/`, `docs/es/`, `docs/ru/`) маркеров нет.

### Проверка ссылок

```sh
python tools/check_links.py
```

[`tools/check_links.py`](../../tools/check_links.py) проверяет каждую относительную ссылку в `README*.md` и `docs/**/*.md`: целевой файл или папка должны существовать, а `#фрагмент` должен соответствовать заголовку в целевом файле Markdown (якоря в стиле GitHub). Внешние ссылки `http(s)` не проверяются. CI запускает скрипт в задании нативных тестов.

## Перегенерация тестовых векторов

[`test/vectors.h`](../../test/vectors.h) генерируется автоматически. Не редактируйте его вручную.

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

Генератор детерминирован. Результат должен совпадать побайтно, если только вы намеренно не меняли генератор.

## Не запускайте тесты на устройстве

**Никогда не запускайте `pio test` на Cardputer с установленным M5Launcher.** Механизм запуска тестов PlatformIO на устройстве прошивает собственный образ поверх всей области приложений, затирая M5Launcher и все приложения, установленные через него. В этом проекте тесты выполняются только нативно. Поведение на устройстве проверяется сборкой, установкой через M5Launcher и полевыми испытаниями.

## CI

[`.github/workflows/m5-mesh-hello.yml`](../../../../.github/workflows/m5-mesh-hello.yml) запускается при каждом push и pull request, затрагивающем эту папку или сам workflow, а также вручную. Права: `contents: read`. Все actions зафиксированы по полному SHA коммита.

| Задание | Что делает |
|---|---|
| Native tests | `python tools/check_links.py` (проверка ссылок), затем `pio test -e native` на ubuntu-latest (включая проверку документации) |
| Native tests (ASan + UBSan) | `pio test -e native_sanitize` |
| Device build | Проверяет закоммиченные образы в `bin/` командой `sha256sum -c SHA256SUMS`, выполняет `pio run -e cardputer` и загружает артефакт с образами релиза, собранным в CI `firmware.bin` и контрольными суммами |

[`.github/workflows/codeql-m5-mesh-hello.yml`](../../../../.github/workflows/codeql-m5-mesh-hello.yml) запускает CodeQL (`c-cpp`, запросы `security-extended`, ручная сборка нативных тестовых программ) по тем же триггерам и еженедельно. Репозиторий приватный и без GitHub Advanced Security, поэтому результаты нельзя загрузить в code scanning. Они сохраняются как артефакт SARIF (`codeql-results-m5-mesh-hello`), и **задание падает при любой находке**. Когда репозиторий станет публичным, установите `upload: always` и добавьте `security-events: write` (см. комментарий в workflow).

[`.github/dependabot.yml`](../../../../.github/dependabot.yml) еженедельно предлагает обновления зафиксированных GitHub Actions.

## Выпуск релиза

1. Обновите [`CHANGELOG.md`](../../CHANGELOG.md): перенесите записи из `Unreleased` под новую версию и дату.
2. Запустите нативные тесты и сборку для устройства:
   ```sh
   python -m platformio test -e native
   python -m platformio run -e cardputer
   ```
3. Скопируйте образ приложения и соберите объединённый полный образ флеш-памяти (`<framework-arduinoespressif32>` — это `~/.platformio/packages/framework-arduinoespressif32`):
   ```sh
   cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
   esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
     0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
     0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
   ```
4. Пересчитайте контрольные суммы:
   ```sh
   cd bin && sha256sum *.bin > SHA256SUMS
   ```
5. Установите `bin/M5-Mesh-Hello.bin` через M5Launcher и проведите быструю проверку на устройстве (инициализация радио, строка состояния, Config, идентичность не изменилась после перезагрузки).
6. Сделайте commit и push и дождитесь прохождения CI (задание сборки для устройства повторно проверяет `SHA256SUMS`).
7. Пометьте релиз тегом `m5-mesh-hello-v<версия>` (ссылки в CHANGELOG используют этот формат) и приложите к релизу на GitHub оба образа и `SHA256SUMS`.

## Соглашения по коду

См. [CONTRIBUTING.md](../../CONTRIBUTING.md#coding-conventions) (на английском). Кратко: `src/mesh/` остаётся переносимым (без заголовков Arduino/ESP-IDF), каждый байт из эфира считается недоверенным и проверяется на границы, в пути приёма нет кучи, используются только строковые функции с ограничением длины, секреты затираются `secureZero()` и никогда не показываются, у каждой публичной функции есть однострочный документирующий комментарий, у каждого файла — заголовок SPDX, зависимости зафиксированы. Изменения документации начинаются с английской версии (`docs/en/`). Переводы обновляются следом.
