# DIAL DOS PRIMOS — backlog de ideias

> Ideias e requisitos futuros. Este arquivo **não implementa** mudanças no firmware. Preservar a PR draft, não fazer merge sem autorização.

## 1. Pokémon Companion: rankings, achievements e Favorite Deck

- **Rankings por grupo** Playgroup.gg, nunca ranking global implícito: jogador pode pertencer a vários grupos. Grupo da partida atual ou última partida como padrão, com seletor manual. Exibir nome do grupo, posição, tamanho do grupo, critério do ranking e período.
- **Favorite Deck**: exibir o deck mais utilizado pelo jogador, separadamente do deck de maior win rate; por grupo e/ou intervalo quando houver dados. Possível identidade gráfica do deck.
- **Achievements permanentes**: marcos de 5/10 vitórias, 25 partidas e sequências atingidas; coleção de medalhas desbloqueadas, bloqueadas e progresso.
- **Indicadores dinâmicos**: win rate alto somente após número mínimo de partidas, liderança no ranking e streak atual. Não confundir indicadores que podem expirar com conquistas permanentes.
- **Coroa dourada em pixel art**, flutuando suavemente 2–3 px acima do Pokémon se o jogador for #1 no grupo selecionado. É um acessório independente dos sprites originais; não modificar /mons/p778.bin.
- **Badges de tipos Pokémon estilo Pokédex**: Ghost/Fairy precisam reproduzir a linguagem visual do componente completo (proporção, contorno, brilho/volume, tipografia), não apenas cores chapadas. Usar assets com permissão apropriada ou arte própria.
- **Home**: o Pokémon é protagonista; mostrar poucas conquistas selecionadas, grupo e posição. **STATS/ACHIEVEMENTS**: games, wins, win rate, streak, Favorite Deck, progresso de badges e histórico, caso a API forneça.
- **Integridade:** dados, conquistas e rankings pertencem ao jogador/escopo de grupo; cosméticos independem do Pokémon; nunca exibir valores de demonstração como dados reais. Validar eventos e API Playgroup.gg antes de definir fórmulas, desempates ou prêmios. Indicar cache/offline.

## 2. Identidade visual / tela de inicialização DIAL DOS PRIMOS

Feedback: **a arte da moeda e a tipografia da intro atual estão ruins**. Redesenhar a identidade completa; **nome e símbolo também podem mudar**, desde que a nova proposta seja validada antes de editar o firmware.

Diagnóstico no firmware atual (`knobby/src/intro.c`):
- Fundo preto, moeda central de até 280 px, label "DIAL DOS" em Montserrat 32, texto "P R I M O S" em Montserrat 14, linha dourada; intro ~36 frames x 55 ms.
- A moeda de alta qualidade depende de asset local (`dial_intro_asset.h` / `dial_sprite`), não versionado no GitHub, segundo `docs/visual-assets-v5.md`; não presumir que git pull altere essa arte.
- Há histórico de borda branca/pixelada inferior, tela preta/arte degradada e problema com flip. Evitar reintroduzir esses defeitos.

Direção proposta para validação visual:
- Criar uma **identidade única**: símbolo original (não precisa ser moeda), nome opcionalmente novo, linhas e silhueta legíveis na tela redonda 360×360.
- Explorar novas opções de nome e lettering, evitando texto genérico quebrado em Montserrat com espaçamento artificial; garantir leitura no hardware.
- Duas ou três propostas **visuais completas**, comparadas em tamanho real (360 px) antes de substituir assets.
- Preferir intro estática de alta qualidade com movimento sutil, sem flip obrigatório, sem aumento por interpolação e sem bordas de asset.
- Garantir consistência de composição, safe-area circular, anti-aliasing/rgb565 e budget de RAM/flash.
- Não modificar Magic gameplay, arte da moeda em outros modos, timings de inicialização ou assets locais sem inspeção e validação física.

## Prioridade e sequência

1. **Sprite Mimikyu: visibilidade confirmada pelo usuário após o upload de dbde848.** Ajuste fino de posição (um pouco mais abaixo) fica para outra rodada; não regredir o decodificador RGB565+alpha.
2. **Próxima frente: Pokébolas / captura.** O modelo já contém inventário Poke/Great/Ultra/Master, EXP, níveis e Pokédex em `pokemon_progress.h`, mas a UI segue como preview e as recompensas reais de pós-jogo não estão habilitadas. Desenhar experiência de abrir/capturar/consultar inventário sem premiar jogadores ficticiamente.
3. Validar fluxo do pós-jogo com usuário Playgroup mapeado e resultado durável. Somente ativar concessão de Pokébolas após revisar transações, recuperação de energia, persistência e deduplicação.
4. **Achievements e leaderboard:** investigar endpoints e eventos reais Playgroup.gg; implementar rankings por grupo e Favorite Deck antes de mostrar métricas reais; medalhas/coroa como fase posterior.
5. **Visual:** badges de tipos com acabamento fiel à Pokédex; baixar alguns pixels o sprite em teste futuro; criar/validar propostas de **nova marca, nome e intro**, sem alterar firmware durante teste atual.

**Nota de estado:** sprite visível no hardware; Pokébolas, achievements, leaderboard e Favorite Deck **ainda pendentes** de integração real. Este arquivo apenas documenta a fila, sem habilitar recompensas nem alterar a interface.


## 3. Gacha de Pokébolas — pesquisa inicial

- Mecânica escolhida: abrir a bola sempre entrega um Pokémon; não há encontros nem falhas de captura.
- Arte: quatro bolas (Poké, Great, Ultra, Master) com mesma resolução-base, perspectiva, sombreamento e quadros de animação. Referências: https://www.spriters-resource.com/game_boy_advance/pokemonfireredleafgreen/asset/20104/ ; https://www.spriters-resource.com/ds_dsi/pokemonblackwhite/asset/39327/ ; https://codepen.io/simeydotme/pen/QWdBOwx . Licenças de redistribuição ainda não confirmadas.
- Taxas oficiais de captura NÃO correspondem ao gacha: Poké 1x, Great 1.5x, Ultra 2x, Master garante captura (https://www.serebii.net/games/capture.shtml).
- **Proposta de gacha, não oficial:** Poké Ball comum 75%, raro 20%, épico 4,5%, lendário 0,5%; Great Ball 55%, 30%, 13%, 2%; Ultra Ball 30%, 40%, 24%, 6%; Master Ball 0%, 0%, 0%, 100%. Proposta sujeita a rebalanceamento.
- Shiny sorteado separadamente. Definir pools reais, recompensas por partida, duplicatas e economia. Sortear/salvar resultado e consumo de bola atomicamente antes da animação; proteger contra queda de energia e reabertura duplicada.


## 4. Master Ball — decisão atual

- **Master Ball garante Pokémon lendário** da pool disponível, com shiny sorteado separadamente.
- **Achievement permanente `Master Trainer`:** o primeiro Pokémon do jogador a atingir o nível 100 desbloqueia a badge e concede exatamente **1 Master Ball**, uma única vez por jogador (não uma vez por Pokémon).
- Os níveis 100 seguintes não concedem outra Master Ball pelo mesmo achievement.
- **Não implementar drop aleatório de Master Ball por vitória por enquanto.** A ideia anterior de 0,1% foi rejeitada como inadequada ao ritmo de vitórias em Commander; reconsiderar somente após medir ritmo real de progressão e economia.
- Concessão da badge e de sua recompensa deve ser idempotente, durável e recuperável após queda de energia.
- Pendente: decidir duplicatas de lendários, pool instalada, taxa de EXP/partida e outros achievements excepcionais que possam premiar Master Ball no futuro.


## 5. Balanceamento da experiência — simulação isolada (ainda NÃO em produção)

- **Problema auditado:** fórmula atual `xp_for_next(level)=100+25*level`; total nível 1→100 = **133.650 EXP**. Ganhos atuais: derrota 50, vitória 75. Isso exigiria cerca de **2.673 derrotas ou 1.782 vitórias**, totalmente inadequado para partidas longas de Commander.
- **Simulador versionado:** `tools/pokemon/xp_balance.py`, executar `python tools/pokemon/xp_balance.py --self-test` e `python tools/pokemon/xp_balance.py`. Não modifica firmware nem perfis.
- **Preset para discussão:** `xp_for_next(level)=30+6*level`; total **32.670 EXP**; derrota = **250 EXP**, vitória = **350 EXP**. Aproximadamente 119 partidas com 25% de vitórias, 109 com 50%, 94 se ganhar todas. Supõe toda EXP num mesmo Pokémon e partidas válidas; avaliar a rotina real do grupo antes de ativar.
- **BUG/REGRA EM ABERTO:** o motor atual premia **cada Pokémon** que chega a 100 com Master Ball; deve ser substituído por achievement **único por jogador** (`Master Trainer`) com 1 Master Ball, persistido de forma transacional. **Não ativar** até definir armazenamento/migração e garantir concessão idempotente.
- **Migração:** alteração de fórmula altera progressão dos perfis existentes. Auditar e testar migração de EXP atual/nível antes de mudar código em produção, e não zerar avanços.
- Qualquer modificação que afete recompensas deve aguardar validação no aparelho e integração segura pós-jogo, sem considerar testes simulados prova suficiente.


### Revisão de balanceamento — 05/11/2026

Feedback: ~105 partidas para nível 100 é excessivo em Commander; 100% de vitórias deve chegar ao marco em menos da metade desse total. **Nova proposta preferida** para teste (substitui o preset anterior de 250/350 e 30+6×nível):

- EXP necessária para subir do nível L para L+1: **20 + 4×L** (total de **21.780 EXP** do nível 1 ao 100).
- **Derrota/participação validada: 300 EXP**; **vitória validada: 800 EXP**.
- Número esperado, aproximado, de partidas quando toda EXP vai a um único Pokémon: **73 com 0% de vitórias; 52 com 25%; 40 com 50%; 28 com 100%**.
- O resultado ainda é **simulação/proposta**, não regra aplicada no firmware. Simulador em `tools/pokemon/xp_balance.py` atualizado.
- Milestone `Master Trainer`: **primeiro Pokémon a nível 100**, uma única badge e uma única Master Ball por jogador. Confirmar e implementar de forma segura antes de ativar recompensas reais.
- A EXP da vitória substitui (não soma a) recompensa-base de derrota; evitar contar duas vezes.


## 6. Master Trainer — carteira isolada, sem premiação ativa

- **Criado:** `knobby/src/pokemon_master_trainer_store.{h,cpp}`, um armazenamento SPIFFS próprio por Playgroup user ID. Não altera o layout do perfil Pokémon versão 1 nem interfere no Magic.
- **Estado persistido:** conquista `Master Trainer` desbloqueada, ID da partida que causou o primeiro nível 100 e **um direito de Master Ball** com estado de utilização. A Master Ball da conquista NÃO é adicionada ainda a `profile.balls[PK_BALL_MASTER]`; esta carteira precisará ser integrada como fonte de saldo quando o gacha existir.
- **Integridade:** dois slots verificados com geração/checksum. Em caso de slot truncado ou corrompido, o sistema falha fechado em vez de usar o registro anterior que poderia ressuscitar uma recompensa já usada. Sem formatação nem exclusão automáticas.
- **API voluntária:** `pk_mt_unlock` exige uma transição primeiro nível 100 e ID novo presente na partida confirmada; `pk_mt_inspect` consulta estado; `pk_mt_mark_spent` marca o único direito utilizado (não chamar até existir revelação do Pokémon com persistência conjunta).
- **Testes:** `tests/pokemon_master_trainer_store_test.cpp` exercita unlock único, retomada do estado, consumo único, danos de arquivo, interrupção de escrita, IDs independentes e falhas de montagem, com mocks SPIFFS. Workflow Pokémon atualizado para rodá-los.
- **Ainda falta antes de habilitar:** regra de persistência conjunta entre resultado confirmado, perfil XP, achievement e gacha; evitar perder a conquista se houver desligamento entre salvar perfil e gravar achievement; definir tratamento operacional para corrupção; testar recuperação no aparelho; integrar saldo da carteira e resultado da abertura de forma atômica. **Nenhuma recompensa real automática foi ligada.**


## 7. Recuperação Master Trainer após queda de energia — nova fase isolada

- Criado `pokemon_master_trainer_intent.{h,cpp}`: uma prova de transição nível 99→100 (snapshots antes/depois + game ID + checksum) guardada em SPIFFS antes de salvar o perfil da partida.
- `pk_mt_intent_prepare()` cria o registro imutável uma vez e não sobrescreve registro incompleto, corrompido ou pertencente a outra partida.
- `pk_mt_intent_finalize()` exige **COMMITTED** no ledger E perfil persistido idêntico à transição preparada; só então chama a carteira Master Trainer para disponibilizar a recompensa única. Tentativas de repetição reconhecem a mesma badge sem recriar Master Ball já gasta.
- Testes isolados de ausência de commit, perfil não persistido, repetição, dados incompatíveis, escrita interrompida e carteira corrompida estão em `tests/pokemon_master_trainer_intent_test.cpp` e no workflow.
- **NÃO CONECTADO AINDA:** integrar a preparação *antes do save do perfil* e a finalização *depois do commit* no coordenador de pós-jogo, sem fazer isso em uma janela que deixe um PREPARED insolúvel. Ainda precisa provar recuperação com queda real de energia, política de corrupção/storage cheio, e consumo atômico da Master Ball com revelação Pokémon no gacha.
- Evitar consumir a carteira por `pk_mt_mark_spent` até a transação de gacha garantir que o Pokémon recebido está salvo. Sem alterações no firmware instalado nem prêmios automáticos.


## 8. Coordenador XP + Master Trainer opt-in (ainda não ativo no pós-jogo)

- Criado `pokemon_reward_master_coordinator.{h,c}`: fluxo opt-in que consulta journal e intent do jogador, calcula EXP, prepara a prova Master Trainer **antes de PREPARED e do save**, salva perfil, registra COMMITTED e finaliza a badge. Nos demais jogos, não cria intent.
- Em reinício depois do save mas antes de COMMITTED, só recupera se a partida já estiver no perfil. Antes de salvar a partida, permanece bloqueado se o estado for ambíguo. Nunca reaplica EXP adivinhando. Antes de processar partidas seguintes, exige concluir a concessão pendente.
- Criado `pokemon_reward_master_storage.{h,cpp}`: adaptador explícito aos métodos atuais NVS + SPIFFS. **Nenhuma chamada inserida no pós-jogo ou nas telas do DIAL.** Continua sendo função de opt-in para teste controlado após conferir resultado durável e identificação.
- Teste `tests/pokemon_reward_master_coordinator_test.c` cobre sequência `intent prepare → ledger PREPARED → profile save → COMMITTED → badge`, falha de preparação, falha de gravação, falta de commit, retomada, repetição, carteira já utilizada e partidas subsequentes.
- **Riscos ainda abertos:** procedimento de recuperação de PREPARED cuja gravação do perfil falhou; corrupção ou escrita interrompida na carteira/intent; migração futura NVS→SPIFFS; testes de power-cycle reais; resultado confirmado do Playgroup; consumo de Master Ball e persistência de Pokémon no mesmo sorteio. Não ativar premiações nem enviar build ao DIAL como se fossem validadas no hardware.


## 9. Sorteador de gacha puro (sem consumo de Pokébolas)

- Implementado `pokemon_gacha.{h,c}`: sorteio isolado de Pokémon a partir de catálogo explícito de espécies com sprite instalado, tier e estado de disponibilidade. **Não altera perfil, Pokébolas, arquivos ou UI**.
- Taxas inicialmente adotadas (não oficiais): Poké 75/20/4,5/0,5%, Great 55/30/13/2%, Ultra 30/40/24/6%, Master 0/0/0/100% [comum/raro/épico/lendário].
- Sorteio de tier seguido de espécie uniformemente dentro do tier e shiny separado (chance **provisória** 1/4096). RNG deve vir de fonte segura fornecida pelo chamador; algoritmo evita viés de módulo.
- Preflight `pk_gacha_catalog_ready` exige pelo menos uma espécie com sprite instalado em **cada** tier com probabilidade positiva na Pokébola. **Apenas Mimikyu confirmado no DIAL não permite abertura ainda**; mostrar indisponível em vez de consumir bola sem entrega.
- Testes `tests/pokemon_gacha_test.c` verificam limites das faixas, Master lendário, shiny separado, catálogo incompleto, duplicatas, falha de RNG, rejeição sem viés e não mutação da saída em caso de falha. Conectados ao workflow Pokémon.
- **Próximas etapas:** escolher e instalar catálogo inicial com pelo menos uma espécie por tier, definir regra de duplicatas/shards, criar registro transacional de abertura (consumo da bola + prêmio persistido) e assets/animações homogêneos das quatro Pokébolas. Só então conectar botões e abertura reais.


## 10. Catálogo real de sprites para habilitar gacha (fase preparatória)

- Criado `tools/pokemon/catalog.json` com **apenas Mimikyu #778**, a única espécie confirmada no DIAL, em categoria *rare* provisória (raridades são internas ao gacha).
- Criado `tools/pokemon/audit_catalog.py`: inspeção **local e somente leitura** da pasta `/mons` (espelho do SD). Valida cabeçalho e estrutura TPK2, Idle e limites do renderer 3× atual; detecta arquivo ausente/inválido e impede liberar uma Pokébola sem pelo menos uma espécie válida em cada tier com chance positiva.
- Criados testes com TPK2 sintético em `tests/test_pokemon_catalog.py`, adicionados ao workflow Pokémon; não incluem sprites de terceiros.
- O auditor é uma ferramenta de preparação **offline**, não uma consulta em tempo real ao SD pelo firmware. O sorteador permanece isolado e nenhum botão abre ou consome Pokébolas.
- **Próximos passos:** obter/instalar espécies com licenças adequadas em todos os quatro tiers; tornar o carregador LVGL genérico por espécie (atualmente hardcoded `p778.bin`); transferir/validar assets sem sobrescrever existentes; integrar sorteio e gasto com persistência; produzir sprites de abertura consistentes para as quatro bolas.


## 11. Renderizador TPK2 por espécie (sem seleção automática)

- O renderizador antes fixo em `/mons/p778.bin` agora expõe `pk_tpk2_idle_load_species(uint16_t species)`, que busca o asset `/mons/p<ID>.bin` para espécies 1–1025, mantendo `pk_tpk2_idle_load()` como entrada compatível para Mimikyu #778.
- `pk_tpk2_idle_loaded_species()` informa o ID carregado; `pk_tpk2_idle_release()` libera o cache. O carregamento continua read-only e desmonta SD. Não há escrita nem criação de arquivos.
- Novo helper puro `pokemon_sprite_path.{h,c}` evita aceitar caminhos livres ou IDs fora da Pokédex. Testes em `tests/pokemon_sprite_path_test.c`, incorporados ao CI.
- **Limitação importante:** o cache de frames LVGL é compartilhado; antes de trocar espécies, a interface precisa pausar animações e remover referências aos frames anteriores para evitar ponteiros inválidos. Por isso a UI atual segue usando **somente Mimikyu**, sem seleção automática e sem alterar a posição/arte.
- **Pendências:** validação do novo carregador no ESP32; auditoria real dos arquivos no SD; escolha da espécie selecionada a partir de coleção persistida; troca segura de frames e layout dinamicamente, incluindo nome, tipo e bioma; transação de abertura de gacha antes de qualquer sorteio real.
