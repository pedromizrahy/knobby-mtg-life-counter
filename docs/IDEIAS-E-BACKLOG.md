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


## 12. Transferência USB multi-sprites — implementação preliminar

- `PK PMD PUT <dex> <size> <crc32>` agora aceita species 1–1025 via `pk_pmd_put_species` e mantém `PK PMD PUT 778` compatível. O parser está em `playgroup_api.cpp`, conservando o bloqueio durante partida ativa.
- Destino `/mons/p<ID>.bin`, temporário `/mons/p<ID>.part`; ambos gerados a partir de ID numérico e nunca de caminho livre. Não sobrescreve destino existente, não remove temporário de execução anterior e não formata SD.
- `tools/pokemon/upload_batch.py`: valida TPK2 e CRC32; **dry-run padrão**, `--execute` explícito para enviar; exclui Mimikyu #778 por padrão. Testes de plano e protocolo em `tests/test_pokemon_batch_upload.py`.
- O primeiro lote `candidate_pool.json` tem 14 candidatos, mas não significa que seus arquivos estão instalados. Nenhum asset foi distribuído no repositório nem transferido para hardware nesta etapa.
- **Pendências antes de upload:** revisão do build ESP32, espaço livre real no SD, testes físicos de leitura/escrita e de reinício após upload interrompido, validação no aparelho dos formatos, proteção de gameplay em janela serial, seleção dinâmica de sprites, consumo transacional no gacha.


## 12. Troca segura do sprite no LVGL (API opt-in; Mimikyu continua padrão)

- `pokemon_ui_show_species(uint16_t)` expõe uma seleção **programática** para Pokémon com TPK2 instalado. Não existe ainda botão/lista de coleção nem integração ao gacha. Só chamar na thread LVGL, fora de partidas reais.
- Antes de carregar outra espécie: pausa timer Idle, oculta imagem, destaca `lv_img_set_src(..., NULL)` e invalida todos os descritores antigos do image cache; em seguida o renderizador pode liberar buffers PSRAM e recarregar. Na falha tenta recuperar a espécie anterior, depois Mimikyu se houver arquivo. A falha não altera coleção/EXP ou Pokébolas.
- Cabeçalho e etiqueta de tipo ficam neutros para outras espécies (`POKEMON #ID` / `TYPE: NOT LINKED`), não inventam nomes/tipos. Mimikyu mantém `MIMIKYU #778` e `GHOST / FAIRY`.
- `pokemon_ui_open_companion()` permanece no Mimikyu #778, sem trocar a home automaticamente. A posição e o estilo do Mimikyu não foram alterados.
- **Pendente:** teste de compilação/execução do LVGL no firmware e troca real no ESP32, instalação de outro asset TPK2, fonte verdadeira de coleção/seleção, renderizar nomes/tipos consultando catálogo real. Os testes de motor Pokémon não compilam a tela LVGL nem comprovam ausência de artefatos no display.


## 13. Seleção por coleção real (gate puro, sem usuário automático)

- Criado `pokemon_companion_selection.{h,c}`: `pk_companion_is_owned(profile,id)` valida que espécie 1–1025 consta nos **slots ocupados do time** do perfil v1. O SD conter sprite ou a Pokédex marcar uma descoberta **não** implica que o Pokémon pertence ao jogador. Um futuro box persistente exigirá outro modelo de seleção.
- `pk_companion_next_owned(profile,from,direction)` percorre as espécies existentes no time em ambas as direções com wrap-around, sem modificar dados, para futura seleção no dial.
- `pokemon_ui_show_owned_species(profile,id)` é chamada LVGL explícita que exige a checagem de coleção antes de tentar carregar o sprite instalado; não carrega usuário, não salva nada e não é invocada automaticamente. O caminho antigo `pokemon_ui_show_species` permanece para **preview/diagnóstico**; não usá-lo como caminho de seleção autenticada.
- Teste unitário `tests/pokemon_companion_selection_test.c` verifica coleção vazia, espécies do time e espécies apenas descobertas, wrap, entradas inválidas e compatibilidade de perfil. Workflow Pokémon atualizado; build Arduino deve confirmar inclusão do novo módulo.
- **Ainda falta:** identificar jogador Playgroup de forma durável, ler perfil real com autorização, definir semantics de posse (Pokédex atual marca descoberta, não fornece múltiplas unidades nem equipe armazenada completa), instalar um segundo TPK2 validado, testar troca visual no ESP32, e garantir timer/cache LVGL estáveis. Sem botão de seleção e sem premiações automáticas.


## 14. Preview manual de Pokémon salvo do jogador (NVS, sem identidade automática)

- Criado `pokemon_companion_profile_view.{h,c}`, que recebe um **user ID Playgroup global previamente autenticado** e um callback de leitura de perfil; não reconhece usuário por assento, nome, guest ID ou valor inventado. Seleciona somente espécies ocupantes do time do perfil v1; flags de descoberta na Pokédex NÃO significam posse. Retorna nível/EXP/Shiny reais sem mutações.
- A função explícita `pokemon_ui_show_saved_species(verified_user_id,species)` consulta o armazenamento NVS existente por `pk_store_load`, valida posse, carrega sprite TPK2 do SD e mostra `LV n / XP n` somente depois de leitura e desenho bem-sucedidos. Caso contrário, não fabrica dados.
- `pokemon_ui_open_companion()` continua abrindo **Mimikyu #778 sem leitura de usuário**. Não há associação automática com identidade Playgroup, nem controles de navegação, nem seleção durante gameplay, nem acesso a equipe alheia autorizado pelo dispositivo.
- Testes `tests/pokemon_companion_profile_view_test.c` comprovam isolamento por user ID, validação de espécie, dados reais, ausência de mutação de saída na falha e bloqueio de perfis inválidos. CI Pokémon testa o bridge; compilação Arduino verifica a chamada de armazenamento/LVGL.
- **Crítico antes de exibir para jogadores reais:** determinar uma identidade autenticada e vinculada ao Playgroup, evitar leaks entre jogadores no dispositivo compartilhado, obter política para convidados/usuários offline, sincronizar storage entre NVS e eventual SPIFFS, e validar na placa a troca PSRAM + timer LVGL. A função atual é apenas preview manual, não autenticação.


## 15. Pokémon LAB — perfil de laboratório independente (2026-11-06)

- Novo modo de desenvolvimento **PK LAB** acessível somente por comandos USB serial, **fora de qualquer jogo Playgroup ativo**. Não está vinculado à conta Playgroup, não usa user ID, não gera registros NVS/SPIFFS/SD, não toca na carteira Master Trainer real nem envia eventos à API. Ao desligar/reiniciar, a sessão some. Chamadas não são feitas pelo pós-jogo.
- Módulo puro `pokemon_lab.{h,c}` mantém estado de laboratório em RAM: **Poké/Great/Ultra/Master ilimitadas**, quantidade de sorteios por bola, quantidades por tier, frequência observada em %, Shiny, último sorteio, nível simulado 1–100 e badge Master Trainer acionável no laboratório. `PK LAB RESET` elimina tudo; `PK LAB START` inicia do zero.
- **Importante:** os draws são estatísticos e usam um pool **SINTÉTICO** com um ID de amostra para cada tier: Poke 25/common; Mimikyu 778/rare; Dragonite 149/epic; Mewtwo 150/legendary. Os IDs/tier são apenas fixtures de simulação. **Isso não instala sprites nem concede Pokémon à coleção.** O motor de gacha real permanece bloqueado sem catálogo/entrega transacional. RNG do aparelho usa `esp_random()`.
- Comandos (monitor serial 115200): `PK LAB HELP`, `PK LAB START`, `PK LAB DRAW POKE 1000`, `PK LAB DRAW GREAT 1000`, `PK LAB DRAW ULTRA 1000`, `PK LAB DRAW MASTER 1000`, `PK LAB BADGE MASTER`, `PK LAB LEVEL 100`, `PK LAB STATUS`, `PK LAB RESET`. Cada lote limitado a 1000 sorteios por comando para não travar a UI; histórico no laboratório não é limitado pelo inventário.
- Teste `tests/pokemon_lab_test.c`: início/reset, badge, níveis, cada tier de cada bola, Master garante lendário, 1000 draws e falha de RNG sem aplicar lote parcial. CI Pokémon compila e roda no host. Nenhuma alteração no perfil do usuário real.
- **Falta desenvolver** interface gráfica acessível pelo menu de Pokémon (escolher laboratório, abrir bolas e ver badges/taxas em 360x360) e catálogo de sprites reais. O laboratório atual é uma ferramenta de teste serial, ainda não uma conta visual ou personagem/usuário no Playgroup. Rodar no hardware somente após CI Arduino PASS e upload manual.


## 16. Tela visual TEST LAB no DIAL (sem Playgroup, 2026-11-06)

- Criado `pokemon_lab_runtime.{h,cpp}`: uma **única sessão do simulador na RAM** para os comandos USB `PK LAB` e a nova tela. Acesso ao estado e sorteios é serializado por mutex FreeRTOS. Usa `esp_random()`. Não acessa SPIFFS, NVS, SD, identidade Playgroup ou coleção real. Não persiste após reiniciar.
- Criado `pokemon_lab_ui.{h,c}`: tela LVGL 360×360, acessada exclusivamente pelo botão **TEST LAB** dentro de **Pokémon → STATS**. A tela mostra nível e Master Trainer simulados, seletor POKE/GREAT/ULTRA/MASTER, sorteio de 1 ou 100 por toque, último resultado **sintético** e estatísticas por tier e Shiny; botões BACK, LV100 e RESET. **Não existe consumo real de bolas, imagens de gacha ou revelação de sprite nesta tela.**
- `PK LAB START / DRAW / STATUS / BADGE / LEVEL / RESET` agora usam a mesma sessão. Um `PK LAB DRAW POKE 1000` no serial será contabilizado na UI ao abrir/reabrir o LAB. O botão RESET inicia a simulação do zero.
- A tela LAB **não aparece dentro do jogo Magic**, não usa dados de usuário do Playgroup e não foi vinculada a um assento. É uma ferramenta de teste para inspecionar sorteios/rates/badges no hardware.
- **Antes de upload:** conferir compilação Arduino da nova tela e, depois, validação visual/touch no ESP32-S3 (geometria circular, event handlers, heap, saída da tela e retorno ao companion). Testes host do motor não comprovam aparência ou fluidez.
- **Etapas futuras:** arte pixelada original e animação de Pokébola, carregamento de sprites reais dentro do laboratório, gráficos de taxa por amostra, histórico e filtros, e opção de perfil de testes persistente (se desejado) em namespace próprio, sem conectar ao Playgroup.


## 17. Animação pixelada de abertura no TEST LAB (2026-11-06)

- Adicionados `pokemon_lab_ball_art.{h,c}`: quatro Pokébolas desenhadas por código, pixelizadas em uma grade lógica de 32×32 ampliada 2× para 64×64 (RGB565 + alpha), com seis quadros: fechada, movimento, tampa abrindo, iluminação, brilho e revelação. Não são imagens oficiais externas ou sprites de Pokémon.
- A tela `pokemon_lab_ui.c` mostra a bola selecionada e executa a sequência em timer LVGL de 140 ms, usando buffer persistente de 12 KiB, sem alocações por frame. O desenho da bola é apenas visual e não altera RNG, perfis, ledger ou inventário.
- **Um toque = um único sorteio** (`x1` ou `x100`) antes da animação; novos draws, troca de bola, badge e reset ficam bloqueados enquanto a sequência ocorre. Retornar da tela pausa o timer sem repetir nem reverter sorteios registrados. Ao terminar, o resultado sintético e as porcentagens ficam visíveis.
- Testes `pokemon_lab_ball_art_test.c` verificam todas as quatro cores e seis frames, rejeição de índices inválidos, transparência e diferenças entre variantes e fases. Adicionados ao workflow Pokémon.
- **Pendente:** validação visual/touch/tempo no ESP32-S3, revisão de layout circular (64×64 é visual provisório, não arte final), artes finais em alta qualidade e efeitos posteriores de Pokémon. LAB é totalmente isolado do Playgroup; gacha real e gasto de Pokébolas continuam desativados.


## 18. Revelação visual congelada e isolamento USB/touch (2026-11-06)

- Foi confirmado CI Arduino e Pokémon Engine Checks **aprovados** na animação anterior (commit `36f8d500`).
- Criado `pokemon_lab_reveal.{h,c}` com rótulos **SINTÉTICOS** das quatro espécies de teste (Pikachu #25 comum, Mimikyu #778 raro, Dragonite #149 épico, Mewtwo #150 lendário), validação rigorosa da combinação ID/tier e destaque de Shiny. Não utiliza sprites reais nem acrescenta à Pokédex.
- A nova chamada `pk_lab_runtime_draw_snapshot(ball,count,out)` registra sorteio e captura resultados sob o **mesmo mutex**. Assim uma segunda abertura via USB não pode trocar a identidade do Pokémon revelado pela UI durante os seis quadros.
- Ao terminar os quadros a tela mostra card de revelação acima da área de estatísticas, com nome, número, raridade e sinalização de Shiny, e **SIM ONLY**. O card utiliza cores por categoria e destaca Shiny. Não é uma arte final, não armazena prêmio, não debita bola.
- Adicionado `tests/pokemon_lab_reveal_test.c` ao CI com casos válidos, Shiny e rejeição de resultados incoerentes ou buffers pequenos.
- Falta validar nova compilação e o novo card no ESP32-S3, incluindo condições de toque e sobreposição na tela circular. Nenhuma alteração nos IDs Playgroup reais nem no pós-jogo.


## 19. Emblemas pixelados do resultado de teste (2026-11-06)

- Compilação Arduino e Pokémon Engine Checks da revelação textual anterior **PASS** no commit `b1f84325`.
- `pokemon_lab_fixture_art.{h,c}` gera quatro emblemas temáticos **originais de laboratório**, 40×40 RGB565+A, sem arte externa: raio para a fixture Pikachu #25, fantasma abstrato para Mimikyu #778, asas estilizadas para Dragonite #149 e símbolo psíquico para Mewtwo #150. Um sinal adicional de brilho é desenhado na variação Shiny.
- A tela `pokemon_lab_ui.c` apresenta o emblema à esquerda do card de revelação, acompanhado de nome, National Dex ID, tier e `SIM ONLY`. Não promete renderização de sprites Pokémon oficiais: são **marcadores visuais de teste**, não criaturas instaladas no cartão SD.
- Testes `tests/pokemon_lab_fixture_art_test.c` validam transparência, quatro espécies distintas, Shiny e entradas inválidas; incluídos no workflow Pokémon.
- Ainda falta validar no ESP32-S3 real o layout circular, as fontes e a transição entre quadros e card; a compilação CI não valida toque nem imagem exibida na tela física. O LAB continua sem posse ou recompensa de Playgroup.


## 20. Exibição de sprite TPK2 real na revelação do TEST LAB (2026-11-06)

- Confirmado CI Arduino e Pokémon Engine Checks **sucesso** no commit anterior `caab0f23`.
- O card do TEST LAB passa a tentar mostrar um **sprite Idle real existente no cartão SD** para um resultado de teste **não Shiny**, por meio do renderizador TPK2 compartilhado com a tela Companion. Se a espécie não tem asset instalado/válido, mantém o emblema original criado pelo firmware. Nenhum Pokémon é adicionado à coleção real.
- O sprite é exibido em tamanho reduzido de aproximadamente 38px, limitado ao espaço do card. **Não confundir com sprite Shiny:** resultados Shiny sempre usam o emblema alternativo do LAB até existir arte Shiny explicitamente validada.
- **Segurança de memória:** antes de o renderizador trocar os buffers PSRAM, o LAB destaca suas próprias referências LVGL; o Companion também destaca suas referências ao trocar espécies. Ao clicar BACK o LAB libera sua referência e solicita o Mimikyu #778 novamente na tela Companion. Não grava SD, não concede premiação nem altera dados Playgroup.
- Esse caminho faz uma consulta SD síncrona durante a finalização da animação. Validar em hardware possíveis pausas de frame, taxa de quadros, proporção, transparência, coexistência com a animação do Companion e comportamento ao SD ausente/corrompido.
- Ainda faltam **assets reais para as demais espécies**, arte Shiny correspondente, seleção real de espécies e validação visual no ESP32-S3. A UI usa emblemas originais enquanto não há assets instalados.


## 21. Idle real animado e indicador de origem no TEST LAB (2026-11-06)

- A interface LAB agora avança pelos frames Idle do TPK2 real, se houver mais de um, usando duração de cada frame. Cria timer LVGL inativo por padrão; a animação só roda com o card de revelação visível, sem modificar os dados do Pokémon.
- Cada mudança de Pokébola, novo DRAW, RESET, entrada ou saída do LAB pausa o timer e destaca a referência à imagem anterior antes de qualquer substituição de buffers PSRAM. Se outra parte mudar a espécie carregada, o timer é interrompido em vez de acessar ponteiros obsoletos.
- O card passa a distinguir explicitamente `SD PREVIEW` (Idle real instalado) de `LAB ART` (emblema sintético). Resultados Shiny continuam exibindo `LAB ART` se não houver sprite Shiny validado.
- Pendente compilar essa atualização e testar no ESP32-S3; verificação de memória e visual/touch na placa é obrigatória. Gacha real permanece desligado; PR não deve ser mergeada sem revisão e teste.


## 22. LAB: invalidação segura de revelação em RESET via USB (2026-11-06)

- Confirmado GitHub **CI Arduino e Pokémon Engine Checks PASS** no commit `031355a9` (animação Idle real e origem SD/LAB). Isso não substitui validação no aparelho.
- `pokemon_lab_runtime.cpp` agora acompanha um contador de **sessão RAM** protegido pelo mutex existente. Os comandos `PK LAB START` e `PK LAB RESET` incrementam o contador; sorteios normais mantêm o token.
- Nova API `pk_lab_runtime_draw_snapshot_token` obtém resultado e token atomicamente após um DRAW. A animação visual verifica o token a cada frame e antes de revelar. Se a sessão for reiniciada pelo USB durante a abertura, cancela o resultado antigo, pausa timer e atualiza a tela com estatísticas da sessão atual. Não altera recompensa/conta de jogador.
- Casos de hardware a testar: abrir x1 e dar `PK LAB RESET` no USB durante os quadros; abrir x100 e dar `PK LAB START`; alternar entre LAB/Companion após preview SD; executar muitas aberturas de Mimikyu; garantir ausência de travamento, sem alteração de dados Playgroup.
- Essa mudança exige novo CI Arduino e teste físico; **não há hardware remoto disponível**. O merge permanece proibido sem pedido explícito.


## 23. Testes do token de sessão e watchdog de revelação (2026-11-06)

- Confirmado novamente **CI Arduino e Pokémon Engine Checks PASS** no commit `65036b8e`.
- Novo `tests/pokemon_lab_runtime_test.cpp` compila `pokemon_lab_runtime.cpp` com mocks isolados de ESP RNG e mutex FreeRTOS, verificando: token incrementado por START/RESET, token estável por DRAW, captura atômica de resultado, dados congelados após outros draws, rejeição de parâmetros inválidos e reset limpando a RAM sem tocar em perfis reais. O mock valida a lógica sequencial, **não mede concorrência real, FreeRTOS ou estabilidade física**.
- `pokemon_lab_ui.c` recebeu watchdog LVGL de sessão (250 ms) enquanto a tela LAB está visível. Se um `PK LAB START/RESET` vier do monitor USB **após a abertura**, o card e o sprite obsoletos são ocultados, timers interrompidos e estatísticas atualizadas. Durante a abertura, a verificação de token por quadro continua ativa.
- Pendentes novo CI Arduino da mudança, teste real da corrida de USB/touch no ESP32-S3, confirmação visual de uso da memória PSRAM, retorno ao Mimikyu e ausência de stutter. Laboratório ainda não produz Pokémon nem itens reais.


## 24. Validação física do LAB — telemetria somente leitura (2026-11-06)

- O commit `9fbfe1fd` passou integralmente no **CI Arduino** e **Pokémon Engine Checks**. Este marco verifica compilação e testes isolados, **não** touch, pixels ou PSRAM real.
- Novo comando serial `PK LAB DIAG` (mesmo parser do `PK LAB STATUS`): só lê o contador da sessão, status LAB, memória interna livre, PSRAM livre/maior bloco alocável e espécie/frame count do cache TPK2. **Não monta SD, não lê perfis reais, não grava, não apaga nada.**
- **Sequência de teste no aparelho, após build e upload local da versão que contenha DIAG:**
  1. Fora de qualquer jogo Playgroup, anotar `PK LAB DIAG` e `PK LAB STATUS` via serial 115200.
  2. Na tela DIAL entrar em Pokémon → STATS → TEST LAB; selecionar Poké/Great/Ultra/Master, executar uma abertura por tipo e verificar texto `SD PREVIEW` versus `LAB ART`. É esperado fallback para espécie não instalada.
  3. Abrir x100, observar o card, fazer `PK LAB DIAG` + `PK LAB STATUS`; repetir algumas aberturas e comparar PSRAM livre/maior bloco com o valor inicial. Não supor fuga por uma única amostra: caches e montagem podem reservar memória naturalmente.
  4. Com o LAB aberto, executar `PK LAB RESET` via USB após a revelação: o card antigo deve desaparecer e as estatísticas voltar a zero; depois iniciar novo draw e executar RESET **durante** a animação.
  5. Voltar ao Companion, conferir Mimikyu #778 e suas animações; navegar para a tela inicial. Repetir após abrir diversos resultados.
- Enviar logs `[Pokemon LAB] DIAG`, `[Pokemon LAB] STATUS`, possíveis mensagens `[Pokemon] TPK2 Idle` e fotos da tela. **Nenhuma conclusão de estabilidade física antes dessa evidência.**
- Não executar durante partidas Magic/Playgroup ativas, não criar usuário real de teste, não fazer merge da PR. Se houver reset, travamento ou alteração inesperada de dados, interromper testes e manter recompensas reais desligadas.


## 25. Correção UX após validação física do usuário (2026-11-06)

**Feedback do aparelho:** menu do LAB confuso, animação inadequada, Master Ball verde, resultado sem sprite grande, impossibilidade de trocar parceiro.

- O `pokemon_lab_ui.c` foi refeito com **dois estados de interface** na mesma tela de 360×360: menu simples (tipo de bola, abrir, quantidade e voltar) e sobreposição **fullscreen** exclusiva para abertura/revelação. Durante a abertura não há estatísticas nem botão de fechar cobrindo a arte; depois surge o sprite central com nome, #Dex, raridade, indicador Shiny e botão CLOSE.
- O renderer de bolas e emblemas foi corrigido para `LV_COLOR_16_SWAP=1`; Master Ball tem cor superior roxa e detalhe em M. Agora são 12 quadros distintos com tremor, abertura e brilho, em vez de seis quadros quase estáticos. A imagem de bola usa zoom para ocupar mais da tela.
- Na tela Companion há controles táteis anteriores/próximos para selecionar **previews RAM-only** de espécies entre 778, 25, 149 e 150 **apenas se o TPK2 da espécie existir no SD**. O preview selecionado é restaurado ao voltar do TEST LAB. Essa seleção **não altera os seis slots do time nem comprova posse** e precisa ser estendida ao perfil real após autenticação/integração. Sem assets adicionais, o único sprite atualmente confirmado é Mimikyu #778.
- O LAB não inventa sprite de Pokémon quando não há TPK2: mostra aviso explícito `SPRITE NOT INSTALLED | LAB ART` e emblema de teste, sem fake ownership. Sprites normais instalados aparecem grandes e Idle animado; Shiny sem asset validado usa fallback explícito.
- Importante: os 12 frames são **nova implementação provisória**. A qualidade visual precisa de fotos/vídeo após novo upload; a próxima evolução será a arte final e a biblioteca de TPK2 dos Pokémon, e depois a escolha permanente de parceiro atrelada a uma identidade verificada.
- Recompensas, saldo, perfil Playgroup e NVS permanecem sem alteração. PR #1 draft, sem merge.
