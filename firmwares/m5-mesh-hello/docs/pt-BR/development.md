> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/development.md).

# Desenvolvimento

Como compilar, testar e publicar o M5-Mesh-Hello. As convenções de código, o estilo de commit e o checklist de pull request estão em [CONTRIBUTING.md](../../CONTRIBUTING.md) (em inglês). Todos os comandos rodam a partir da pasta `firmwares/m5-mesh-hello/`.

## Ferramentas

| Ferramenta | Versão | Observações |
|---|---|---|
| Python | 3.10+ (a CI usa 3.12) | |
| PlatformIO Core | **6.2.0** (fixado na CI) | `python -m pip install "platformio==6.2.0"`, depois rode como `python -m platformio ...` (ou `pio ...` se estiver no `PATH`) |
| Plataforma | `espressif32@8.0.0` (Arduino) | Baixada pelo PlatformIO |
| Bibliotecas | M5Cardputer 1.1.1, M5Unified 0.3.0, M5GFX 0.3.0, RadioLib 7.8.1 | Fixadas em [`platformio.ini`](../../platformio.ini) |
| Criptografia | Subconjunto de rweather/Crypto | Incorporado em [`lib/crypto/`](../../lib/crypto/) (MIT) |
| Compilador do host para os testes nativos | GCC com C++17 | **Windows**: [WinLibs](https://winlibs.com/) (`winget install BrechtSanders.WinLibs.POSIX.UCRT`, coloque o `mingw64/bin` dele no `PATH`). **Linux**: `gcc`/`g++` (por exemplo `build-essential`). **macOS**: Xcode Command Line Tools ou GCC do Homebrew. |

No Windows com Git Bash, por exemplo:

```sh
PATH="/c/Users/<you>/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin:$PATH" \
  python -m platformio test -e native
```

## Compilação

```sh
python -m platformio run -e cardputer      # -> .pio/build/cardputer/firmware.bin
```

Instale o resultado com o M5Launcher (copie para o cartão SD) ou grave com `python -m platformio run -e cardputer -t upload`. Conecte a antena antes de o firmware rodar.

## Ambientes

| Ambiente | Finalidade |
|---|---|
| `cardputer` | Firmware do aparelho (ESP32-S3, Arduino) |
| `native` | Testes nativos Unity no PC. Compila só `src/mesh/` (o núcleo portátil) mais o teste. |
| `native_sanitize` | `native` com `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g`. As flags de link são adicionadas por [`tools/sanitize_link.py`](../../tools/sanitize_link.py). |

## Testes nativos

```sh
python -m platformio test -e native                       # all suites
python -m platformio test -e native -f test_region        # one suite
```

(O primeiro comando roda todas as suítes; o segundo, uma só.)

| Suíte | Cobre |
|---|---|
| `test_codec` | Codificação/decodificação do cabeçalho e de protobuf |
| `test_crypto` | AES-CTR, X25519, SHA-256, AES-CCM contra vetores de referência |
| `test_packet` | Montagem e interpretação de quadros, PKI, ACK/NAK, dedup |
| `test_region` | djb2, frequências de todas as regiões, hash do canal, limite de potência, tempo no ar, orçamento de duty cycle |
| `test_logic` | NodeDb, ChatStore, Greeter, Outbox |
| `test_app_logic` | Configurações (padrões, ida e volta do blob, validação, chave legada), montagem da saudação, conversão para exibição em CP437, funções de edição de texto, nomes de modelos de hardware, limitadores de frequência |
| `test_fuzz` | Robustez do caminho de recepção no estilo fuzzing (veja abaixo) |
| `test_docs_regions` | A tabela de regiões em [`docs/en/frequencies.md`](../en/frequencies.md) bate com `src/mesh/region.*` |

### Sanitizers (somente Linux/macOS)

```sh
python -m platformio test -e native_sanitize
```

ASan/UBSan precisam dos runtimes dos sanitizers, que o GCC/Clang fornecem no Linux e no macOS. O MinGW/WinLibs no Windows não tem nenhum, então esse ambiente não linka lá: use o WSL, ou confie na CI, que o roda a cada push e pull request com `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1` e `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

### Teste de fuzzing

`test/test_fuzz` passa **200000** entradas com semente fixa por `headerParse`, `pbDecodeData`/`pbDecodeUser`/`pbDecodeRoutingError`, `pkiDecrypt` e `parseFrame`. As entradas são quadros aleatórios e mutações de quadros válidos: inversão de bits, sobrescrita de bytes, inserções, remoções, truncamento, acréscimos. Ele verifica que nada trava, que nada é escrito fora das structs de saída (canários) e que todo resultado aceito é consistente. Cada entrada fica num buffer de heap de tamanho exato, então o ASan detecta leituras além do fim. Defina `M5MH_FUZZ_ITERS` no ambiente para mudar o número de iterações. A semente é fixa, então as execuções são reproduzíveis.

### Verificação da documentação

`test/test_docs_regions` interpreta a tabela entre `<!-- regions:begin -->` e `<!-- regions:end -->` em `docs/en/frequencies.md` e compara cada linha com o código: nome e ordem, limites da faixa, frequência LongFast, limite de potência, ciclo de trabalho e indicador de experimental. Se você mudar a tabela de regiões no código, atualize o documento no mesmo commit. O teste roda na suíte `native`, então a CI falha quando eles discordam. O teste procura o arquivo relativo à pasta do projeto, com uma alternativa relativa ao próprio caminho do fonte. Só a versão em inglês é verificada; as traduções em `docs/pt-BR/`, `docs/es/`, `docs/ru/` e `docs/ja/` não têm os marcadores.

### Verificação de links

```sh
python tools/check_links.py
```

[`tools/check_links.py`](../../tools/check_links.py) confere todo link relativo em `README*.md` e `docs/**/*.md`: o arquivo ou pasta de destino precisa existir, e um `#fragmento` precisa corresponder a um título no arquivo Markdown de destino (âncoras no estilo do GitHub). Links externos `http(s)` não são verificados. A CI o roda no job de testes nativos.

## Regenerando os vetores de teste

[`test/vectors.h`](../../test/vectors.h) é gerado. Não edite à mão.

```sh
python -m pip install cryptography
python tools/gen_vectors.py
```

O gerador é determinístico. A saída precisa ser idêntica byte a byte, a menos que você tenha mudado o gerador de propósito.

## Não rode testes no dispositivo

**Nunca rode `pio test` contra o Cardputer quando o M5Launcher estiver instalado.** O executor de testes no dispositivo do PlatformIO grava a própria imagem sobre toda a área de apps, o que sobrescreve o M5Launcher e todos os apps instalados por ele. Este projeto só testa nativamente. O comportamento no aparelho é verificado compilando, instalando pelo M5Launcher e testando em campo.

## CI

[`.github/workflows/m5-mesh-hello.yml`](../../../../.github/workflows/m5-mesh-hello.yml) roda a cada push e pull request que mexe nesta pasta ou no workflow, e por disparo manual. Permissões: `contents: read`. Todas as actions são fixadas por SHA completo de commit.

| Job | O que faz |
|---|---|
| Native tests | `python tools/check_links.py` (verificação de links), depois `pio test -e native` no ubuntu-latest (inclui a verificação da documentação) |
| Native tests (ASan + UBSan) | `pio test -e native_sanitize` |
| Device build | Verifica as imagens commitadas em `bin/` com `sha256sum -c SHA256SUMS`, roda `pio run -e cardputer` e sobe um artefato com as imagens da versão, o `firmware.bin` compilado pela CI e os checksums |

[`.github/workflows/codeql-m5-mesh-hello.yml`](../../../../.github/workflows/codeql-m5-mesh-hello.yml) roda o CodeQL (`c-cpp`, consultas `security-extended`, compilação manual dos programas de teste nativos) nos mesmos gatilhos e semanalmente. O repositório é público, então os resultados são enviados ao code scanning e aparecem na aba **Security** do repositório. Eles também ficam guardados como artefato SARIF (`codeql-results-m5-mesh-hello`), e **o job falha se houver qualquer achado**. Só a compilação dos testes nativos é analisada: o núcleo portável em `src/mesh/` e os testes. O código exclusivo do aparelho em `src/app/` e `src/hw/` não faz parte dessa compilação.

[`.github/dependabot.yml`](../../../../.github/dependabot.yml) propõe atualizações semanais das GitHub Actions fixadas.

## Publicando uma versão

1. Atualize o [`CHANGELOG.md`](../../CHANGELOG.md): mova as entradas de `Unreleased` para uma nova versão e data.
2. Rode os testes nativos e a compilação para o aparelho:
   ```sh
   python -m platformio test -e native
   python -m platformio run -e cardputer
   ```
3. Copie a imagem do aplicativo e monte a imagem completa da flash (`<framework-arduinoespressif32>` é `~/.platformio/packages/framework-arduinoespressif32`):
   ```sh
   cp .pio/build/cardputer/firmware.bin bin/M5-Mesh-Hello.bin
   esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
     0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
     0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
   ```
4. Gere de novo os checksums:
   ```sh
   cd bin && sha256sum *.bin > SHA256SUMS
   ```
5. Instale `bin/M5-Mesh-Hello.bin` pelo M5Launcher e faça um teste rápido no aparelho (inicialização do rádio, barra de status, Config, identidade inalterada após reiniciar).
6. Faça commit, push e espere a CI passar (o job de compilação para o aparelho verifica de novo o `SHA256SUMS`).
7. Crie a tag da versão como `m5-mesh-hello-v<versão>` (os links do CHANGELOG usam esse formato) e anexe as duas imagens e o `SHA256SUMS` à release do GitHub.

## Convenções de código

Veja [CONTRIBUTING.md](../../CONTRIBUTING.md#coding-conventions) (em inglês). Em resumo: `src/mesh/` continua portátil (sem headers Arduino/ESP-IDF), todo byte vindo do rádio é não confiável e tem os limites verificados, não há heap no caminho de RX, só funções de string com limite são usadas, segredos são apagados com `secureZero()` e nunca exibidos, toda função pública tem um comentário de documentação de uma linha, todo arquivo tem um cabeçalho SPDX e as dependências são fixadas. Mudanças na documentação começam em inglês (`docs/en/`). As traduções vêm depois.
