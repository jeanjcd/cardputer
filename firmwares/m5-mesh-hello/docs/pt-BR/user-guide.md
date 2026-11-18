> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/user-guide.md).

# Guia do usuário

Este guia cobre o M5-Mesh-Hello v0.1.0 no M5Stack **Cardputer-Adv** com o **Cap LoRa-1262**. Ele explica cada tela, tecla, configuração e mensagem de status.

O texto na tela está em português, e este guia o reproduz exatamente como aparece. A tela não tem glifos acentuados para algumas letras, então algumas palavras aparecem sem acento no aparelho, por exemplo `Regiao` e `auto-saudaçao` (veja [Limitações da tela](#limitações-da-tela)).

Conteúdo:

- [Antes de começar](#antes-de-começar)
- [Primeira inicialização](#primeira-inicialização)
- [Layout da tela](#layout-da-tela)
- [Teclas](#teclas)
- [Tela Nós](#tela-nós)
- [Tela Conversa](#tela-conversa)
- [Tela Canal](#tela-canal)
- [Tela Config](#tela-config)
- [Saudação automática](#saudação-automática)
- [Marcadores de mensagem](#marcadores-de-mensagem)
- [Quando o dispositivo escuta e quando transmite](#quando-o-dispositivo-escuta-e-quando-transmite)
- [Por que nenhum nó aparece](#por-que-nenhum-nó-aparece)
- [Solução de problemas: todos os status e mensagens](#solução-de-problemas-todos-os-status-e-mensagens)
- [Limitações da tela](#limitações-da-tela)
- [Limites](#limites)
- [Perguntas frequentes](#perguntas-frequentes)

## Antes de começar

1. **Rosqueie a antena no Cap antes de ligar.** O firmware transmite um anúncio NodeInfo menos de um segundo após a inicialização. Transmitir sem antena pode danificar o rádio.
2. Instale o firmware com o M5Launcher (veja o [início rápido do README](../../README.pt-BR.md#início-rápido) e [hardware.md](hardware.md#instalação-com-o-m5launcher)).
3. Descubra qual região os usuários Meshtastic perto de você usam. No Brasil, a comunidade usa **ANZ** (o padrão). Veja [frequencies.md](frequencies.md).

## Primeira inicialização

- Na primeiríssima inicialização, o aparelho gera um par de chaves X25519 a partir do gerador de números aleatórios do hardware e o grava na flash (NVS). O par de chaves **nunca muda** depois disso. Os nós Meshtastic guardam a primeira chave pública que ouvem de cada nó ("a primeira chave vence"), então uma chave nova faria com que eles parassem de conseguir enviar mensagens diretas para você.
- O seu **ID de nó** (`!xxxxxxxx`) vem do endereço MAC de fábrica do chip e também é permanente.
- Os nomes padrão são **4stro-031** (longo) e **4S31** (curto). Você pode mudá-los na tela Config.
- O aparelho inicia na região salva da última vez (ANZ numa instalação nova), difunde seu NodeInfo pedindo aos vizinhos que enviem os deles e então fica escutando.
- **A saudação automática fica DESLIGADA** após toda inicialização.

## Layout da tela

A tela de 240 × 135 mostra 40 colunas de texto (as palavras aparecem como a fonte as desenha, por exemplo `auto-saudaçao`):

```
+----------------------------------------+
|ANZ 919.875 | AUTO:OFF | rx 42 | -97dBm |  <- barra de status (ou um aviso amarelo)
| Nós*  | Conversa |  Canal  |  Config   |  <- abas (* = algo não lido)
|BASE Base Stati TBEAM          -97  2m* |
|JOAO Joao Movel HELTEC_V3     -112 15s +|  <- um nó por linha
|a3f1 !9c2ea3f1  -                -   -  |
|                                        |
|Enter:conversa A:auto-saudaçao Tab:tela |  <- dica de teclas / entrada de texto
+----------------------------------------+
```

### Barra de status

`<região> <frequência> | AUTO:ON ou AUTO:OFF | rx <quadros recebidos> | <RSSI do último quadro>`

- **Região e frequência** em MHz, por exemplo `ANZ 919.875`. Se o rádio não estiver funcionando, este campo mostra `SEM CAP LoRa` ou `RADIO ERRO` (veja [Solução de problemas](#solução-de-problemas-todos-os-status-e-mensagens)).
- **`AUTO:ON` / `AUTO:OFF`**: estado da saudação automática.
- **`rx N`**: quadros LoRa recebidos desde a inicialização. Conta todo quadro que o rádio decodifica, inclusive os que o firmware não consegue ler (outros canais, mensagens diretas de outras pessoas). Se o número sobe, o rádio está ouvindo alguma coisa.
- **RSSI** do último quadro recebido, por exemplo `-97dBm`. Mostra `-dBm` até chegar o primeiro quadro, e `duty limit` enquanto o orçamento de tempo no ar da região estiver segurando as transmissões.
- Se a linha não couber em 40 colunas, os espaços em volta de `|` são removidos.
- A barra fica **vermelha** quando o Cap está ausente, o rádio falhou ou o limite de duty cycle está ativo.

**Avisos** (toasts) são mensagens curtas que substituem a barra de status por 2.5 segundos, com fundo amarelo, por exemplo `Config salva`.

### Abas

`Nós`, `Conversa`, `Canal`, `Config`. A aba ativa fica destacada. `Nós` vira `Nós*` quando algum nó tem mensagens diretas não lidas, e `Canal` vira `Canal*` quando há mensagens não lidas no canal público.

## Teclas

| Tecla | Onde | Ação |
|---|---|---|
| `Tab` | em todo lugar | Próxima tela: Nós → Conversa → Canal → Config → Nós |
| `;` | Nós, Config | Move a seleção para cima (a tecla `;` tem a seta para cima) |
| `.` | Nós, Config | Move a seleção para baixo (a tecla `.` tem a seta para baixo) |
| `Enter` | Nós | Abre a conversa com o nó selecionado |
| `Enter` | Conversa, Canal | Envia o texto digitado |
| `Enter` | Config | Edita o item selecionado ou o executa (`Região`, `Enviar NodeInfo`); durante a edição: salva |
| `` ` `` (Esc) | Conversa, Canal, Config | Volta para Nós; durante a edição de um item da Config: cancela a edição |
| `Fn` + `;` | Conversa, Canal | Rola o histórico para cima (mensagens mais antigas) |
| `Fn` + `.` | Conversa, Canal | Rola o histórico para baixo (mensagens mais novas) |
| `del` (Backspace) | entrada de texto, edição na Config | Apaga o último caractere |
| `A` (ou `a`) | só em Nós | Liga ou desliga a saudação automática |
| letras, dígitos, símbolos | entrada de texto | Digitar (somente ASCII imprimível) |

Observações:

- As teclas não se repetem quando mantidas pressionadas. Pressione uma vez por caractere ou passo.
- Nas telas de texto, `;` e `.` sem `Fn` são digitados como texto, e não é possível digitar o caractere `` ` `` porque ele é a tecla Esc. Outras combinações com `Fn` são ignoradas.
- Durante a edição de `Potência`, só dígitos são aceitos.

## Tela Nós

A lista de nós ouvidos desde a inicialização, mais os contatos salvos de sessões anteriores. O nó ouvido mais recentemente fica no topo. Dez linhas ficam visíveis, e a lista rola junto com a seleção.

Cada linha, da esquerda para a direita:

| Coluna | Largura | Conteúdo |
|---|---|---|
| Nome curto | 4 | O nome curto do nó, ou os últimos 4 dígitos hexadecimais do número do nó se o NodeInfo dele ainda não chegou |
| Nome longo | 10 | O nome longo (cortado em 10 caracteres), ou `!xxxxxxxx` se for desconhecido |
| Modelo de hardware | 13 | Por exemplo `TBEAM`, `HELTEC_V3`, `CARDPUTER_ADV`; `HW#<n>` para modelos fora da tabela do firmware; `-` se nada se sabe ainda |
| RSSI | 4 | Intensidade do sinal do último quadro ouvido dele, em dBm; `-` se não foi ouvido desde a inicialização |
| Tempo | 3 | Tempo desde a última vez que foi ouvido: `15s`, `2m`, `1h`, `3d`; `-` se não foi ouvido desde a inicialização |
| `*` | 1 | Mensagens diretas **não lidas** deste nó |
| `+` | 1 | Já **cumprimentado** (pela saudação automática ou pelo envio manual da saudação); nunca mais será cumprimentado automaticamente |

Cores: **amarelo** = mensagens não lidas; **branco** = a chave pública dele é conhecida (dá para enviar DM na hora); **cinza-escuro** = ainda sem chave (uma DM para ele espera a chave, veja o marcador `?`).

Contatos carregados da flash na inicialização mostram `-` em RSSI e tempo até serem ouvidos de novo.

Quando a lista está vazia, ela mostra `Nenhum nó ouvido ainda.` e `Aguarde pacotes da rede Meshtastic.`. Veja [Por que nenhum nó aparece](#por-que-nenhum-nó-aparece).

Teclas: `;`/`.` selecionam, `Enter` abre a conversa, `A` liga/desliga a saudação automática. A linha de dica mostra `Enter:conversa A:auto-saudação Tab:tela`.

## Tela Conversa

A conversa por mensagem direta (DM) com um nó. Abra com `Enter` na tela Nós.

- Sem nó escolhido, mostra `Escolha um nó na aba Nós (Enter).`.
- **Linha de cabeçalho**: `!<id do nó> <nome longo> <modelo> <família do chip>`, por exemplo `!9c2ea3f1 Base Station TBEAM ESP32`. Termina com `(sem chave)` em laranja se a chave pública do nó ainda não for conhecida. Se o nó saiu da lista de nós (mais de 100 nós ouvidos), o cabeçalho mostra `!<id do nó> (fora da lista)`.
- **Histórico**: 9 linhas, a mais antiga no topo, mensagens longas quebradas em várias linhas. Mensagens recebidas começam com `<`. As suas começam com um marcador de estado e `>` (veja [Marcadores de mensagem](#marcadores-de-mensagem)). Texto recebido fica em branco, o seu em ciano, e mensagens que falharam em vermelho. Um `^` amarelo no canto superior direito indica que você rolou para cima. Uma conversa vazia mostra `(sem mensagens)`.
- **Linha de entrada** embaixo: `> texto_`. Quando você abre uma conversa, ela vem **preenchida com o seu texto de saudação** como sugestão. Apague com `del` ou simplesmente envie.
- `Enter` envia o texto como DM criptografada (PKI) e limpa a entrada. `` ` `` volta para Nós. `Fn`+`;` / `Fn`+`.` rolam.

Enviar a saudação exatamente como veio preenchida conta como ter cumprimentado aquele nó: ele recebe a marca `+` e a saudação automática nunca vai cumprimentá-lo.

Abrir uma conversa limpa a marca de não lida daquele nó.

## Tela Canal

O canal público: o canal primário padrão do Meshtastic (`LongFast`, chave padrão), que qualquer nó com as configurações padrão consegue ler.

- 10 linhas de histórico. As mensagens recebidas são guardadas como `<nome curto>: <texto>`, ou `<últimos 4 dígitos hexadecimais do id do nó>: <texto>` se o nome do remetente for desconhecido.
- `Enter` envia a linha de entrada para todos no canal. Mensagens de canal não têm confirmação, então as suas ficam em `.` depois de enviadas.
- `` ` `` volta para Nós. `Fn`+`;` / `Fn`+`.` rolam. Visualizar a tela limpa a marca `Canal*`.

## Tela Config

Seis itens. Selecione um com `;`/`.` e pressione `Enter`. A linha de dica mostra `Enter:editar/acionar  ;/. move`. Durante a edição, o prompt mostra `<item>: Enter salva, ` cancela` acima de uma linha de entrada.

| Item (na tela) | Significado | O que `Enter` faz |
|---|---|---|
| `Região` | Região de rádio | Passa para a próxima região da lista (volta ao início no fim) |
| `Potência` | Potência de TX | Edita a potência em dBm |
| `Nome longo` | Nome longo | Edita o nome longo |
| `Nome curto` | Nome curto | Edita o nome curto |
| `Saudação` | Saudação | Edita o texto da saudação |
| `Enviar NodeInfo` | Enviar NodeInfo | Difunde o seu NodeInfo agora |

### Região

Mostra o nome da região e a frequência LongFast, por exemplo `ANZ 919.875`. Cada `Enter` avança para a próxima região na ordem da [tabela de regiões](frequencies.md#regiões-suportadas) e **ressintoniza o rádio na hora**. Também reaplica o limite de potência da região. A nova região é salva 2 segundos após a última mudança, ou quando você sai da tela Config (aviso `Config salva`). Se você desligar o aparelho dentro desses 2 segundos, a mudança se perde.

- Regiões experimentais mostram `(exp)`, por exemplo `IN 865.875 (exp)`. Ao selecionar uma delas, aparece o aviso `Fora da faixa do Cap (exp)`. Enquanto uma estiver selecionada, a tela mostra o alerta `! Fora de 868-923 MHz do Cap (exp)`. Essas frequências ficam fora da faixa nominal do Cap, então o alcance pode ser menor. Use uma delas só se for a região do seu país.
- Nas regiões com limite de ciclo de trabalho (EU_868, UA_868, TH), uma linha extra mostra o tempo no ar usado na última hora: `Ar 1h: 12s de 360s (duty 10%)`.
- Todos os nós precisam estar na mesma região para se ouvirem. Veja [frequencies.md](frequencies.md).

### Potência

A potência de transmissão desejada, 2–22 dBm (padrão 22). Digite 1 ou 2 dígitos e pressione `Enter`. Um valor inválido mostra o aviso `Potência: 2 a 22 dBm` e mantém o editor aberto.

O rádio usa a **potência efetiva** = min(sua configuração, limite da região, 22 dBm). Quando a potência efetiva é menor, o item mostra as duas, por exemplo `22 dBm (efetiva 14)`. A mudança vale na hora e é salva (`Config salva`).

### Nome longo / Nome curto

Os nomes do seu nó como outros usuários Meshtastic os veem. Nome longo: 1–24 bytes. Nome curto: 1–4 bytes (recomenda-se letras/dígitos ASCII). Só é possível digitar ASCII no teclado. Uma entrada inválida mostra `Nome longo: 1 a 24 bytes` ou `Nome curto: 1 a 4 letras` (apesar do texto, o limite real é de 1–4 bytes).

Depois que você salva um nome, o aparelho **difunde um novo NodeInfo** para que os outros nós o conheçam. As difusões têm intervalo mínimo de 30 s; se a última foi há menos de 30 s, a nova sai quando os 30 s se completarem. **O seu ID de nó e a sua chave continuam os mesmos**: os outros nós continuam reconhecendo você e ainda podem lhe enviar DMs. Só o nome exibido muda. Os outros nós atualizam o nome quando recebem o novo NodeInfo.

### Saudação

O texto usado pela saudação automática e pré-preenchido em toda conversa nova, com até 199 bytes.

- Por padrão, ele é montado a partir do seu nome longo: `Saudações de <nome longo>! 👋 (Cardputer-Adv)`. Ele acompanha o nome longo quando você renomeia o nó.
- Edite-o para definir o seu próprio texto, que passa a ser enviado exatamente como você digitou.
- Salve um texto **vazio** (ou exatamente o texto padrão) para voltar ao padrão montado automaticamente.
- Um texto inválido mostra `Saudação inválida`.

O editor mostra a saudação com as substituições de exibição (por exemplo `?` no lugar do emoji), mas o texto guardado não muda, a menos que você o edite. Se você editar a saudação padrão, o emoji é mantido desde que você não o apague.

### Enviar NodeInfo

Difunde o seu NodeInfo (nomes e chave pública) no canal público na hora. Use para que os nós próximos aprendam a sua chave, por exemplo antes de alguém tentar lhe enviar uma DM.

As difusões são limitadas a **uma a cada 30 segundos**. Esse limite é compartilhado com o anúncio da inicialização e com as mudanças de nome. Se você pressionar `Enter` antes disso, aparece o aviso `NodeInfo em Ns` (N segundos) e a difusão sai automaticamente quando o tempo acabar. Pressionar de novo não enfileira uma segunda.

### Linha de identidade

Abaixo dos itens, em verde: `ID !xxxxxxxx  chave aabbccdd`.

- `ID !xxxxxxxx` é o seu ID de nó. É assim que os outros veem você nas listas de nós deles.
- `chave aabbccdd` é uma impressão digital da sua chave **pública**: os 4 primeiros bytes dela em hexadecimal. Serve para conferir que a sua chave não mudou depois de reiniciar ou atualizar. A chave privada nunca é exibida.
- Se não foi possível gravar a chave na flash, essa área mostra `ERRO: chave não gravada na NVS!` em vermelho. Veja [Solução de problemas](#solução-de-problemas-todos-os-status-e-mensagens).

Sair da tela Config (`Tab` ou `` ` ``) salva uma mudança de região pendente. Pressionar `Tab` durante a edição de um item descarta a edição.

## Saudação automática

A saudação automática envia a sua saudação como DM para cada nó novo, **uma vez por nó, para sempre**.

- **Ligue ou desligue com `A` na tela Nós.** A barra de status mostra `AUTO:ON` ou `AUTO:OFF`. Não há item na Config para isso.
- Ela fica **DESLIGADA após toda inicialização**, de propósito, para que o aparelho nunca comece a cumprimentar pessoas por conta própria.
- Ela só cumprimenta nós cuja **chave pública é conhecida**, ou seja, nós dos quais já chegou um NodeInfo com chave. Uma DM precisa da chave. Nós em cinza não são cumprimentados até a chave deles chegar.
- Ao ligá-la, os nós com chave já ouvidos desde a inicialização também entram na fila, os mais recentes primeiro. Depois disso, um nó entra na fila quando o NodeInfo dele chega.
- No máximo **uma saudação a cada 30 segundos**. A primeira saudação não é atrasada.
- A fila comporta no máximo **10 nós**. Nós que chegam com a fila cheia não entram nela naquele momento. Eles entram de novo na próxima vez que o NodeInfo deles for ouvido (os nós normalmente se anunciam a cada 3 horas).
- **Nunca duas vezes**: a lista de cumprimentados fica na flash (até 500 IDs de nó) e sobrevive a reinicializações e atualizações. Enviar a saudação exata manualmente pela tela de conversa também adiciona o nó à lista. Quando a lista chega a 500 nós, a saudação automática para de cumprimentar nós novos em vez de esquecer os antigos.
- Nós cumprimentados mostram `+` na lista de nós.
- Desligar a saudação automática limpa a fila. Os nós já cumprimentados continuam marcados.

As saudações aparecem na conversa de cada nó como qualquer DM que você envia, com os [marcadores](#marcadores-de-mensagem) de sempre.

## Marcadores de mensagem

As suas mensagens começam com um marcador de estado seguido de `>`. As recebidas começam com `<`.

| Marcador | Estado | Significado |
|---|---|---|
| `~` | na fila | Esperando na fila de transmissão (ou pelo orçamento de duty cycle) |
| `.` | enviada | O rádio a transmitiu. Para uma DM, a confirmação (ainda) não chegou. Mensagens do canal público ficam em `.` porque o canal não tem confirmações. |
| `v` | confirmada | O ACK do destinatário voltou: a DM foi entregue e decifrada |
| `?` | esperando chave | A chave pública do destinatário é desconhecida. A DM fica retida até a chave chegar (veja abaixo). |
| `x` | falhou | A transmissão falhou (`Falha no TX`: o rádio informou um erro durante o envio, ou não estava pronto), o destinatário respondeu com erro (NAK) ou a chave estava ausente no momento do envio |

**O estado `?`.** Uma DM só pode ser criptografada com a chave pública do destinatário, que chega no NodeInfo dele. Quando você envia para um nó sem chave:

1. A mensagem é guardada com `?` e aparece o aviso `Aguardando chave do no`.
2. O aparelho envia **o seu próprio NodeInfo diretamente para aquele nó, pedindo o NodeInfo dele em troca** (no máximo uma vez por minuto por nó).
3. Quando o NodeInfo do nó chega, todas as mensagens `?` para ele mudam para `~` e são enviadas.

Se o nó estiver fora de alcance ou não responder, a mensagem fica em `?`. Reenviar ou pressionar `Enviar NodeInfo` pode ajudar quando o nó estiver alcançável.

**Sem reenvio automático.** Se uma DM fica em `.` (sem ACK), o firmware não a envia de novo. Digite novamente se precisar.

## Quando o dispositivo escuta e quando transmite

**O rádio está sempre escutando**, exceto quando transmite um quadro próprio. Ele nunca retransmite pacotes de outros nós: o aparelho se anuncia com o papel `CLIENT_MUTE` e não redifunde nada que ouve. Todos os seus quadros usam limite de saltos (hop limit) 3, então outros nós da malha podem retransmiti-los, como é normal no Meshtastic.

Ele transmite somente nestes casos:

| Quando | O quê | Limite |
|---|---|---|
| Na inicialização | Difusão de NodeInfo **com want_response** (os vizinhos respondem com o NodeInfo deles) | Uma vez por inicialização |
| A cada 3 horas | Difusão de NodeInfo | 3 h depois da difusão de NodeInfo anterior |
| Você muda o nome longo ou curto | Difusão de NodeInfo | No máximo uma a cada 30 s (compartilhado com a inicialização e com "Enviar NodeInfo") |
| Você seleciona `Enviar NodeInfo` | Difusão de NodeInfo | No máximo uma a cada 30 s |
| Você envia uma mensagem | Texto público (Canal) ou DM criptografada com want_ack (Conversa) | Quando você pressiona Enter |
| Você envia uma DM para um nó sem chave | NodeInfo enviado diretamente para aquele nó com want_response | No máximo uma vez por minuto por nó |
| A saudação automática está LIGADA | DM de saudação para um nó que acabou de ter a chave conhecida | Uma a cada 30 s, uma única vez por nó |
| Você recebe uma DM que pede ACK | ACK para o remetente (e de novo se o remetente retransmitir a mesma DM) | Uma vez por DM recebida |
| Um nó pede o nosso NodeInfo (want_response) | NodeInfo para aquele nó | No máximo uma vez a cada 30 s no total **e** uma vez a cada 12 h por nó |
| Um nó nos envia uma DM criptografada mas não temos a chave dele | NAK (`PKI_UNKNOWN_PUBKEY`) mais o nosso NodeInfo com want_response, para que a próxima tentativa dele possa ser decifrada | No máximo uma vez a cada 30 s no total e uma vez a cada 10 min por nó |

Regras que valem para toda transmissão:

- **Um quadro de cada vez**, com pelo menos **1.5 s** entre o fim de um quadro e o início do próximo. No máximo 6 quadros esperam na fila de transmissão.
- Cada quadro LoRa ocupa o ar por cerca de **0.4–2.2 s** conforme o tamanho, tipicamente cerca de **1 s** (veja [tempo no ar](frequencies.md#tempo-no-ar)).
- Nas regiões com duty cycle, os quadros esperam pelo [orçamento de tempo no ar](frequencies.md#ciclo-de-trabalho-e-orçamento-de-tempo-no-ar).
- Wi-Fi e Bluetooth nunca são ligados.
- Enquanto um quadro está sendo transmitido, o aparelho não reage às teclas (por até cerca de 2.2 s).

## Por que nenhum nó aparece

A lista de nós só é preenchida quando o aparelho ouve quadros Meshtastic que consegue decodificar. Se `rx` continua em 0, nada está sendo recebido. Se `rx` sobe mas a lista continua vazia, estão chegando quadros que não podem ser decodificados (outro canal ou outra chave) ou que são mensagens diretas de outras pessoas.

Verifique, nesta ordem:

1. **Mesma região e frequência.** Todos os nós precisam usar a mesma região. No Brasil, a comunidade usa **ANZ (919.875 MHz)**, e a região oficial é **BR_902 (903.875 MHz)**. As duas não se ouvem. Pergunte aos usuários locais, ou experimente a outra região e espere. Veja [frequencies.md](frequencies.md).
2. **Mesmo preset.** Este firmware só fala **LongFast**, o padrão do Meshtastic. Nós configurados com outro preset (LongSlow, MediumFast, ShortFast...) usam outros parâmetros de modem e ficam invisíveis.
3. **Canal público padrão.** Nomes de nós, chaves públicas e mensagens de canal só são decodificados no **canal primário padrão** (`LongFast` com a chave padrão `AQ==`). Nós num canal privado ou com chave personalizada são ouvidos (`rx` sobe), mas nada pode ser decodificado. DMs entre outros nós são criptografadas para eles, e você também não consegue lê-las.
4. **Alcance, altura e antena.** O alcance LoRa depende principalmente da altura e da linha de visada. Dentro de casa, no térreo e cercado de prédios, talvez você não ouça ninguém. Vá até uma janela, uma varanda ou um terraço. Confira se a antena está rosqueada e na vertical. Veja [hardware.md](hardware.md#antena-e-alcance).
5. **Dê tempo.** Os nós Meshtastic ficam em silêncio a maior parte do tempo. Com as configurações padrão, um nó envia telemetria a cada **30 minutos** aproximadamente, o NodeInfo a cada **3 horas** aproximadamente e a posição periodicamente se tiver GPS e compartilhamento de posição ligados. O anúncio da inicialização pede aos vizinhos que respondam com o NodeInfo deles, mas só respondem os nós que o ouvirem. Deixe o aparelho ligado por uma hora ou mais num bom lugar.
6. **Anuncie-se.** Use `Enviar NodeInfo` para que os outros conheçam você. Lembre-se do limite de 30 s.

Um nó aparece na lista assim que qualquer quadro decodificável dele é ouvido: telemetria, posição, texto ou NodeInfo. O nome e o modelo dele aparecem, e ele fica branco, só depois que o NodeInfo chega.

## Solução de problemas: todos os status e mensagens

### Barra de status

| Exibido | Significado | O que fazer |
|---|---|---|
| `SEM CAP LoRa` (vermelho) | O expansor de E/S do Cap não respondeu na inicialização, ou a saída da chave de antena não pôde ser configurada | Desligue, confira se o Cap está bem encaixado no conector do Cardputer-Adv e ligue de novo. O firmware não tenta de novo enquanto está rodando. |
| `RADIO ERRO` (vermelho) | O Cap foi encontrado, mas o SX1262 não inicializou | Desligue e ligue. Se persistir, o módulo de rádio pode estar com defeito ou outro firmware o deixou num estado ruim. Regrave e tente de novo. |
| `duty limit` (vermelho) | O orçamento de tempo no ar da região na última hora se esgotou, e há quadros esperando | Nada se perde. Os quadros saem conforme o orçamento libera, dentro da hora. A Config mostra `Ar 1h: ...`. Só EU_868, UA_868 e TH têm orçamento. |
| `-dBm` | Nenhum quadro recebido desde a inicialização | Veja [Por que nenhum nó aparece](#por-que-nenhum-nó-aparece). |

Quando o rádio não está funcionando, as tentativas de transmitir também geram o aviso `Falha no TX` (o rádio não está pronto).

### Avisos (amarelos, 2.5 s)

| Aviso | Significado | O que fazer |
|---|---|---|
| `Config salva` | Configurações gravadas na flash | Nada. |
| `ERRO ao gravar config` | A gravação na flash falhou | A configuração vale até reiniciar, mas pode se perder. Tente de novo. Se continuar falhando, a partição NVS pode estar cheia ou danificada. |
| `ERRO: chave nao gravada!` (na inicialização) | Um novo par de chaves foi gerado, mas não pôde ser guardado | A chave vai mudar na próxima inicialização, e os nós que guardaram a chave antiga vão rejeitar a nova. Verifique a flash/NVS (reinstale). Também aparece na Config como `ERRO: chave não gravada na NVS!`. |
| `ERRO no radio` | Erro do rádio ao mudar a região ou a potência | Desligue e ligue. O rádio pode não estar na frequência exibida. |
| `Fora da faixa do Cap (exp)` | Você selecionou uma região experimental | Tudo bem se for a região do seu país. Espere alcance reduzido. |
| `Falha no TX` | O rádio informou um erro durante o envio, ou o rádio não está pronto (por exemplo, Cap não detectado) | A mensagem é marcada com `x`. Veja se a barra de status mostra `SEM CAP LoRa` / `RADIO ERRO` e desligue e ligue o aparelho. |
| `Texto longo demais` | Mais de 199 bytes | Encurte a mensagem. A entrada pelo teclado já para em 199 bytes. |
| `Fila de TX cheia` | 6 quadros esperando ao enviar para o canal | Espere alguns segundos e envie de novo. Nas regiões com duty cycle, o orçamento pode estar segurando quadros. |
| `Fila de mensagens cheia` | Já há 10 DMs esperando (por exemplo, várias mensagens `?`) | Espere as chaves chegarem ou as mensagens saírem. A DM não foi guardada. |
| `Aguardando chave do no` | DM guardada com `?`, pedido de NodeInfo enviado | Espere o NodeInfo do nó. Veja [o estado `?`](#marcadores-de-mensagem). |
| `NodeInfo em Ns` | Uma difusão de NodeInfo foi pedida menos de 30 s depois da anterior; sai em N segundos | Nada. Sai automaticamente. |
| `Potência: 2 a 22 dBm` | Valor inválido | Digite um número de 2 a 22. |
| `Nome longo: 1 a 24 bytes` | Nome longo vazio ou longo demais | Digite de 1 a 24 bytes. |
| `Nome curto: 1 a 4 letras` | Nome curto vazio ou longo demais (o limite é de 1–4 bytes) | Digite de 1 a 4 bytes (recomenda-se letras/dígitos ASCII). |
| `Saudação inválida` | Por exemplo, caracteres de controle | Digite de novo. Salve um texto vazio para restaurar o padrão. |

### Outras situações

| Situação | Explicação |
|---|---|
| Uma DM fica em `?` | A chave do nó não chegou. Veja [o estado `?`](#marcadores-de-mensagem). |
| Uma DM fica em `.` | Foi transmitida, mas nenhum ACK voltou. O destinatário pode estar fora de alcance, ou o ACK se perdeu. Não há reenvio automático. |
| Uma DM mostra `x` logo depois de `?` virar `~` | A chave do nó sumiu antes do envio (por exemplo, o nó foi removido de uma lista cheia). Envie de novo. |
| Nó cinza sem nome | Quadros dele foram ouvidos, mas o NodeInfo não. Espere, ou abra uma conversa e envie uma DM: isso pede o NodeInfo dele. |
| O cabeçalho mostra `(fora da lista)` | Aquele nó foi removido da lista de 100 nós. As mensagens dele continuam até serem empurradas para fora. |
| Teclas ignoradas por um instante | Um quadro está sendo transmitido (por até cerca de 2.2 s). |
| Mensagens sumiram depois de reiniciar | O histórico de conversas fica só na RAM. Nomes e chaves dos contatos de DM (até 50), a lista de cumprimentados e as configurações são mantidos. |

## Limitações da tela

A tela usa uma fonte embutida de 8 bits (página de código 437). O texto é convertido **só para exibição**:

- Letras que existem na fonte aparecem acentuadas, por exemplo `é ç ô á ü ñ`.
- Letras acentuadas sem glifo aparecem como a letra base, por exemplo `ã → a`, `õ → o`, `À → A`, `Ê → E`.
- Aspas curvas e travessões viram `'`, `"` e `-`.
- Emojis e qualquer outro caractere sem glifo aparecem como `?`. Caracteres invisíveis de junção e seletores de variação são descartados.

**O que é enviado e guardado nunca é alterado.** As mensagens saem exatamente como digitadas (UTF-8), e as recebidas são guardadas intactas. O próprio teclado digita somente ASCII imprimível.

## Limites

| Item | Limite |
|---|---|
| Texto da mensagem | **199 bytes** (UTF-8; letras acentuadas ocupam 2 bytes, emojis 4) |
| Nome longo / nome curto | 1–24 bytes / 1–4 bytes (recomenda-se letras/dígitos ASCII) |
| Saudação | 199 bytes |
| Mensagens por conversa | 30 (as mais antigas são descartadas); 96 no total somando todas as conversas; só na RAM |
| Nós na lista | 100 (o ouvido há mais tempo é descartado) |
| Contatos salvos na flash (nome + chave) | 50 (nós com quem você trocou DMs) |
| Lista de cumprimentados | 500 nós |
| DMs esperando (na fila ou `?`) | 10 |
| Fila de transmissão | 6 quadros |
| Fila da saudação automática | 10 nós |
| Potência de TX | 2–22 dBm, limitada ao máximo da região |
| Reenvios de DM | Nenhum |

## Perguntas frequentes

**Ele capta sinais quando alguém transmite?**
Sim, desde que o remetente esteja na mesma região/frequência e no preset LongFast, e dentro do alcance. O rádio escuta o tempo todo, exceto durante as próprias transmissões curtas. Todo quadro recebido incrementa `rx` e atualiza o RSSI na barra de status, mesmo quando o firmware não consegue decodificá-lo. Só os quadros no canal público padrão, mais as DMs endereçadas a você, são decodificados em nós e mensagens.

**O meu aparelho está transmitindo agora?**
Só nas situações listadas em [Quando o dispositivo escuta e quando transmite](#quando-o-dispositivo-escuta-e-quando-transmite): o NodeInfo da inicialização, a cada 3 horas, depois de uma mudança de nome ou de `Enviar NodeInfo`, as suas próprias mensagens, a saudação automática (se LIGADA), ACKs e respostas com limite de frequência a pedidos de NodeInfo. Ele nunca retransmite o tráfego de outras pessoas. Não há indicador de TX. Enquanto um quadro é enviado, o teclado para de responder por um instante. Nas regiões com duty cycle, a Config mostra o tempo no ar usado na última hora.

**O que é a região ANZ?**
ANZ é a região Meshtastic da Austrália e da Nova Zelândia (915–928 MHz). A frequência LongFast nela é 919.875 MHz. A comunidade Meshtastic brasileira usa ANZ, por isso ela é o padrão deste firmware. A região oficial do Meshtastic para o Brasil é a BR_902 (903.875 MHz). Use a região de onde você está; veja [frequencies.md](frequencies.md#brasil).

**Onde liga e desliga a saudação automática?**
Na tela **Nós**: pressione `A`. A barra de status mostra `AUTO:ON` ou `AUTO:OFF`. Ela sempre fica DESLIGADA depois de reiniciar. Não há item na Config para isso.

**Como forço o envio do meu NodeInfo?**
Config → `Enviar NodeInfo` → `Enter`. Sai na hora, ou depois do que faltar do limite de 30 s (aviso `NodeInfo em Ns`).

**Posso renomear o nó?**
Sim. Config → `Nome longo` (1–24 bytes) e `Nome curto` (1–4 bytes, recomenda-se letras/dígitos ASCII). Os novos nomes são salvos e anunciados com um NodeInfo novo. O seu ID de nó e a sua chave não mudam, então os outros nós continuam reconhecendo você e podem continuar lhe enviando DMs.

**Posso usar com o meu celular ou com o app Meshtastic?**
Não. Bluetooth e Wi-Fi ficam desligados, e o aparelho é configurado só pelo próprio teclado. Ele consegue conversar por LoRa com celulares conectados a outros nós Meshtastic.

**Outras pessoas vão ler as minhas mensagens?**
As mensagens do canal público podem ser lidas por qualquer um com as configurações padrão do Meshtastic, por definição. As DMs são criptografadas de ponta a ponta com a chave pública do destinatário. Veja [security.md](security.md).

**Ele retransmite mensagens de outras pessoas?**
Não. Ele é um nó `CLIENT_MUTE` e nunca redifunde pacotes de outros nós.

**Por que a saudação mostra um `?` na tela?**
O emoji 👋 não tem glifo na fonte da tela. Ele é enviado corretamente. Veja [Limitações da tela](#limitações-da-tela).
