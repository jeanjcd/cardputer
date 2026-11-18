> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/frequencies.md).

# Regiões e frequências

Tudo sobre as regiões de rádio, frequências, potência de transmissão e ciclo de trabalho (duty cycle) no M5-Mesh-Hello v0.1.0. Todos os valores vêm de [`src/mesh/region.h`](../../src/mesh/region.h) e [`src/mesh/region.cpp`](../../src/mesh/region.cpp). Um teste nativo, `test/test_docs_regions`, confere a tabela de regiões da **versão em inglês** deste documento contra o código a cada execução da CI (veja [Mantendo a tabela correta](#mantendo-a-tabela-correta)).

> **Nota legal.** Use sempre a região do país onde você está fisicamente. Você é responsável por seguir as normas de rádio locais (frequência, potência, ciclo de trabalho, ganho de antena). Este documento é informação técnica, não aconselhamento jurídico.

## O que é uma região Meshtastic

Um rádio LoRa escuta exatamente uma frequência por vez. Para dois nós Meshtastic conversarem, eles precisam usar a mesma frequência e os mesmos parâmetros de modem. O Meshtastic define uma **região** para cada área regulatória. Uma região é uma faixa de frequências (início e fim), um limite de ciclo de trabalho, um limite de potência e um espaçamento entre canais. Um nó pega a região, o nome do canal (aqui `LongFast`) e a largura de banda e calcula a partir deles a frequência central real, com uma fórmula fixa. Assim, todo nó na mesma região com o canal padrão cai na mesma frequência, sem nenhuma coordenação.

O M5-Mesh-Hello usa sempre o preset **LongFast** (largura de banda 250 kHz, spreading factor 11, coding rate 4/5, sync word `0x2B`, preâmbulo de 16 símbolos, cabeçalho explícito, CRC ligado) no canal primário padrão chamado `LongFast`. A região é a única configuração de rádio que você escolhe. Mude-a na tela **Config** (`Região`; Enter percorre a lista). O padrão é **ANZ**.

## Regiões suportadas

Estas são as regiões cuja frequência LongFast fica dentro da faixa nominal do Cap LoRa-1262, de 868–923 MHz. As regiões **experimentais** têm frequência LongFast em [860, 868) ou (923, 925] MHz, logo fora da especificação do Cap. A tela Config as lista nesta ordem.

A tabela oficial, verificada pela CI, é a da [versão em inglês](../en/frequencies.md#supported-regions). Esta é uma cópia traduzida.

| Região | Países / uso | Faixa (MHz) | LongFast (MHz) | Potência máx. (dBm) | Duty cycle (%) | Experimental |
|---|---|---|---|---|---|---|
| ANZ | Austrália, Nova Zelândia; também usada pela comunidade brasileira | 915.0 – 928.0 | 919.875 | 30 | 100 | não |
| BR_902 | Brasil (região oficial do Meshtastic) | 902.0 – 907.5 | 903.875 | 30 | 100 | não |
| US | Estados Unidos (e outros países que usam 902–928 MHz) | 902.0 – 928.0 | 906.875 | 30 | 100 | não |
| EU_868 | União Europeia (faixa SRD de 868 MHz) | 869.4 – 869.65 | 869.525 | 27 | 10 | não |
| RU | Rússia | 868.7 – 869.2 | 869.075 | 20 | 100 | não |
| KR | Coreia do Sul | 920.0 – 923.0 | 922.875 | 23 | 100 | não |
| UA_868 | Ucrânia | 868.0 – 868.6 | 868.375 | 14 | 1 | não |
| MY_919 | Malásia | 919.0 – 924.0 | 922.875 | 27 | 100 | não |
| SG_923 | Singapura | 917.0 – 925.0 | 917.875 | 20 | 100 | não |
| PH_868 | Filipinas (868 MHz) | 868.0 – 869.4 | 868.125 | 14 | 100 | não |
| PH_915 | Filipinas (915 MHz) | 915.0 – 918.0 | 917.875 | 24 | 100 | não |
| IN | Índia | 865.0 – 867.0 | 865.875 | 30 | 100 | sim |
| NZ_865 | Nova Zelândia (865 MHz) | 864.0 – 868.0 | 864.875 | 36 | 100 | sim |
| NP_865 | Nepal | 865.0 – 868.0 | 867.875 | 30 | 100 | sim |
| KZ_863 | Cazaquistão | 863.0 – 868.0 | 866.875 | 30 | 100 | sim |
| JP | Japão | 920.5 – 923.5 | 923.375 | 13 | 100 | sim |
| TW | Taiwan | 920.0 – 925.0 | 923.875 | 27 | 100 | sim |
| TH | Tailândia | 920.0 – 925.0 | 923.875 | 27 | 10 | sim |

Observações:

- Um ciclo de trabalho de 100 % significa que o firmware não aplica limite de tempo no ar.
- "Potência máx." é o limite da região copiado do Meshtastic. O Cap chega no máximo a 22 dBm, então a potência realmente usada é menor (veja [Potência de transmissão](#potência-de-transmissão)).
- Todas as regiões desta tabela têm espaçamento entre canais de 0 MHz.
- Os limites das faixas, ciclos de trabalho, limites de potência e espaçamentos foram copiados da tabela `RDEF` em [`src/mesh/RadioInterface.cpp`](https://github.com/meshtastic/firmware/blob/master/src/mesh/RadioInterface.cpp) do Meshtastic. Os nomes dos países são o significado usual de cada código de região. O projeto oficial pode adicionar ou mudar regiões em versões futuras.

## Regiões não suportadas

| Região(ões) | Faixa | Por que não |
|---|---|---|
| Regiões de 433 MHz (por exemplo `EU_433`, `UA_433`, `MY_433`, `PH_433`, `KZ_433`) | 433 MHz | O front-end de RF e a antena do Cap LoRa-1262 são feitos para 868–923 MHz. 433 MHz exige outro hardware. |
| `CN` | 470–510 MHz | Mesmo motivo: muito fora da faixa do Cap. |
| `LORA_24` | 2.4 GHz | Exige outro chip de rádio (SX128x). O SX1262 não sintoniza nessa faixa. |
| Qualquer região cuja frequência LongFast fique fora de 860–925 MHz | — | Não testada e fora da margem aceita para as regiões experimentais. |

## Como a frequência é calculada

O código segue exatamente o `RadioInterface::applyModemConfig()` do Meshtastic, inclusive a aritmética de ponto flutuante de precisão simples ([`regionFreqMHz`](../../src/mesh/region.cpp)):

```
hash        = djb2("LongFast")                       // 130429955
numChannels = floor((freqEnd - freqStart) / (spacing + bw/1000))
slot        = hash % numChannels
freq        = freqStart + bw/2000 + slot * bw/1000   // MHz; bw in kHz (250)
```

- `djb2` é o hash clássico de strings: começa com `h = 5381` e, para cada byte, `h = h * 33 + byte`, como valor de 32 bits sem sinal. Para `"LongFast"` ele dá **130429955**.
- `bw/2000` é metade da largura de banda em MHz (0.125 MHz), então o centro do canal fica meia largura de banda acima do início do seu slot.
- Se `numChannels` fosse 0 (uma faixa mais estreita que um canal), o código retornaria `freqStart + bw/2000`. Nenhuma região suportada cai nesse caso.

### Exemplo: ANZ

| Passo | Valor |
|---|---|
| Faixa | 915.0 – 928.0 MHz, espaçamento 0 |
| `numChannels` | floor(13.0 / (0 + 0.25)) = **52** |
| `slot` | 130429955 mod 52 = **19** |
| `freq` | 915.0 + 0.125 + 19 × 0.25 = **919.875 MHz** |

O mesmo método para outras regiões:

| Região | numChannels | slot | LongFast (MHz) |
|---|---|---|---|
| BR_902 | 22 | 7 | 903.875 |
| US | 104 | 19 | 906.875 |
| EU_868 | 1 | 0 | 869.525 |
| SG_923 | 32 | 3 | 917.875 |

### Hash do canal

Todo quadro leva no cabeçalho um hash de canal de um byte, para que os receptores escolham a chave certa. É o XOR de todos os bytes do nome do canal, combinado por XOR com todos os bytes da chave. Para `"LongFast"` com a chave padrão, ele vale **`0x08`**. Veja [protocol.md](protocol.md#criptografia-do-canal-aes-128-ctr).

## Potência de transmissão

Você define uma potência de 2 a 22 dBm na tela Config (`Potência`, padrão 22). O rádio usa a **potência efetiva** ([`effectiveTxPower`](../../src/mesh/region.cpp)):

```
limit     = region power limit, or 17 dBm if the region's limit is 0 ("unknown", as in Meshtastic)
effective = min(user setting, limit, 22)     and at least 2 dBm
```

Ou seja: `limit` é o limite de potência da região, ou 17 dBm se o limite da região for 0 ("desconhecido", como no Meshtastic); a potência efetiva é o menor valor entre a sua configuração, `limit` e 22, e no mínimo 2 dBm.

22 dBm é o máximo do SX1262. Quando a potência efetiva é menor que a sua configuração, a tela Config mostra as duas, por exemplo `22 dBm (efetiva 14)` em UA_868 ou PH_868, ou `22 dBm (efetiva 20)` em RU e SG_923. O limite de potência é reaplicado toda vez que você muda a região.

O ganho da antena não é levado em conta. Se as regras locais limitam a EIRP/ERP e você usa uma antena de alto ganho, reduza a potência por conta própria.

## Ciclo de trabalho e orçamento de tempo no ar

Algumas regiões limitam a fração do tempo em que um aparelho pode transmitir: EU_868 e TH permitem 10 %, UA_868 permite 1 %. Nessas regiões, o firmware mantém uma contabilidade móvel de uma hora do tempo no ar ([`AirtimeBudget`](../../src/mesh/region.h)) e segura os quadros quando o próximo ultrapassaria o orçamento.

- **Orçamento** = duty % de uma hora: 10 % → **360 s por hora**, 1 % → **36 s por hora**.
- **Contabilidade**: 61 baldes de um minuto. Um quadro enviado durante o minuto *M* continua contando até o minuto *M + 60*, então conta por **pelo menos 3600 s** (até 3660 s). Isso erra para o lado seguro.
- **Antes de cada transmissão**, o firmware calcula o tempo no ar do quadro (arredondado para cima em milissegundos inteiros) e verifica `used + airtime ≤ budget`. Se a verificação falha, o quadro e tudo o que está na fila atrás dele **esperam**. Nada é descartado. A barra de status mostra `duty limit` em vermelho, e a tela Config mostra `Ar 1h: Xs de Ys (duty Z%)` (tempo no ar na última hora: X s de Y s).
- Nas regiões com ciclo de trabalho de 100 % não há orçamento, e a linha da Config não aparece.

### Tempo no ar

O firmware usa a fórmula do Semtech SX126x ([`loraAirtimeUs`](../../src/mesh/region.cpp)) com os parâmetros LongFast: SF11, BW 250 kHz, CR 4/5, preâmbulo 16, cabeçalho explícito, CRC ligado, otimização para baixa taxa de dados desligada.

```
Tsym          = 2^SF / BW                              = 2048 / 250 kHz = 8.192 ms
preamble      = (16 + 4.25) × Tsym                     = 165.888 ms
payloadSymbols = 8 + ceil((8·L − 4·SF + 28 + 16) / (4·SF)) × CR   // L = whole frame length in bytes, CR = 5
               = 8 + ceil(8·L / 44) × 5
airtime       = preamble + payloadSymbols × Tsym
```

(`L` é o tamanho total do quadro em bytes; `CR` = 5.)

Assim, todo quadro custa um valor fixo de **≈ 231 ms** (L = 0), mais 5 símbolos (41 ms) a cada 5.5 bytes, o que dá em média **≈ 7.4 ms por byte**:

| Tamanho do quadro L (cabeçalho + payload) | Tempo no ar |
|---|---|
| 0 B (mínimo teórico) | 231 ms |
| 22 B (texto público curto, ex.: "oi") | 395 ms |
| 88 B (o nosso NodeInfo com os nomes padrão) | 887 ms |
| 128 B | 1.21 s |
| 200 B | 1.75 s |
| 255 B (quadro máximo) | 2.16 s |

Um quadro típico leva cerca de **1 s**. Na EU_868, 360 s por hora equivalem a uns 400 quadros do tamanho de um NodeInfo por hora, muito mais do que este firmware envia em uso normal. Na UA_868 (36 s por hora), cabem cerca de 40 desses quadros por hora.

## Regiões experimentais

O M5Stack Cap LoRa-1262 é especificado para **868–923 MHz**. O chip SX1262 em si sintoniza de **150 a 960 MHz**, então o rádio pode ser ajustado para frequências um pouco fora da especificação do Cap. A chave de RF, a filtragem e o casamento de impedância são projetados para a faixa nominal. Espere potência de transmissão mais fraca e pior sensibilidade fora dela. Quanto pior, não foi medido.

O firmware inclui regiões cuja frequência LongFast está em [860, 868) MHz (IN, NZ_865, NP_865, KZ_863) ou (923, 925] MHz (JP, TW, TH) e as sinaliza:

- Na Config, a região mostra `(exp)`, por exemplo `IN 865.875 (exp)`.
- Ao selecionar uma delas, aparece o aviso `Fora da faixa do Cap (exp)`.
- Enquanto uma estiver selecionada, a Config mostra `! Fora de 868-923 MHz do Cap (exp)`.

Use uma região experimental só se ela for a região correta do seu país, e espere alcance reduzido.

**Japão (JP).** As regras japonesas da faixa de 920 MHz (ARIB STD-T108) podem exigir detecção de portadora (LBT) e limites de tempo de transmissão; este firmware não implementa LBT e não foi avaliado quanto à conformidade — verifique você mesmo os requisitos do 電波法/技適 antes de transmitir no Japão.

## Brasil

Duas regiões são relevantes no Brasil, e elas **não** se ouvem:

| Região | LongFast | Situação |
|---|---|---|
| **ANZ** | 919.875 MHz | O que a comunidade Meshtastic brasileira usa hoje (por exemplo [meshbrasil.com](https://meshbrasil.com)). **O padrão deste firmware.** |
| **BR_902** | 903.875 MHz | A região oficial do Meshtastic para o Brasil (902–907.5 MHz). |

As regras da Anatel para equipamentos de radiação restrita (sem licença) permitem dispositivos de espalhamento espectral, como o LoRa, em **902–907.5 MHz e 915–928 MHz** dentro da faixa de 902–928 MHz. O trecho de 907.5–915 MHz não faz parte dessa destinação sem licença. As duas frequências acima ficam dentro de trechos permitidos. **Confira a regulamentação vigente da Anatel** antes de confiar nisso.

Se você não ouvir ninguém na ANZ, pergunte aos usuários locais qual região eles usam, ou experimente a BR_902.

## Antenas por faixa

Uma antena é sintonizada para uma faixa de frequências. Longe dela, a ROE (VSWR) aumenta e parte da potência é refletida em vez de irradiada.

| Frequência da sua região | Melhor antena |
|---|---|
| 863–870 MHz (EU_868, RU, UA_868, PH_868, IN, NZ_865, NP_865, KZ_863) | Antena de 868 MHz |
| 902–928 MHz (ANZ, BR_902, US, KR, MY_919, SG_923, PH_915, JP, TW, TH) | Antena de 915 MHz |

A antena que vem com o Cap é sintonizada em 868 MHz. Nas regiões de 902–928 MHz ela ainda funciona, com perda esperada de cerca de **0.5–2 dB**: 919.875 MHz fica cerca de 6 % acima da sintonia e 903.875 MHz cerca de 4 % acima. Com ROE de 2:1, cerca de 11 % da potência é refletida (≈ 0.5 dB). Com 3:1, a perda é ≈ 1.25 dB. Isso custa algo como 5–20 % do alcance. Se for comprar uma antena para uma região de 902–928 MHz, escolha uma sintonizada em 915 MHz em vez de uma "banda larga". Com a antena de 868 MHz, a BR_902 (903.875 MHz) fica um pouco mais perto da sintonia do que a ANZ. Veja [hardware.md](hardware.md#antena-e-alcance) para mais dicas de alcance.

**Nunca transmita sem uma antena conectada.** O firmware transmite um NodeInfo logo após a inicialização.

## Mantendo a tabela correta

Na versão em inglês deste documento, [`docs/en/frequencies.md`](../en/frequencies.md), a tabela de regiões fica entre os comentários HTML `<!-- regions:begin -->` e `<!-- regions:end -->`. O teste nativo [`test/test_docs_regions`](../../test/test_docs_regions/test_main.cpp) lê aquele arquivo, interpreta cada linha da tabela e a compara com `regionAt(i)` na mesma ordem: o nome, os limites da faixa, a frequência LongFast calculada por `regionFreqMHz`, o limite de potência, o ciclo de trabalho e o indicador de experimental. Ele roda a cada `pio test -e native` e, portanto, na CI a cada push e pull request. Se você mudar `region.cpp`, atualize a tabela em inglês no mesmo commit (a ordem das colunas importa para o teste) e depois as traduções. As traduções não são verificadas pelo teste.
