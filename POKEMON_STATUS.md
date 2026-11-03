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

## Bloqueadores para recompensas reais

- A proteção antirrepetição atual do motor é um buffer rotativo com **32 IDs recentes**, insuficiente para garantir idempotência vitalícia após 32 partidas. Projetar um registro durável de partidas premiadas antes de ligar recompensas.
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
