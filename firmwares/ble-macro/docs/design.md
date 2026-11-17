# Cardputer BLE Macro — design (curto)

## Intenção
Firmware próprio para o M5Stack Cardputer (ESP32-S3), para aprender e brincar. Ele vira um
**teclado + mouse Bluetooth** e roda uma macro em loop que dá para ver acontecendo no PC
(Windows, macOS ou Linux). Depois a ideia é evoluir para macros de teclado (abrir um site,
dar play em vídeo), por isso o teclado já entra agora.

## Objetivos
- Parear com o PC via Bluetooth LE como um dispositivo combo (teclado + mouse).
- Uma tecla do Cardputer liga e desliga a macro.
- Com a macro ligada, a cada 60 s:
  1. o cursor vai rápido para 3 pontos aleatórios da tela, um após o outro;
  2. o Cardputer manda 10 toques rápidos no **Shift esquerdo**.
- Repete sem parar até a macro ser desligada.

## Fora do escopo (agora)
- Cliques do mouse, digitação de texto, abrir sites (fica para depois).
- Posição exata na tela (mouse absoluto).
- Configurar a macro pelo próprio Cardputer.

## Restrições
- O Cardputer só tem BLE (não tem Bluetooth clássico).
- A tela do PC sempre tem pelo menos 1920×1080.
- Tem que funcionar nos 3 sistemas sem instalar nada no PC.

## Abordagens consideradas
- **A. Mouse relativo com "volta ao canto" (escolhida).** Antes de cada ponto, um movimento
  grande para cima e para a esquerda prende o cursor em (0,0); depois ele anda até um ponto
  sorteado dentro de 1920×1080. É um mouse comum, então funciona nos 3 sistemas, mas a
  aceleração do mouse deixa os pontos só aproximados (aceitável aqui).
- **B. Mouse absoluto (descritor tipo tablet).** Posição exata, mas o descritor HID é feito na
  mão e o comportamento no macOS é incerto. Fica como evolução se um dia precisar de clique
  preciso.
- **C. Firmware pronto (Bruce, etc.).** Descartado: o objetivo é ter um firmware próprio.

## Decisões do Jean
- Abordagem A.
- Combo teclado + mouse por BLE.
- 3 movimentos aleatórios rápidos e depois 10 toques rápidos no Shift esquerdo, a cada 1 minuto.

## Escolhas minhas (podem ser corrigidas)
- **Toolchain:** PlatformIO + framework Arduino, com a biblioteca `M5Cardputer` para tela e
  teclado. Uma biblioteca BLE combo baseada em NimBLE (teclado + mouse num só dispositivo);
  qual exatamente fica para o builder.
- **Nome Bluetooth:** "M5-Keyboard" (definido pelo Jean).
- **Tecla de liga/desliga:** Enter (`OK`) do Cardputer.
- **Temporização:** o primeiro ciclo roda logo ao ligar a macro e depois a cada 60 s.
  Desligar no meio de um ciclo interrompe o que falta.
- **Movimento "rápido":** relatórios de no máximo ±127 por eixo, poucos ms entre eles, cerca de
  300 ms parado em cada ponto para dar para ver. Pontos sorteados com margem de 100 px das bordas.
- **Shift:** cada toque fica cerca de 30 ms pressionado e 30 ms solto.
- **Sem conexão:** a macro não roda, a tela mostra "aguardando conexão" e o ciclo volta quando
  o PC reconectar.
- **Tela do Cardputer:** status do Bluetooth, macro ON/OFF, contagem regressiva para o próximo
  ciclo, número de ciclos já feitos e a dica da tecla.

## Para o builder decidir
- Biblioteca BLE exata e versões fixadas.
- Organização do código, de um jeito que facilite adicionar novas macros depois.
