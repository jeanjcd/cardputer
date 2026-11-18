> Traduzido do inglês; em caso de dúvida, vale a [versão em inglês](../en/roadmap.md).

# Roadmap e limitações conhecidas

A v0.1.0 é um nó Meshtastic deliberadamente pequeno. Esta página lista o que ele ainda não faz, ideias para uma v2 e as limitações conhecidas da v1. Nenhuma das ideias é promessa. Contribuições são bem-vindas (veja [CONTRIBUTING.md](../../CONTRIBUTING.md), em inglês).

## Ideias para a v2

| Ideia | Observações |
|---|---|
| **GPS / posição** | Usar o GNSS ATGM336H do Cap (UART, ESP RX G15 / TX G13) para enviar `POSITION_APP` e mostrar a distância até outros nós. O TX LoRa interfere no receptor GNSS, então os dois precisam ser agendados com cuidado. |
| **Mais canais e PSKs personalizadas** | Canais secundários, chaves personalizadas, canais AES-256-CTR (PSKs de 32 bytes), nomes de canal além de `LongFast` e importação de URLs/QR de canal. Hoje só o canal primário padrão é suportado, e `ctrCrypt` é só AES-128. |
| **Outros presets** | LongSlow, MediumFast, ShortFast e os outros presets de modem (a fórmula da frequência já leva em conta o nome do canal e a largura de banda). |
| **Assinatura e verificação XEdDSA** | Assinar os nossos pacotes e verificar pacotes assinados de outros, como faz o firmware Meshtastic mais recente. |
| **Papel de repetidor / roteador** | Retransmitir opcionalmente os pacotes de outros nós (tratamento do limite de saltos, roteamento por next-hop) em vez de `CLIENT_MUTE`. |
| **Reenvio de DM** | Retransmitir uma DM algumas vezes quando nenhum ACK chega, como faz o reliable router do Meshtastic. |
| **Fonte UTF-8** | Uma fonte de tela com glifos de português/espanhol/russo e emojis, além de entrada não ASCII pelo teclado. |
| **Histórico de conversas persistente** | Manter as mensagens entre reinicializações (cartão SD ou NVS, com limite). |
| **TX não bloqueante** | Transmissão por interrupção, para que a interface continue respondendo durante os até 2.2 s de tempo no ar. |
| **Telemetria / traceroute** | Mostrar a telemetria de outros nós e responder ou enviar traceroutes. |
| **Indicador de bateria** | Ler o ADC da bateria, que a v1 não usa. Exige cuidado: a fonte de entropia de RF usada na geração de chaves compartilha hardware com o ADC, então o ADC precisa ser configurado depois da geração de chaves. |
| **MeshCore?** | Suporte ao protocolo MeshCore como malha alternativa. A explorar. |
| **Migração de identidade** | Uma migração versionada do registro da chave, para que uma futura mudança de formato nunca gere uma chave nova. |
| **Esquecer chave do nó** | Uma ação para esquecer a chave de um contato salvo, para que um contato que restaurou o aparelho de fábrica (chave nova) possa ser aceito de novo. Hoje uma chave salva nunca pode ser substituída. |

## Limitações conhecidas da v1

| Limitação | Detalhes |
|---|---|
| **Texto de 199 bytes** | Mensagens, saudações e a linha de entrada são limitadas a 199 bytes de UTF-8. |
| **Glifos da tela** | A fonte CP437 não tem algumas letras acentuadas (exibidas sem acento, por exemplo `ã → a`) nem nenhum emoji (exibidos como `?`). O texto enviado nunca é alterado. Veja o [guia do usuário](user-guide.md#limitações-da-tela). |
| **Teclado só com ASCII** | Só é possível digitar ASCII imprimível. Não dá para digitar o caractere `` ` `` (é o Esc). |
| **O TX bloqueante congela a interface** | Enquanto um quadro é transmitido (até cerca de 2.2 s para 255 bytes), as teclas não são lidas e a tela não atualiza. |
| **Sem repetição de tecla** | Manter uma tecla pressionada não a repete. |
| **Rolagem nas telas de texto** | O histórico rola com `Fn`+`;` / `Fn`+`.`, uma linha por toque. |
| **Sem reenvio de DM** | Uma DM sem ACK fica em `.`. |
| **Mensagens só na RAM** | 30 por conversa, 96 no total, perdidas ao desligar. |
| **100 nós na RAM** | Quando a lista está cheia, sai primeiro o nó sem chave, ouvido há mais tempo, que não é contato salvo; senão, o nó ouvido há mais tempo. As chaves dos contatos de DM salvos (até 50) ficam numa tabela separada, então sobrevivem à saída da lista. |
| **Chaves de contatos salvos não podem ser substituídas** | Se um contato salvo restaurar o aparelho de fábrica e ganhar uma chave nova, ela é rejeitada e as DMs com ele param de funcionar. Ainda não há a ação "esquecer chave do nó". Veja as [Perguntas frequentes do guia do usuário](user-guide.md#perguntas-frequentes). |
| **Conjunto de 500 cumprimentados** | Depois de 500 nós cumprimentados, a saudação automática para de cumprimentar nós novos, para que ninguém seja cumprimentado duas vezes. |
| **Um só canal e preset** | Só o canal primário padrão, só LongFast. |
| **Sem assinaturas** | O XEdDSA não está implementado. Veja [security.md](security.md#o-que-não-é-protegido). |
| **Sem app de celular, Wi-Fi, BLE ou MQTT** | O aparelho é configurado só pelo próprio teclado. |
| **Extração física da chave** | A criptografia da flash fica desligada sob o M5Launcher, então a chave privada na NVS pode ser lida com acesso físico. |
| **Mudança de região perdida em 2 s** | Uma mudança de região é salva 2 s depois da última mudança, ou ao sair da Config. Desligar antes disso a perde. | Nada é transmitido até ela ser salva.
| **Sem detecção automática do Cap em funcionamento** | Se o Cap estiver ausente na inicialização (`SEM CAP LoRa`), é preciso reiniciar o firmware depois de corrigir. |
| **Regiões experimentais** | IN, NZ_865, NP_865, KZ_863, JP, TW e TH ficam fora da faixa nominal de 868–923 MHz do Cap, e o desempenho delas não foi medido. |
| **Interoperabilidade de ponta a ponta ainda não verificada em campo** | Inicialização do rádio, TX, persistência da identidade e a interface foram verificados no aparelho. A troca de DMs e mensagens de canal com outros nós Meshtastic está implementada e coberta por testes contra vetores compatíveis com o firmware oficial, mas ainda não foi confirmada em campo. |
