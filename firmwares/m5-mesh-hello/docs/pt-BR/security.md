> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/security.md).

# Segurança

Esta página trata do que o M5-Mesh-Hello v0.1.0 protege, do que ele não protege e de como verificar uma versão. Para relatar uma vulnerabilidade, siga o [SECURITY.md](../../SECURITY.md) (em inglês). Não abra uma issue pública.

## Modelo de ameaças

| Adversário | Capacidade | Principais preocupações |
|---|---|---|
| **Atacante remoto via LoRa** | Consegue transmitir quadros arbitrários na frequência da região, à distância | Corrupção de memória ou travamentos por quadros malformados, contornar a criptografia, forçar o aparelho a transmitir (abuso do ar), esgotamento do armazenamento |
| **Nó Meshtastic malicioso ou falsificado** | Consegue enviar quadros Meshtastic bem formados com qualquer número de nó `from`, nomes e chave | Se passar por outros nós, substituir a chave de um contato, inundar com pedidos de NodeInfo, nos fazer disparar saudações em massa |
| **Acesso físico** | Tem em mãos o aparelho ou o chip de flash dele | Ler a chave privada, ler os contatos salvos e as configurações, instalar firmware modificado |
| **Cadeia de suprimentos** | Adultera dependências, a CI ou os binários publicados | Código malicioso no build ou em `bin/` |

## Proteções

### Entrada de rádio não confiável

- **Todo byte vindo do rádio é não confiável.** O parser do cabeçalho, o decodificador de protobuf, a decifração e a lógica do aplicativo conferem todo tamanho e índice contra buffers de tamanho fixo. Quadros com mais de 255 bytes, payloads com mais de 239 bytes, campos de protobuf que passam do fim, campos `payload` grandes demais e chaves públicas que não têm 32 bytes são rejeitados.
- **Sem alocação de heap no caminho de RX.** Todos os buffers têm tamanho fixo, e a struct decodificada grande é estática.
- **Fuzzing e sanitizers.** `test/test_fuzz` passa 200000 quadros aleatórios e mutados por todo o caminho de recepção. A CI roda todos os testes nativos, inclusive o de fuzzing, sob AddressSanitizer e UndefinedBehaviorSanitizer.
- **Análise estática.** O CodeQL (`security-extended`) roda a cada push e pull request e semanalmente. O job falha com qualquer achado.

### Criptografia

- As DMs usam **X25519 + SHA-256 + AES-256-CCM** com tag de 8 bytes, como no Meshtastic. As tags são comparadas em **tempo constante**, e uma decifração que falha apaga o buffer de texto claro, então dados não autenticados nunca são usados.
- Chaves X25519 fracas de contatos (as que dão um segredo compartilhado inválido) são rejeitadas.
- **Segredos são apagados** com `secureZero()` após o uso: chaves compartilhadas, a saída bruta do X25519, key schedules, o bloco contador do CTR e o registro de chave lido da NVS.
- A chave privada **nunca é exibida, impressa nem registrada em log**. A interface mostra só os 4 primeiros bytes da chave pública.
- **A geração de chaves usa entropia de hardware.** A fonte de entropia por ruído de RF do ESP32-S3 é ativada (`bootloader_random_enable`) enquanto a chave é gerada, porque Wi-Fi e Bluetooth estão desligados. O gerador de IDs de pacote também é semeado com entropia de hardware a cada inicialização, então os blocos contadores do CTR e os nonces do CCM não se repetem entre reinicializações, e toda DM leva um `extraNonce` aleatório novo.
- O par de chaves guardado é conferido ao carregar (`pub == X25519(priv)`). Um registro corrompido nunca é usado pela metade.

### Comportamento do rádio e antiabuso

- **Wi-Fi e Bluetooth ficam desligados.** A única interface remota é o LoRa.
- **O aparelho nunca retransmite** o tráfego de outros nós (`CLIENT_MUTE`).
- **As respostas automáticas têm limite de frequência**, por nó e global, para que números `from` falsificados não consigam manter o aparelho no ar:
  - Respostas de NodeInfo: uma vez a cada 12 h por nó e uma vez a cada 30 s no total.
  - NAK + NodeInfo para DMs PKI que não conseguimos decifrar: uma vez a cada 10 min por nó e uma vez a cada 30 s no total.
  - Pedidos de NodeInfo quando você manda DM para um nó sem chave: uma vez a cada 60 s por nó.
  - As nossas próprias difusões de NodeInfo: no máximo uma a cada 30 s, mais uma a cada 3 h.
- **Ritmo de transmissão**: um quadro de cada vez, com pelo menos 1.5 s de intervalo, fila de 6 quadros e o orçamento de tempo no ar do ciclo de trabalho da região.
- **Saudação automática**: desligada na inicialização, cumprimenta cada nó no máximo uma vez (persistido), no máximo uma saudação a cada 30 s, com fila de 10.
- **Supressão de duplicatas** dos últimos 64 pares `(from, id)`.
- **Armazenamento limitado**: blobs NVS de tamanho fixo, no máximo 50 contatos salvos (slots reutilizados), no máximo 500 IDs cumprimentados. Uma enxurrada de nós falsos não consegue fazer o armazenamento crescer.

### Identidade e chaves

- **A primeira chave vence**: a primeira chave pública recebida para um número de nó é mantida, e um NodeInfo posterior com chave diferente é ignorado. As chaves dos contatos de DM são persistidas, então isso sobrevive a reinicializações.
- **DMs legadas são rejeitadas**: uma DM de texto criptografada só com a chave do canal público é descartada, então uma DM exibida numa conversa sempre foi autenticada por PKI com a chave guardada do remetente.

### Cadeia de suprimentos

- As bibliotecas e a plataforma são **fixadas em versões exatas** no `platformio.ini`. A biblioteca de criptografia é incorporada (`lib/crypto/`, MIT).
- As GitHub Actions são **fixadas por SHA completo de commit**, os workflows rodam com `contents: read`, o checkout não guarda credenciais e o Dependabot propõe atualizações das actions.
- As imagens da versão vêm com **`bin/SHA256SUMS`**. A CI as verifica de novo a cada execução.

## O que NÃO é protegido

- **A chave privada pode ser lida com acesso físico.** A criptografia da flash e o secure boot ficam desligados, porque o firmware é instalado pelo M5Launcher, que não consegue usá-los. Quem conseguir copiar a flash consegue ler o par de chaves na NVS (chave `idkey`) e se passar pelo nó em DMs. Os contatos salvos, a lista de cumprimentados e as configurações também podem ser lidos. Se o aparelho for perdido, considere a identidade dele comprometida.
- **O canal público pode ser lido por qualquer um.** Ele usa a chave padrão conhecida do Meshtastic, por definição, então toda mensagem que você envia na tela Canal, e todo NodeInfo (nomes, chave pública, modelo de hardware), é público.
- **Sem assinaturas de pacote (XEdDSA) na v1.** Quadros de broadcast, inclusive textos de canal e NodeInfo, não são autenticados. Qualquer um pode enviar uma mensagem de canal ou um NodeInfo alegando qualquer número de nó. A política padrão do Meshtastic também aceita pacotes sem assinatura.
- **Falsificação antes de a chave ser conhecida.** Números de nó não estão vinculados a chaves. Enquanto não tivermos guardado a chave real de um nó, um atacante pode enviar um NodeInfo com aquele número de nó e a própria chave, e a regra "a primeira chave vence" então mantém a chave do atacante. DMs "para" aquele nó iriam para o atacante, e as DMs do nó verdadeiro seriam rejeitadas. Isso é uma propriedade do protocolo oficial.
- **Os metadados são visíveis.** Números de nó, nomes, modelo de hardware, horário e tamanho dos pacotes e quem envia para quem (os campos `to`/`from` do cabeçalho não são criptografados) ficam visíveis para qualquer receptor. O seu ID de nó deriva do endereço MAC do aparelho.
- **O conteúdo das DMs é criptografado de ponta a ponta, mas a entrega não é garantida.** Não há reenvio, e um atacante pode interferir na frequência (jamming) ou descartar ACKs.
- **Negação de serviço na camada física** (jamming) está fora do escopo.
- **O histórico de conversas** fica só na RAM e some ao desligar. Ele não é protegido contra alguém que esteja com o aparelho desbloqueado nas mãos.

## Verificando uma versão

As imagens da versão e seus checksums estão em [`bin/`](../../bin/):

```sh
cd bin
sha256sum -c SHA256SUMS
```

As duas linhas precisam dizer `OK`. No Windows sem `sha256sum`:

```bat
certutil -hashfile M5-Mesh-Hello.bin SHA256
```

Compare o resultado com a linha daquele arquivo no `SHA256SUMS`.

Os checksums provam que os arquivos batem com o que foi commitado no repositório. Para conferir que um binário corresponde ao código-fonte, compile-o de novo (`python -m platformio run -e cardputer`) com as ferramentas fixadas e compare. A CI também compila o firmware a partir do código-fonte a cada push e publica o `firmware.bin` compilado por ela, com o próprio checksum, como artefato do workflow. Não há garantia de que os builds sejam reproduzíveis bit a bit entre máquinas diferentes.

## Relatando uma vulnerabilidade

Use o relato privado de vulnerabilidades do GitHub, como descrito no [SECURITY.md](../../SECURITY.md) (em inglês). Inclua a versão, a região e, se possível, um quadro que reproduza o problema (dump em hexadecimal).
