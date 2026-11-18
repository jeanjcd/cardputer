[English](README.md) | **Português** | [Español](README.es.md) | [Русский](README.ru.md) | [日本語](README.ja.md)

> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](README.md).

# M5-Mesh-Hello

Um nó pequeno e autônomo, compatível com [Meshtastic](https://meshtastic.org), para o **M5Stack Cardputer-Adv** com o **Cap LoRa-1262**. Ele entra na rede Meshtastic como um nó comum (nome padrão **4stro-031** / **4S31**), lista os nós ao seu redor, conversa com eles por mensagens diretas criptografadas, lê e escreve no canal público LongFast e pode cumprimentar cada nó novo **exatamente uma vez**. Não precisa de celular.

Este não é o firmware oficial do Meshtastic. É uma implementação mínima escrita do zero (RadioLib + protocolo + criptografia), com interface própria pelo teclado, pequena o bastante para ser instalada ao lado de outros apps com o M5Launcher.

## Recursos

- Meshtastic **LongFast** no canal público padrão (AES-128-CTR, chave padrão).
- **Mensagens diretas criptografadas** (PKI: X25519 + SHA-256 + AES-256-CCM) com confirmação de entrega (ACK).
- **Lista de nós** com nomes, modelo de hardware, RSSI, tempo desde a última recepção e marcadores de não lida e de já cumprimentado.
- Chat no **canal público**.
- **Saudação automática**: desligada a cada inicialização. Quando ligada, envia sua saudação uma única vez por nó, para sempre (lista persistida), no máximo uma a cada 30 s.
- **18 regiões** (11 dentro da faixa nominal do Cap e 7 experimentais), com a potência de TX limitada ao máximo de cada região e um orçamento de tempo no ar por hora nas regiões com ciclo de trabalho (duty cycle).
- Nome longo/curto, texto da saudação e potência de TX editáveis. O par de chaves é gerado uma única vez a partir da entropia do hardware e fica guardado na NVS (só muda se a NVS for apagada).
- Nunca retransmite o tráfego de outros nós (`CLIENT_MUTE`). Wi-Fi e Bluetooth ficam desligados.
- Núcleo de protocolo portátil com testes unitários nativos, teste de fuzzing, ASan/UBSan e CodeQL na CI.

## Telas

```
+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:ON | rx 57 | -101dBm |
| Nós*  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|BASE Base Stati TBEAM          -97  2m* |   |!9c2ea3f1 Base Station TBEAM ESP32      |
|JOAO Joao Movel HELTEC_V3     -112 15s +|   |v>Saudaçoes de 4stro-031! ? (Cardputer-A|
|a3f1 !9c2ea3f1  -                -   -  |   |  dv)                                   |
|                                        |   |< Oi! Bem-vindo a rede                  |
|                                        |   |                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |   |> _                                     |
+----------------------------------------+   +----------------------------------------+
        Nós (lista de nós)                            Conversa (mensagens diretas)

+----------------------------------------+   +----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |   |ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |
|  Nós  | Conversa |  Canal  |  Config   |   |  Nós  | Conversa |  Canal  |  Config   |
|< JOAO: bom dia pessoal                 |   |Regiao     ANZ 919.875                  |
|.>alguem em Sao Paulo?                  |   |Potência   22 dBm                       |
|                                        |   |Nome longo 4stro-031                    |
|                                        |   |Nome curto 4S31                         |
|                                        |   |Saudaçao   Saudaçoes de 4stro-031! ? (Ca|
|                                        |   |Enviar NodeInfo                         |
|                                        |   |ID !a1b2c3d4  chave 1a2b3c4d            |
|> _                                     |   |Enter:editar/acionar  ;/. move          |
+----------------------------------------+   +----------------------------------------+
        Canal (canal público)                         Config
```

`Tab` troca de tela, `;`/`.` movem a seleção, `Enter` abre/envia/edita, `` ` `` volta, `A` na tela Nós liga/desliga a saudação automática. O guia completo está em [docs/pt-BR/user-guide.md](docs/pt-BR/user-guide.md).

## Hardware

- **M5Stack Cardputer-Adv** (o Cardputer original não é suportado).
- **M5Stack Cap LoRa-1262** (SX1262, faixa nominal de 868–923 MHz, até 22 dBm).
- **Uma antena** no conector RP-SMA do Cap. A antena do kit é sintonizada em 868 MHz e funciona nas regiões de 902–928 MHz com perda de cerca de 0.5–2 dB. Para essas regiões, uma antena de 915 MHz é melhor.

> **Sempre rosqueie a antena antes de ligar o aparelho.** O firmware transmite seu NodeInfo logo após a inicialização. Transmitir sem antena pode danificar o rádio.

Detalhes: [docs/pt-BR/hardware.md](docs/pt-BR/hardware.md).

## Início rápido

1. **Verifique** as imagens da versão em [`bin/`](bin/):
   ```sh
   cd bin && sha256sum -c SHA256SUMS
   ```
2. Copie `bin/M5-Mesh-Hello.bin` para o cartão microSD e instale com o **M5Launcher**. Se houver uma versão anterior instalada, remova-a antes (**PMan → Remove**).
   (`bin/M5-Mesh-Hello-merged.bin` é uma imagem completa da flash para o esptool no endereço `0x0`. Gravá-la **apaga o M5Launcher e todos os apps instalados por ele**. Use-a só se você não usa o M5Launcher.)
3. Conecte a antena e inicie o app. Na primeira inicialização ele cria seu par de chaves e pergunta a sua **região** (ANZ pré-selecionada; `;` / `.` para mudar, `Enter` para confirmar). **Nada é transmitido até você confirmar.** Depois ele se anuncia e começa a escutar.
4. Você pode mudar a região depois na tela **Config** (`Região`): todos os nós precisam usar a mesma, e você deve usar a que é legal no seu país.
5. Espere os nós aparecerem na tela **Nós**, selecione um, pressione `Enter` e envie a saudação já preenchida. Ou pressione `A` para ligar a saudação automática.

## Regiões

O padrão é **ANZ** (919.875 MHz), usada pela comunidade Meshtastic brasileira. A região oficial do Meshtastic para o Brasil é a BR_902 (903.875 MHz). O firmware também suporta US, EU_868, RU, KR, UA_868, MY_919, SG_923, PH_868 e PH_915, além das experimentais IN, NZ_865, NP_865, KZ_863, JP, TW e TH (um pouco fora da faixa nominal do Cap). **Use sempre a região do país onde você está.** No Japão (JP), as regras da faixa de 920 MHz (ARIB STD-T108) podem exigir detecção de portadora (LBT) e limites de tempo de transmissão; este firmware não implementa LBT e não foi avaliado quanto à conformidade — verifique você mesmo os requisitos do 電波法/技適 antes de transmitir no Japão. A tabela completa, a fórmula da frequência, os limites de potência e o ciclo de trabalho estão em [docs/pt-BR/frequencies.md](docs/pt-BR/frequencies.md).

## Documentação

| | |
|---|---|
| [Guia do usuário](docs/pt-BR/user-guide.md) | Telas, teclas, configurações, saudação automática, solução de problemas, perguntas frequentes |
| [Hardware](docs/pt-BR/hardware.md) | Mapa de pinos, antena, alcance, M5Launcher |
| [Regiões e frequências](docs/pt-BR/frequencies.md) | Regiões, fórmula, potência, ciclo de trabalho, Brasil |
| [Protocolo](docs/pt-BR/protocol.md) | Formato no ar e criptografia, para implementadores |
| [Arquitetura](docs/pt-BR/architecture.md) | Módulos, fluxo de dados, armazenamento, limites |
| [Desenvolvimento](docs/pt-BR/development.md) | Compilação, testes, CI, versões |
| [Segurança](docs/pt-BR/security.md) | Modelo de ameaças e limites |
| [Roadmap](docs/pt-BR/roadmap.md) | Ideias e limitações conhecidas |

Todos os idiomas: [docs/README.md](docs/README.md). Os arquivos a seguir existem apenas em inglês: contribuições em [CONTRIBUTING.md](CONTRIBUTING.md), relatos de segurança em [SECURITY.md](SECURITY.md), mudanças em [CHANGELOG.md](CHANGELOG.md), licença em [LICENSE](LICENSE) e componentes de terceiros em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Status

**v0.1.0.** Testado em campo num Cardputer-Adv + Cap LoRa-1262: inicialização do rádio, transmissão, persistência da identidade entre reinicializações e toda a interface (telas, navegação, liga/desliga da saudação automática) funcionam. **A troca de ponta a ponta com outros nós Meshtastic (receber NodeInfo, DMs e ACKs deles em campo) ainda não foi verificada**, porque nenhum nó estava ao alcance durante os testes. O protocolo foi implementado com base no firmware oficial e testado contra vetores de referência, mas considere a interoperabilidade não confirmada até que alguém relate que funciona. Relatos são bem-vindos.

## Licença

[GPL-3.0-or-later](LICENSE) (texto em inglês). Os componentes de terceiros e suas licenças estão listados em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
