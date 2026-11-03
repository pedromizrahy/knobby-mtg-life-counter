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
