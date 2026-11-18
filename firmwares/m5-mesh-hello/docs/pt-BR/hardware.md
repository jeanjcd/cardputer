> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/hardware.md).

# Hardware

O M5-Mesh-Hello roda numa combinação específica:

- **M5Stack Cardputer-Adv** (ESP32-S3, tela de 240 × 135, teclado atrás de um controlador TCA8418).
- **M5Stack Cap LoRa-1262**: transceptor LoRa Semtech SX1262, faixa nominal de 868–923 MHz, até +22 dBm, conector de antena RP-SMA. Ele também traz um receptor GNSS ATGM336H, que a v1 não usa.
- Uma **antena** no conector RP-SMA do Cap. A antena do kit é sintonizada em 868 MHz (veja [Antena e alcance](#antena-e-alcance)).

O Cardputer original (não Adv) e outros módulos LoRa não são suportados: o mapa de pinos, o expansor de E/S e a chave de antena são diferentes.

> **Nunca transmita sem antena.** O firmware envia seu NodeInfo menos de um segundo após a inicialização, então rosqueie a antena antes de ligar o aparelho.

## Mapa de pinos

Segundo [`src/hw/radio.cpp`](../../src/hw/radio.cpp) e a documentação da M5Stack para o Cap LoRa-1262:

| Sinal | Pino do ESP32-S3 | Observações |
|---|---|---|
| SX1262 NSS (chip select) | G5 | |
| SX1262 DIO1 (IRQ) | G4 | Interrupção ao receber pacote; também lido por nível (polling) |
| SX1262 RESET | G3 | |
| SX1262 BUSY | G6 | |
| SPI SCK | G40 | Compartilhado com o slot microSD |
| SPI MISO | G39 | Compartilhado com o slot microSD |
| SPI MOSI | G14 | Compartilhado com o slot microSD |
| microSD CS | G12 | Levado a nível **alto** (desselecionado) antes de o rádio iniciar |
| I²C SDA / SCL (barramento interno) | G8 / G9 | Compartilhado pelo controlador de teclado, pela IMU e pelo expansor de E/S do Cap |
| Expansor de E/S PI4IOE5V6408 | endereço I²C `0x43` | A porta **P0** aciona a chave de RF da antena |
| UART do GNSS (ATGM336H) | ESP RX = G15, ESP TX = G13, 115200 baud | **Não usado na v1** |

O barramento LoRa/SD usa o periférico SPI2 (`FSPI`) do ESP32-S3. A tela usa o SPI3.

## Chave de antena (PI4IOE5V6408, P0)

A chave de RF do Cap é habilitada pela porta P0 de um expansor de E/S PI4IOE5V6408 no barramento I²C interno (endereço `0x43`, 400 kHz). Na inicialização, o firmware:

1. inicializa o expansor;
2. configura P0 como saída, desativa o modo de alta impedância e a leva a nível **alto**;
3. lê de volta o registrador de saída.

O Cap só é considerado **encontrado** se todos os passos derem certo e P0 for lido de volta em nível alto. Caso contrário, a barra de status mostra `SEM CAP LoRa`. O firmware ainda tenta iniciar o SX1262, mas sem a chave habilitada nada de útil pode ser enviado ou recebido. A comutação entre transmissão e recepção dentro do Cap é feita pelo pino DIO2 do SX1262 (o padrão do RadioLib, como no exemplo oficial da M5Stack).

`M5Cardputer.begin()` precisa rodar antes de o rádio iniciar, porque é ele que ativa o barramento I²C interno. O expansor compartilha esse barramento com o controlador de teclado e a IMU.

## Configuração do rádio

| Parâmetro | Valor |
|---|---|
| TCXO | **3.0 V**, regulador em modo **LDO** (como no exemplo oficial da M5Stack) |
| Proteção de sobrecorrente | 140 mA |
| Ganho de RX | Reforçado (boosted), como faz o Meshtastic, para melhor sensibilidade |
| Modem | LoRa, BW 250 kHz, SF 11, CR 4/5 (Meshtastic LongFast) |
| Sync word | `0x2B` (Meshtastic) |
| Preâmbulo | 16 símbolos |
| Cabeçalho / CRC | Cabeçalho explícito, CRC do payload ligado |
| Potência de TX | 2–22 dBm, limitada por região (veja [frequencies.md](frequencies.md#potência-de-transmissão)) |
| Frequência | Calculada a partir da região (veja [frequencies.md](frequencies.md)) |

A recepção é feita por interrupção no DIO1. Como rede de segurança, o firmware também verifica o nível do DIO1 a cada volta do loop e rearma a recepção após 10 minutos sem nenhum RX ou TX. A transmissão é bloqueante: o loop principal, e portanto a interface, fica parado até o quadro sair (até cerca de 2.2 s para um quadro de tamanho máximo).

## Barramentos compartilhados

- **SPI com o slot microSD.** O CS do cartão SD (G12) é mantido em nível alto para que o cartão ignore o barramento enquanto o rádio o usa. A v1 não usa o cartão SD durante o funcionamento. O M5Launcher o usa apenas para instalar o firmware.
- **I²C com o teclado.** O expansor da chave de antena fica no mesmo barramento I²C interno que o controlador de teclado TCA8418. O firmware escreve no expansor uma única vez, na inicialização.

## GNSS (não usado na v1)

O Cap tem um receptor GNSS ATGM336H na UART (ESP RX = G15, TX = G13, 115200 baud). A v1 não inicia a UART e não envia posição. O compartilhamento de posição está no [roadmap](roadmap.md). A M5Stack avisa que as transmissões LoRa interferem no receptor GNSS, o que uma v2 terá de levar em conta.

## Antena e alcance

A antena que vem com o Cap é sintonizada em **868 MHz**. Numa região de 902–928 MHz ela ainda funciona, com uma pequena perda:

- 919.875 MHz (ANZ) fica cerca de 6 % acima da frequência de sintonia, e 903.875 MHz (BR_902) cerca de 4 % acima. Longe da sintonia, a ROE (VSWR) da antena aumenta.
- Com ROE de 2:1, cerca de 11 % da potência é refletida, uma perda de ≈ **0.5 dB**. Com 3:1, ≈ **1.25 dB**.
- Não existe medição pública desta antena em 915 MHz. Espere uma perda de cerca de **0.5–2 dB**, o que corresponde a algo como 5–20 % do alcance. Funciona para longas distâncias, mas não é o ideal.

O que mais importa para o alcance, em ordem:

1. **Altura e linha de visada.** Elevar o aparelho alguns metros, ou ir para uma janela, varanda ou terraço, ganha muito mais do que qualquer troca de antena. Prédios, morros e corpos bloqueiam sinais de 900 MHz.
2. **Uma antena sintonizada na sua faixa.** Para as regiões de 902–928 MHz (ANZ, BR_902, US, ...), compre uma antena sintonizada em **915 MHz**, não uma "banda larga". Para as regiões de 863–870 MHz (EU_868, RU, ...), a antena de 868 MHz do kit é a certa.
3. **Orientação vertical**, a mesma polarização dos outros nós.
4. **A potência máxima permitida.** O padrão é 22 dBm. A região pode reduzir a potência efetiva.

Com a antena de 868 MHz do kit, a BR_902 fica um pouco mais perto da sintonia da antena do que a ANZ, mas a comunidade brasileira está na ANZ. Veja [frequencies.md](frequencies.md#antenas-por-faixa).

**Nunca transmita sem uma antena conectada.** Transmitir para um conector aberto reflete a potência de volta para o amplificador de potência e pode danificá-lo.

## Instalação com o M5Launcher

As imagens da versão estão em [`bin/`](../../bin/):

| Arquivo | Uso |
|---|---|
| `M5-Mesh-Hello.bin` | A imagem do aplicativo. Instale com o **M5Launcher** a partir do cartão SD. |
| `M5-Mesh-Hello-merged.bin` | Uma imagem completa da flash (bootloader + tabela de partições + boot_app0 + app) para gravar no offset `0x0` com o esptool ou um gravador web. Ela **substitui tudo na flash, inclusive o M5Launcher e os apps instalados por ele**. Use-a só se você não usa o M5Launcher. |
| `SHA256SUMS` | Checksums SHA-256 das duas imagens |

Passos:

1. Verifique o download: `sha256sum -c SHA256SUMS` dentro de `bin/` (no Windows: `certutil -hashfile M5-Mesh-Hello.bin SHA256` e compare). Veja [security.md](security.md#verificando-uma-versão).
2. Copie `M5-Mesh-Hello.bin` para o cartão microSD.
3. No M5Launcher, escolha o cartão SD, selecione o arquivo e instale.
4. Rosqueie a antena e depois inicie o app.

Observações sobre partições:

- O M5Launcher cria uma partição de aplicativo do tamanho do binário que instala. A v0.1.0 tem cerca de 590 KiB (603744 bytes). O firmware não usa partição SPIFFS/LittleFS, e tudo o que ele guarda vai para a **NVS** (namespace `meshhello`, veja [architecture.md](architecture.md#persistência-nvs)).
- Para instalar uma versão mais nova, primeiro **remova a antiga** no M5Launcher (**PMan → Remove**) e depois instale o novo binário. Isso libera a partição para que a nova imagem receba uma do tamanho certo.
- As configurações, o par de chaves, a lista de cumprimentados e os contatos salvos ficam na NVS e normalmente sobrevivem à reinstalação do app. Se o M5Launcher apaga ou não a NVS depende da versão do Launcher e das opções usadas.

**Não rode `pio test` num aparelho com o M5Launcher instalado.** O executor de testes no dispositivo do PlatformIO grava a própria imagem e sobrescreve o M5Launcher e os apps dele. Todos os testes deste projeto rodam nativamente no PC (veja [development.md](development.md)).

## Nota sobre a tela

Com o M5GFX atual, a rotação 0 já é paisagem (240 × 135) no Cardputer-Adv. O firmware nunca chama `setRotation(1)`.
