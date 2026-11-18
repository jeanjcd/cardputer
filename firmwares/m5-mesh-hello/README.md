# M5-Mesh-Hello

Firmware para o **Cardputer-Adv + Cap LoRa-1262 (oficial M5)** que entra na rede **Meshtastic** como um nó comum (padrão **4stro-031** / **4S31**). Mostra os nós ao redor, conversa com eles por mensagem direta (DM criptografada com PKI), lê e escreve no canal público LongFast e tem uma **saudação automática** que cumprimenta cada nó novo **uma única vez**.

Não é o firmware oficial do Meshtastic: é um "Meshtastic mínimo" feito à mão (RadioLib + protocolo + criptografia), com interface própria e sem celular.

## Instalação (M5Launcher)

1. Copie `bin/M5-Mesh-Hello.bin` para o cartão SD e instale pelo **M5Launcher** (**SD → instalar**).
   Se o Launcher recusar, use `bin/M5-Mesh-Hello-merged.bin`, que é a imagem completa para gravar em `0x0`.
2. **Antes de ligar, rosqueie a antena no Cap.** Nunca transmita sem antena: o rádio pode queimar.
3. Ao ligar, o aparelho anuncia o nó na rede (NodeInfo com a chave pública) e começa a ouvir.

Na primeira vez é gerado um par de chaves X25519 com entropia de hardware. Ele fica na NVS e **nunca muda**: os nós Meshtastic guardam a primeira chave que ouvem de cada nó, então trocar a chave impediria que respondessem.

## Telas e teclas

| Tecla | Ação |
|---|---|
| `Tab` | troca de tela: **Nós → Conversa → Canal → Config** |
| `;` / `.` | sobe / desce nas listas (Nós, Config) |
| `Fn`+`;` / `Fn`+`.` | rola o histórico (Conversa, Canal) |
| `Enter` | abre a conversa / envia / edita |
| `` ` `` (Esc) | volta / cancela a edição |
| `Del` | apaga o último caractere |
| `A` | liga/desliga a saudação automática (tela Nós) |

Barra de status: `ANZ 919.875 | AUTO:ON | rx 42 | -97dBm` (região e frequência, saudação automática, pacotes recebidos, RSSI do último). Mostra `duty limit` quando a região limita o tempo de transmissão e a cota da hora acabou. Mensagens curtas (erros, "Config salva") aparecem em amarelo no lugar dela por 2,5 s.

- **Nós**: uma linha por nó, por exemplo `4S31 4stro-031   CARDPUTER_ADV  -97  2m*+`: nome curto, nome longo, modelo, RSSI (dBm), há quanto tempo foi ouvido, `*` = mensagem não lida, `+` = já cumprimentado. Nós em cinza ainda não mandaram a chave pública. `Enter` abre a conversa.
- **Conversa**: cabeçalho com ID `!xxxxxxxx`, nome, modelo e chip. Histórico (mais antigo em cima) e a caixa de texto embaixo, **já preenchida com a saudação**. Marcas nas suas mensagens:
  `~` na fila, `.` enviada, `v` confirmada (ACK), `?` aguardando a chave do nó, `x` falhou.
- **Canal**: mensagens públicas do LongFast (com o nome curto de quem mandou) e caixa de texto para mandar no canal.
- **Config**: região, potência, nome longo, nome curto, texto da saudação e "Enviar NodeInfo". Mostra a nossa ID e a impressão digital da chave pública (os 4 primeiros bytes). A chave privada nunca aparece.

O texto é limitado a 199 bytes. Acentos do português aparecem na tela quando a fonte tem o caractere (é, ç, ô...); os que ela não tem (ã, õ, emoji) aparecem como a letra sem acento ou `?`. **Isso é só na tela**: o texto enviado sai exatamente como foi escrito, em UTF-8.

## Região

Um rádio só escuta uma frequência por vez, então todos os nós precisam estar na mesma região. **Padrão: ANZ (919,875 MHz)**, que é onde a comunidade brasileira está hoje (meshbrasil.com). **BR_902 (903,875 MHz)** é a região oficial do Brasil no Meshtastic. Na tela Config, `Enter` em "Região" passa para a próxima região da lista:

| Região | LongFast | Potência máx. | Duty cycle |
|---|---|---|---|
| ANZ | 919,875 MHz | 30 dBm | — |
| BR_902 | 903,875 MHz | 30 dBm | — |
| US | 906,875 MHz | 30 dBm | — |
| EU_868 | 869,525 MHz | 27 dBm | 10 % |
| RU | 869,075 MHz | 20 dBm | — |
| KR | 922,875 MHz | 23 dBm | — |
| UA_868 | 868,375 MHz | 14 dBm | 1 % |
| MY_919 | 922,875 MHz | 27 dBm | — |
| SG_923 | 917,875 MHz | 20 dBm | — |
| PH_868 | 868,125 MHz | 14 dBm | — |
| PH_915 | 917,875 MHz | 24 dBm | — |
| IN *(exp)* | 865,875 MHz | 30 dBm | — |
| NZ_865 *(exp)* | 864,875 MHz | 36 dBm | — |
| NP_865 *(exp)* | 867,875 MHz | 30 dBm | — |
| KZ_863 *(exp)* | 866,875 MHz | 30 dBm | — |
| JP *(exp)* | 923,375 MHz | 13 dBm | — |
| TW *(exp)* | 923,875 MHz | 27 dBm | — |
| TH *(exp)* | 923,875 MHz | 27 dBm | 10 % |

- Os valores vêm da tabela de regiões do firmware oficial (`RadioInterface.cpp`), com a mesma fórmula de canal.
- **(exp)** = experimental: a frequência fica um pouco fora da faixa de 868–923 MHz para a qual o Cap é especificado (entre 860 e 868 MHz ou entre 923 e 925 MHz). O chip SX1262 sintoniza, mas o desempenho pode ser pior.
- Fora da lista: regiões em 433 MHz e 470–510 MHz (o Cap não cobre) e 2,4 GHz.
- **Potência efetiva** = o menor valor entre a configuração, o limite da região e 22 dBm (máximo do Cap). A tela Config mostra a efetiva quando ela é menor.
- **Duty cycle**: em EU_868 e TH (10 %) e UA_868 (1 %), o firmware soma o tempo de transmissão da última hora e segura os envios quando a cota acabaria (360 s ou 36 s por hora). As mensagens esperam na fila, não são descartadas.

## Antena

A antena do kit é sintonizada para 868 MHz. Em 919,875 MHz (ANZ) ela perde algo entre ~0,5 e 2 dB, uns 5–20 % do alcance. Funciona, mas não é o ideal. Em ordem do que mais ajuda:

1. **Altura e visada**: janela, laje, alguns metros acima do chão.
2. **Uma antena sintonizada em 915 MHz** (902–928), se for comprar. Não "wideband".
3. Antena **na vertical**, como a dos outros nós.
4. **Potência no máximo** (22 dBm, o padrão).

Com a antena de 868, BR_902 (903,875 MHz) fica mais perto da sintonia dela, mas a comunidade está em ANZ.

## Saudação automática

- Começa **desligada** a cada boot. Liga e desliga com `A` na tela Nós.
- Só cumprimenta nós cuja chave pública já recebemos (pelo NodeInfo deles). Ao ligar, entram na fila também os nós com chave ouvidos desde o boot.
- No máximo **1 saudação a cada 30 s**, fila de até 10.
- **Nunca cumprimenta o mesmo nó duas vezes**: a lista de cumprimentados fica na NVS (até 500 IDs) e sobrevive a reinícios. Mandar a saudação à mão também conta.
- Texto padrão: `Saudações de <nome longo>! 👋 (Cardputer-Adv)`. Ele acompanha o nome longo, a não ser que você edite o texto na Config (aí vale o seu, exatamente como escrito). Apagar o texto volta ao padrão.

## Como o nó se comporta na rede

- Papel `CLIENT_MUTE`: **não repassa** pacotes de outros nós. Hop limit 3 em tudo o que envia.
- NodeInfo: no boot (pedindo o NodeInfo dos vizinhos), a cada 3 h, ao trocar o nome, e pela opção "Enviar NodeInfo" (no máximo 1 a cada 30 s). Responde a pedidos de NodeInfo no máximo 1 vez a cada 12 h por nó.
- DM para um nó sem chave: a mensagem fica como `?` e mandamos o nosso NodeInfo direto para ele, pedindo o dele. Quando a chave chega, a mensagem sai.
- DMs recebidas com pedido de confirmação recebem ACK. Um ACK nosso marca `v`; um NAK (erro de roteamento) marca `x`.
- Uma transmissão por vez, com pelo menos 1,5 s entre elas.
- Os nomes e chaves dos nós com quem trocamos DM ficam na NVS (até 50) para as respostas funcionarem depois de reiniciar.

## Limitações (v1)

- Só o canal primário padrão (LongFast, chave `AQ==`). Sem outros canais, presets ou MQTT.
- Sem GPS/posição (o GNSS do Cap fica para a v2), sem Bluetooth/Wi-Fi e sem configuração pelo app do celular.
- Não assina pacotes (XEdDSA). A política padrão do Meshtastic aceita.
- Sem reenvio automático de DM: se o ACK não vier, a mensagem fica em `.`.
- Mensagens só na RAM (30 por conversa, 96 no total): somem ao reiniciar.
- Até 100 nós na lista (sai o mais antigo).
- Teclado: só caracteres ASCII.

## Compilar

PlatformIO, framework Arduino (espressif32 8.0.0, M5Cardputer 1.1.1, M5Unified/M5GFX 0.3.0, RadioLib 7.8.1):

```
python -m platformio run -e cardputer     # gera .pio/build/cardputer/firmware.bin
python -m platformio test -e native       # testes Unity no PC (protocolo, cripto, regiões, regras)
```

Os testes nativos precisam de um g++ no PATH (no Windows, por exemplo, o WinLibs/MinGW).

A imagem completa (`-merged.bin`) é montada com o esptool:

```
esptool --chip esp32s3 merge-bin -o bin/M5-Mesh-Hello-merged.bin \
  0x0 .pio/build/cardputer/bootloader.bin 0x8000 .pio/build/cardputer/partitions.bin \
  0xe000 <framework-arduinoespressif32>/tools/partitions/boot_app0.bin 0x10000 .pio/build/cardputer/firmware.bin
```

> Tela: na M5GFX atual, a rotação 0 do Cardputer-Adv já é paisagem (240×135). **Não** chame `setRotation(1)`.

## Código

| Pasta | Papel |
|---|---|
| `src/mesh/` | Núcleo portátil, sem Arduino: cabeçalho, protobuf, criptografia (AES-CTR, X25519, AES-256-CCM), montagem/leitura de pacotes, banco de nós, conversas, saudação, fila de DMs, regiões/airtime, configurações, texto |
| `src/hw/` | Rádio SX1262, NVS e identidade persistente |
| `src/app/` | Aplicação (laço principal, agendamento de TX) e interface |
| `test/` | Testes Unity nativos |

`docs/design.md` e `docs/plano.md` guardam o design e o plano originais.
