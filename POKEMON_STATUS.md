# Pokémon Companions — Estado e próximo passo

Atualizado em 2026-11-02. Fonte de verdade para o andamento do recurso, sem confundir implementação, testes automáticos e validação no aparelho.

## Situação verificada

| Item | Estado | Evidência / ressalva |
| --- | --- | --- |
| Motor básico de perfil, EXP, níveis, equipe (até 6) e Pokédex | IMPLEMENTADO / testes unitários aprovados | `knobby/src/pokemon_progress.c`, `tests/pokemon_progress_test.c`; não equivale a teste de partidas reais |
| Persistência em NVS em dois slots | IMPLEMENTADO / hardware aprovado no cenário de autoteste | `PK SELFTEST` retornou `[Pokemon] SELFTEST PASS` no ESP32-S3, COM4 |
| Identificação de participantes no pós-jogo | IMPLEMENTADO / ainda não testado em partida de teste no aparelho | Exige jogo Playgroup, resultado confirmado, snapshot salvo e identidades únicas válidas |
| Prévia automática de elegibilidade ao confirmar o vencedor | IMPLEMENTADO / ainda sem teste funcional no DIAL | `knobby/src/postgame.c`; logs `[Pokemon] Reward preview` e `[Pokemon] Eligible user` |
| Comando `PK STATUS` | IMPLEMENTADO | Só consulta a sessão corrente: após retorno à tela inicial ela deixa de existir |
| Build Windows da integração | APROVADO no aparelho do desenvolvedor | `BUILD OK`: 11.217.088 / 13.107.200 bytes (85,6%), 1.890.112 bytes livres; o aviso sobre assets sem hashes oficiais continua |
| Upload da integração | CONCLUÍDO | Porta COM4: `UPLOAD CONCLUIDO`, hash verificado |
| CI GitHub do commit `f31fbed` | APROVADO | CI e Pokémon Engine Checks; não substitui teste de fluxo real no ESP32 |
| Recompensas efetivas de jogo | NÃO IMPLEMENTADAS / NÃO ATIVAS | Nenhuma EXP ou Poké Ball é concedida pelo pós-jogo |
| Gacha, shards, tela Pokémon/Tamagotchi e animações | NÃO IMPLEMENTADOS | Design acordado, mas não conectado |

## PRÓXIMA AÇÃO — validar a prévia do pós-jogo no hardware

Nenhum novo build ou upload é necessário para este teste: o usuário já instalou a versão `f31fbed` com `BUILD OK` e `UPLOAD CONCLUIDO`.

1. Abrir o monitor serial em 115200 baud (PowerShell):
   `& "C:\Program Files\Arduino CLI\arduino-cli.exe" monitor -p COM4 -c baudrate=115200`
2. Iniciar uma **partida de teste** no DIAL usando jogadores associados a IDs válidos e distintos do Playgroup.gg. Não é necessário fazer uma partida completa; apenas percorrer o fluxo de fim de jogo.
3. Confirmar o vencedor no pós-jogo. Observar os logs **na confirmação**, antes de o DIAL voltar à tela inicial.
4. Esperado para sessão confirmada e mapeada: `[Pokemon] Reward preview: ... (NOT APPLIED)` e uma linha `[Pokemon] Eligible user=... seat=... win=... (NOT APPLIED)` por participante. Conferir os IDs e qual linha tem `win=1`.
5. Para jogadores offline ainda não mapeados, é esperado `Reward preview skipped` e **nenhuma** recompensa; não considerar esse caso como falha do armazenamento.

**Não executar `PK STATUS` após voltar à tela inicial para diagnosticar um jogo recém-encerrado:** o contexto da sessão é zerado; use os logs automáticos na confirmação.

Se o usuário não puder realizar a partida de teste, avançar somente com mudanças independentes da execução do hardware e marcar a validação como pendente. Nunca afirmar que o pós-jogo está validado sem logs correspondentes.

## Progresso adicional — registro durável de recompensas (2026-11-02)

- Implementados `knobby/src/pokemon_reward_ledger.h` e `.cpp`: journal **append-only** por Playgroup User ID em SPIFFS, com registros de 16 bytes, checksum e estados `PREPARED` / `COMMITTED`. Não usa a partição NVS.
- A leitura valida **todo o journal**, falhando de modo conservador se houver um registro incompleto, corrompido ou uma transição inválida. O método não formata SPIFFS.
- **Ainda não integrado ao pós-jogo e ainda não testado no hardware.** Recompensas continuam DESATIVADAS. Criar um journal sozinho NÃO torna segura a premiação; faltam a lógica transacional e a recuperação após queda de energia.
- Protocolo exigido antes de ativar: (1) verificar resultado confirmado durável e identidades válidas; (2) reconciliar qualquer `PREPARED` pendente do jogador; (3) escrever `PREPARED`; (4) atualizar perfil e conferir salvamento; (5) escrever `COMMITTED`; (6) em reinicialização, usar o ID ainda no histórico recente do perfil para determinar se o `PREPARED` já foi aplicado; **não** processar mais 32 jogos enquanto um `PREPARED` estiver pendente. Testar interrupções em cada fase antes de ativar.
- Necessário revisar o crescimento indefinido do journal e o espaço SPIFFS compartilhado com a fila Playgroup e cache de artes. Não habilitar prêmios até existir política de limite/recuperação, com falha segura para armazenamento cheio.
- O teste da prévia de elegibilidade no pós-jogo continua pendente; não confundir o novo journal com validação dessa integração.

## Progresso adicional — coordenador e recuperação segura (2026-11-02)

- Implementado `pokemon_reward_coordinator.c/.h`: transação isolada que exige histórico durável, perfil existente e todas as operações de armazenamento injetadas por callbacks. **Não está conectado ao pós-jogo.**
- Fluxo previsto: consultar journal, procurar pendências do usuário, carregar perfil, preparar sessão, gravar perfil, confirmar sessão. Repetição com sessão COMMITTED não dá prêmio; falha após salvar o perfil e antes de COMMITTED permite finalizar o histórico **sem aplicar EXP novamente**.
- Caso o journal esteja PREPARED e o perfil **não** contenha a sessão no histórico recente, a situação é ambígua e o coordenador **bloqueia**, sem presumir que pode premiar novamente. Recuperação automática desse caso continua pendente de desenho mais forte (snapshot/preimage).
- Adicionado `pk_ledger_find_pending`: detecta transações pendentes de um jogador no SPIFFS; nenhuma partida nova deve premiar esse jogador antes de resolver a pendência.
- Criado `tests/pokemon_reward_coordinator_test.c`, com simulações de salvamento interrompido, falha no preparo, falha na confirmação, repetição e journal corrompido; incluído no workflow `pokemon-engine.yml`.
- **Ainda falta** conectar o coordenador ao armazenamento real via adaptador, estabelecer inicialização explícita do primeiro Pokémon, testar múltiplos perfis no hardware e resolver capacidade NVS, política de ocupação SPIFFS e recuperação de estado ambíguo. Os resultados dos testes simulados não validam queda real de energia nem atomicidade da NVS.

## Progresso adicional — adaptador NVS/SPIFFS sem ativação (2026-11-02)

- Implementado `pokemon_reward_storage.h/.cpp`, que conecta callbacks do coordenador à gravação de perfis NVS e ao histórico SPIFFS. A função `pk_reward_apply_existing` **não tem chamadas no pós-jogo**, portanto ainda não concede EXP a ninguém.
- `pk_reward_apply_existing` só funciona com perfil já existente e exige que o chamador valide previamente resultado confirmado e gravado e identidades vinculadas. Não inicializa Pokémon, não altera configuração Magic e não sincroniza nada.
- Adicionado teste `tests/pokemon_reward_storage_test.cpp` e execução no workflow `pokemon-engine.yml`, verificando a ligação com armazenamentos simulados, repetição, falha na confirmação e recuperação.
- **Limite de segurança:** teste de adaptador com mocks não garante durabilidade física, capacidade da NVS, recuperação de NVS parcialmente gravada ou reinicialização da placa. Ainda é preciso implementar inicialização segura e testar no ESP32-S3 antes de ativar recompensas.
- A prévia do pós-jogo já está instalada no DIAL, mas falta o teste de identificação com partida Playgroup de exemplo.

## Progresso adicional — criação segura de perfil inicial (2026-11-02)

- `pk_store_inspect` diferencia explicitamente perfil `MISSING`, `VALID`, `CORRUPT` e `ERROR`; uma falha ao ler os dois slots NVS não se confunde mais com perfil ainda não criado.
- `pk_store_create_starter` exige os dois slots vazios e cria explicitamente o primeiro Pokémon, Mimikyu #778, por User ID do Playgroup.gg. **Ainda não é chamado automaticamente pelo pós-jogo.**
- `pk_store_save` agora atualiza exclusivamente perfis válidos: não cria perfis implicitamente e **recusa sobrescrever dois slots corrompidos**.
- `PK SELFTEST` continua usando apenas o ID de diagnóstico reservado e mantém o cenário de salvar/ler/atualizar/limpar.
- Adicionados mock de `Preferences` e testes de slots corruptos, criação duplicada, falha de escrita e diagnóstico, no workflow `pokemon-engine.yml`.
- Ainda não validado no ESP32-S3 após esta alteração; o autoteste anterior validava a versão antiga. Testes com mock não medem desgaste/capacidade do NVS de 20 KiB nem prova de recuperação em queda real de energia.
- **Não implementar criação automática de vários perfis** antes de avaliar a ocupação NVS, migração para SPIFFS ou política de limites sem apagar configurações já existentes.

## Progresso adicional — armazenamento de múltiplos perfis (2026-11-02)

- Auditoria: NVS tem apenas **0x5000 = 20 KiB** e é compartilhada com o Magic, Wi-Fi e Playgroup. A rotina de inicialização existente em `knobby/src/storage.c` contém `nvs_flash_erase()` para certos erros de inicialização: **não é seguro multiplicar grandes perfis Pokémon nessa partição**. Não alteramos essa rotina legada nesta etapa.
- Implementado backend alternativo **isolado** `knobby/src/pokemon_file_store.h/.cpp` para salvar perfis em **SPIFFS** (`0x360000 = 3.375 MiB`), com dois arquivos por usuário, número de geração, hash de integridade e leitura da última cópia válida. Usa `SPIFFS.begin(false)` e nunca formata o armazenamento nem exclui perfis. Não tem chamadas de jogo e não substitui ainda o backend NVS.
- Implementado teste host `tests/pokemon_file_store_test.cpp` com SPIFFS simulado: perfil novo, atualização, cópia danificada, ambas cópias danificadas, escrita interrompida e falha de montagem. Workflow atualizado para executar esses testes.
- **Migração de perfis NVS para SPIFFS ainda NÃO implementada**, e o adaptador de recompensas `pokemon_reward_storage.cpp` continua apontando para NVS. Não criar automaticamente perfis nessa versão.
- **Riscos ainda pendentes:** SPIFFS compartilha espaço com fila de partidas Playgroup e cache de artes; é preciso reservar espaço, controlar falha de escrita, definir política de migração verificável com rollback e testar a recuperação no ESP32-S3.
- **NÃO usar upload nem considerar pronto para produção** enquanto os testes do último commit e o build Windows/hardware desse novo backend não forem validados.

## Progresso adicional — migração explícita NVS → SPIFFS (2026-11-02)

- Implementados `pk_file_import_if_missing` e `pk_profile_migrate_to_spiffs`: migração **exclusivamente manual/opt-in**, não chamada pelo boot, pós-jogo ou sincronização.
- A origem NVS é somente lida e **nunca apagada**. Para perfil NVS válido e SPIFFS vazio, grava arquivo temporário `/pkm_<user_id>.tmp`, verifica checksum e conteúdo, renomeia para slot A e confere byte a byte o perfil de destino. Não existe exclusão automática de perfis.
- Perfis já iguais em NVS/SPIFFS retornam `PK_MIGRATE_ALREADY`; dados divergentes ou corrompidos retornam **conflito sem sobrescrita**; origem NVS ausente com arquivo válido retorna `PK_MIGRATE_FILE_ONLY`.
- Falha/interrupção que deixa arquivo `.tmp` bloqueia nova tentativa (fail closed), preservando os dados para recuperação manual. O programa ainda **não** possui comando de resolução de staging nem procedimento de reparo em hardware.
- Criados testes `pokemon_profile_migration_test.cpp` para repetição, origem intacta, divergência, corrupção, falha de escrita e falta de montagem; integrados ao workflow Pokémon.
- **Não ativar migração no aparelho nem trocar o backend de premiação** até conferir o CI final, compilar em Windows, validar espaço SPIFFS compartilhado, teste de hardware e procedimento explícito de recuperação. O adaptador `pk_reward_apply_existing` ainda aponta para NVS e está desligado do Magic.

## Progresso adicional — diagnóstico de armazenamento no hardware (2026-11-02)

- Implementado comando serial **`PK STORAGE`** no parser Playgroup existente; é apenas leitura. Usa `SPIFFS.begin(false)` (nunca formata), imprime capacidade total/usada/livre e conta arquivos `pkf_*` (perfis), `pkl_*` (histórico) e `pkm_*` (migrações interrompidas).
- Bloqueia sua execução durante uma partida Playgroup ativa ainda sem resultado confirmado. Não cria nenhum perfil, não executa migração e não concede recompensas.
- **Próximo teste em hardware:** após aprovar CI e Windows `BUILD OK`, fazer upload da versão e enviar `PK STORAGE` pelo monitor serial em 115200 baud na COM4. Esperado `STORAGE OK`, `STORAGE FILES ...` e `STORAGE READ-ONLY`. Usar total/used/free medidos para definir margem da fila de partidas e cache de arte antes de habilitar qualquer gravação Pokémon.
- A migração NVS → SPIFFS continua **manual/desligada**, sem comando serial de migração. O teste anterior `PK SELFTEST PASS` cobriu apenas a NVS, não esta implementação SPIFFS.
- O CI principal não substitui o build Windows nem o teste de SPIFFS no dispositivo.

## Progresso adicional — autoteste manual SPIFFS (2026-11-02)

- **Hardware previamente aprovado:** `PK STORAGE` na COM4 retornou `total=3245681 used=1831045 free=1414636 bytes`, com `profiles=0 ledgers=0 staging=0`. A consulta foi apenas de leitura.
- Implementado `pk_spiffs_selftest` e o comando **`PK SPIFFS SELFTEST`**. Executa somente quando solicitado via monitor serial e fora de partida Playgroup ativa.
- O teste reserva exclusivamente `/pkt_spiffs_01.bin`, recusa sobrescrever um arquivo já existente, exige margem inicial de 256 KiB, grava, relê, substitui e relê novamente 12 bytes, depois remove só o arquivo criado pelo teste. Não formata, não percorre perfis reais, não altera a fila ou o cache. Se a remoção falhar, retorna FAIL e o próximo teste se recusa a sobrescrever o arquivo remanescente.
- Adicionado mock de SPIFFS e teste automatizado que verifica proteção de arquivos alheios, recusa de sobrescrita, erro de gravação, erro de remoção, falta de capacidade e montagem indisponível.
- **Ainda não foi testado fisicamente:** concluir GitHub CI, compilar Windows e fazer upload antes de executar `PK SPIFFS SELFTEST` no monitor a 115200 baud. Se `FAIL`, não repetir indiscriminadamente nem apagar arquivos manualmente.
- A migração de perfis reais e a premiação continuam desativadas.

## Progresso adicional — autoteste de perfil completo no SPIFFS (2026-11-03)

- **Hardware aprovado anteriormente:** `PK SPIFFS SELFTEST PASS` retornado pelo usuário no DIAL real. O teste verificou escrita/leitura/atualização/limpeza de um arquivo temporário, **não** a estrutura completa de perfil.
- Implementado comando explícito `PK PROFILE SELFTEST` e rotina `pk_file_profile_selftest`, **somente fora de partida Playgroup ativa**. Não altera perfis reais, NVS ou o journal de recompensas; não distribui recompensas no jogo.
- O teste usa exclusivamente ID diagnóstico reservado `0xFFFFFFFD` (bloqueado nas APIs públicas de perfis SPIFFS), com os arquivos `/pkf_fffffffda.bin` e `/pkf_fffffffdb.bin`. Recusa executar caso qualquer dos dois já exista, inclusive se sobrou de interrupção anterior.
- Valida criação de Mimikyu #778, leitura do slot A, cálculo local de 75 XP + 1 Poké Ball de vitória, escrita do slot B com geração 2, leitura de B como última versão, corrupção deliberada **somente do próprio slot B de diagnóstico**, fallback para A, restauração do slot B e verificação final. Limpa somente arquivos criados pela própria execução; caso não consiga remover, retorna FAIL e uma nova tentativa é bloqueada até diagnóstico manual.
- Exige no início ao menos **256 KiB de espaço livre** no SPIFFS, não formata o armazenamento. Mock de SPIFFS testa repetição, dados de terceiros intactos, arquivo preexistente, falha de escrita e falha de limpeza.
- **Pendente teste físico:** aguardar CI, compilar Windows, fazer upload e executar `PK PROFILE SELFTEST` no monitor serial COM4/115200. Não afirmar PASS em hardware até receber resultado. Sem mudança de gameplay ou ativação de recompensas.

## Validação em hardware — perfil completo SPIFFS (2026-11-03)

- **Confirmado no DIAL ESP32-S3, via monitor serial COM4:** `PK PROFILE SELFTEST` produziu `[Pokemon] PROFILE SELFTEST PASS`. O teste exercita criação de Mimikyu #778 no ID diagnóstico, premiação simulada local (+75 XP e +1 Poké Ball), duas gerações dos registros SPIFFS, recuperação diante de corrupção do slot B de diagnóstico e limpeza de ambos os slots.
- **Confirmado no GitHub:** no commit `913690fd7566969310b5f2c5ef3e506221d5ae61`, os workflows `Pokemon engine checks` e `CI` concluíram com sucesso. O upload e teste físico foram relatados pelo usuário.
- O log também continha `[Diag] Reset reason: UNKNOWN (11)` antes do comando; origem não diagnosticada. Sem evidência neste log de falha do teste nem motivo para afirmar causa.
- Ainda **não testados no hardware**: perfis reais multiusuário, persistência após reinício completo, migração NVS→SPIFFS, journal de recompensas/recuperação de transação e vinculação pós-jogo. O adaptador de premiação continua sem ativação.
- Próximo passo: verificar persistência após reinício e limites de armazenamento sem remover, alterar ou migrar dados reais; depois consolidar procedimento seguro para perfis reais.

## Progresso adicional — teste de persistência após reinício (2026-11-03)

- O teste anterior `PK PROFILE SELFTEST PASS` foi confirmado **no DIAL real**, mas verificava operações na mesma sessão; não prova sobrevivência a um desligamento/reinicialização.
- Implementadas duas ações seriais **manuais**: `PK REBOOT PREP` e `PK REBOOT VERIFY`. PREP cria dois registros SPIFFS válidos do perfil de Mimikyu #778 em ID reservado **0xFFFFFFFC**, com slot A inicial e slot B contendo uma vitória simulada (+75 XP e +1 Poké Ball). VERIFY, executado **depois de reiniciar o DIAL manualmente**, relê ambos, confere perfil e gerações, verifica o slot mais recente e remove apenas esses dois arquivos após validação integral.
- Ambos os comandos são recusados durante jogo Playgroup ativo, não tocam em dados NVS, jogadores reais, fila, cache ou journal e nunca formatam o SPIFFS. O comando PREP recusa dados diagnósticos preexistentes; VERIFY com dados ilegíveis falha sem apagar nada.
- O reboot não é executado pelo firmware; é preciso fechar o monitor serial, reiniciar a placa e reabrir o monitor antes do VERIFY. O programa **não consegue atestar sozinho** que houve reinício entre os dois comandos.
- Testes simulados integrados ao teste de armazenamento cobrem gravação entre chamadas, arquivos preexistentes, corrupção e escrita interrompida. **Ainda pendente validação no DIAL físico** e aprovação do último CI.
- Em caso de FAIL, não repetir PREP, não formatar nem excluir manualmente arquivos de diagnóstico: investigar primeiro. Recompensas reais e migração automática continuam desativadas.

## Validação em hardware — persistência após reinicialização (2026-11-03)

- **Confirmado no DIAL real:** `PK REBOOT PREP` retornou `[Pokemon] REBOOT PREP PASS: restart the dial, then run PK REBOOT VERIFY.`.
- Após reinício físico do DIAL, **confirmado no monitor serial:** `PK REBOOT VERIFY` retornou `[Pokemon] REBOOT VERIFY PASS: both persisted slots matched; diagnostic files cleaned.`.
- A verificação releu os **dois registros** SPIFFS reservados do perfil Mimikyu #778, com gerações e dados esperados (slot atualizado com vitória simulada, +75 XP e +1 Poké Ball), e concluiu a limpeza dos arquivos de diagnóstico. A confirmação de reinício físico é reportada pelo operador; o próprio comando não atesta o power cycle.
- Os testes físicos de capacidade, arquivo temporário, perfil completo e persistência pós-reinício estão concluídos. Estes resultados **não validam ainda** migração de jogador real, premiação pós-jogo, histórico antirrepetição ao longo da vida, recuperação transacional em queda de energia durante escrita, nem desempenho/limites multiusuário.
- Próxima etapa: validação opt-in do journal e da coordenação de recompensas com identidade reservada, sem ligação automática ao Magic, e proteção contra duplicação após interrupção.

## Interface inicial do Pokémon — acesso direto pelo home (2026-11-03)

- **Pedido confirmado:** Pokémon é um modo separado, **fora da partida** e acessível **diretamente da tela inicial** junto a `LOCAL PLAY` e `MULTIPLAYER`, por botão compacto de **Poké Bola pixelada** (desenhada em LVGL, sem assets de terceiros). Não passa por Extras, não substitui Dice, Event Log, Mana Pool, Table Sync, Wi-Fi nem Settings.
- Implementado `knobby/src/pokemon_ui.c/.h`, com telas LVGL em 360×360. Tela inicial Pokémon apresenta prévia visual de Mimikyu #778 usando formas LVGL próprias (placeholder, **ainda não é sprite oficial**), e três páginas de demonstração: companion/nível/XP, Poké Balls e Pokédex. Usa toque em setas e rotação do dial para páginas; botão BACK e gesto de retorno levam ao home.
- **Dados de demonstração explicitamente identificados como DEMO**, não associados a contas reais. Não cria perfil no SPIFFS/NVS, não grava XP, não concede Poké Balls e não ativa captura ou premiação. Seleção de identidade Playgroup e alimentação/cuidado/evolução continuam pendentes.
- O acesso ao Pokémon é exposto somente no home do pré-jogo e não altera os componentes de gameplay Magic. A UI **ainda precisa de compilação Arduino CI aprovada** e teste no aparelho; testes isolados de Pokémon Engine não validam navegação/display LVGL.

## Auditoria do hardware e diagnóstico microSD (2026-11-03)

- Unidade real: GUITION Knobby vermelho/preto, com **cartão microSD 512 MB fisicamente identificado pelo usuário**; a revisão exata K518 ou K718 **ainda depende do resultado do probe**, pois `board_detect()` usa fallback K518 se a sondagem I2C falhar.
- Fontes hardware: `Guition-JC/Guition`, arquivos `components/board/guition_boards/jc3636k518/board_devices.yaml` e `jc3636k718/board_devices.yaml`. No K518: SDMMC CLK=4 CMD=3 D0=5 D1=6 D2=42 D3=2. No K718: CLK=39 CMD=38 D0=40 D1=41 D2=48 D3=47. Diferem também display/touch/dial, já suportados no firmware.
- K518 de referência: ESP32-S3R8 240 MHz, flash 16 MB, PSRAM 8 MB, tela circular 360x360 ST77916 (QSPI), touch CST816S (I2C), dial, segundo ESP32 para comunicação USB. O USB-C pode conectar chips diferentes quando invertido; não utilizá-lo para USB Mass Storage sem projeto específico.
- Firmware atual: `knobby/partitions.csv` reserva app 12,5 MiB, SPIFFS 3,375 MiB e NVS 20 KiB. Não modificar tabela de partições para experimentar SD; preservar `BUILD.cmd`/`UPLOAD.cmd` e identidade USB/COM4.
- Integrado **somente como diagnóstico manual**, não na inicialização nem na renderização: `PK SD STATUS`. Executa `SD_MMC.setPins()` na revisão detectada, monta microSD **1-bit** com `format_if_mount_failed=false`, imprime tipo/capacidade/ocupação e verifica existência de `/mons` e `/mons/p778.bin`, desmontando após leitura. Não formata, cria, apaga, escreve nem copia arquivos; bloqueado com Playgroup ativo. **Ainda precisa de aprovação CI e teste físico** para confirmar acesso efetivo do ESP32-S3 ao cartão.
- TamaPoke original (socquique/TamaPoke): formato `TPK2` nos arquivos `/mons/pNNN.bin`, sprite multi-ação indexado em PSRAM. Fonte e firmware sob MIT, mas sprites PMD SpriteCollab CC BY-NC 4.0; direitos Pokémon de terceiros. Não importar sprites/arte ao repositório sem tratar licenças. A atual interface DEMO não foi substituída; resta portar o carregador/renderizador e paisagens após validar SD, capacidade de PSRAM e desempenho.
- O cartão 512 MB é suficiente para iniciar a validação, mas não prometer conter **todas as 1025 espécies e variantes** sem medir tamanhos reais. Não solicitar nova desmontagem; cartão permanece dentro do equipamento.

## Hardware real: microSD montado e leitura concluída (2026-11-03)

- **Confirmado no ESP32-S3 real, `PK SD STATUS`:** `board=JC3636K518 CLK=4 CMD=3 D0=5 (1-bit)`.
- **SD montado em modo leitura, sem formatar:** `type=SDSC capacity=503316480 total=499105792 used=432902144 bytes`.
- **Arquivos TamaPoke não encontrados:** `mons_dir=NO mimikyu_tpk2=NO`. O comando encerrou com `unmounted; no files modified`.
- O cartão apresenta ~413 MiB utilizados e ~63 MiB disponíveis (66.203.648 bytes), cerca de 86,7% da área utilizável ocupada. Origem dos dados desconhecida.
- **Não formatar, sobrescrever, apagar nem copiar assets para o cartão antes de identificar os arquivos existentes e realizar backup.**
- O DIAL voltou a ligar após falha temporária na remontagem, e o firmware executou o teste SD; causa da falha anterior não está diagnosticada.
- Próximo passo: inventário limitado e estritamente de leitura dos diretórios/arquivos para identificar o uso do espaço e planejar sprites e paisagens.

## Bloqueadores para recompensas reais

- A proteção antirrepetição atual do motor é um buffer rotativo com **32 IDs recentes**, insuficiente para idempotência vitalícia por si só. Um journal persistente foi acrescentado, mas **a coordenação transacional e a recuperação ainda precisam ser implementadas e testadas** antes de ligar recompensas.
- A partição NVS tem **20 KiB** e os perfis usam registros duplicados. Avaliar capacidade e estratégia de armazenamento antes de adicionar múltiplos jogadores; evitar qualquer operação que possa levar à limpeza total do NVS.
- A identidade offline pode ser resolvida posteriormente; a premiação precisa respeitar o vínculo final com cada Playgroup user ID.
- A premiação deve ocorrer **depois da confirmação e gravação durável** do resultado, com recuperação após reinicialização, sem duplicação e sem impacto no resultado Magic.
- Não criar perfil Pokémon de jogadores com ID inválido, não mapear por assento e não conceder recompensas durante a partida.
- Rever como inicializar o primeiro Pokémon e quando criar um perfil antes de aplicar EXP (hoje `pk_award_game` exige ao menos um Pokémon no time).

## Regras do projeto

- Branch de trabalho: `feat/game-events-turn-manager`; **não fazer merge da PR**.
- Não modificar o título/intro `DIAL DOS PRIMOS`, animações de dados, nem o fluxo Magic fora do escopo.
- Primeiro testes automatizados, depois build Windows, depois upload/teste físico quando necessário.
- Registrar como `implementado`, `testado no CI` e `testado no DIAL` apenas quando cada evidência existir.
