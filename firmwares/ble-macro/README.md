# ble-macro (M5-Keyboard)

Firmware para o Cardputer-Adv que vira um **teclado + mouse Bluetooth** e roda uma macro em loop.

Com a macro ligada, a cada 60 s:
1. o cursor vai rápido para 3 pontos aleatórios da tela (área de 1920×1080, com 100 px de margem);
2. o Shift esquerdo é tocado 10 vezes seguidas.

## Uso

1. Instale `bin/M5-Keyboard-Macro.bin` pelo **M5Launcher**: copie para o SD e abra **SD → instalar**.
   Se o Launcher recusar, use `bin/M5-Keyboard-Macro-merged.bin`, que é a imagem completa para gravar em `0x0`.
2. No PC, pareie o dispositivo **M5-Keyboard** (Windows, macOS ou Linux).
3. **Enter** no Cardputer liga e desliga a macro. A tela mostra o status do Bluetooth, se a macro está ON/OFF, quanto falta para o próximo ciclo e quantos ciclos já rodaram.

Comportamento:
- Sem conexão, a macro fica pausada. Quando o PC reconecta, um ciclo começa na hora.
- Desligar no meio de um ciclo interrompe na hora e solta todas as teclas, para o Shift não ficar preso.
- Os pontos são aproximados, porque a aceleração do mouse do sistema distorce os movimentos. Antes de cada ponto, o cursor é jogado contra o canto superior esquerdo, que serve de referência.

## Compilar

PlatformIO, framework Arduino:

```
python -m platformio run -e cardputer          # gera .pio/build/cardputer/firmware.bin
python -m platformio test -e cardputer         # testes Unity rodando no próprio Cardputer (sobrescreve o firmware instalado)
```

Ajuste `upload_port`/`test_port` no `platformio.ini`, que está em `COM7`.

Versões que funcionam: espressif32 8.0.0, Arduino core 3.3.12, M5Cardputer 1.1.1 e **NimBLE-Arduino 2.x**. A versão 1.4.x conflita com o core 3 e faz o firmware reiniciar em loop.

> Tela: na M5GFX atual, a rotação 0 do Cardputer/ADV já é paisagem (240×135). **Não** chame `setRotation(1)`, porque deixa a interface em pé.

## Código

| Arquivo | Papel |
|---|---|
| `src/macro_actions.*` | Monta a lista de ações de um ciclo (movimentos ≤127 por relatório, esperas, toques de Shift) |
| `src/macro_runner.*` | Executa as ações sem bloquear: intervalo de 60 s, liga/desliga, pausa sem conexão |
| `src/hid_output.h` | Interface abstrata de saída HID (permite trocar o Bluetooth por um fake nos testes) |
| `src/ble_hid.*` | Dispositivo BLE HID combo via NimBLE: report 1 = teclado, report 2 = mouse relativo |
| `src/ui.*` | Tela do Cardputer |
| `test/test_macro/` | 11 testes Unity da lógica da macro |

Para criar outra macro (abrir um site, dar play em vídeo...), monte outra lista de `Action` em `macro_actions.cpp`. Para digitar texto, falta adicionar teclas comuns ao `HidOutput`.

`docs/design.md` e `docs/plano.md` guardam o design e o plano originais. Eles citam NimBLE 1.4 e `setRotation(1)`, que foram corrigidos depois, como descrito acima.
