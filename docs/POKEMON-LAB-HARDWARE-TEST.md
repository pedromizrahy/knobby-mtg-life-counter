# DIAL DOS PRIMOS — TEST LAB: validação física controlada

**Status:** código anterior compilado no CI; confirmar o SHA atual em GitHub Actions antes do upload; testes reais de touch, SD e PSRAM **ainda pendentes**. Não é requisito conectar Playgroup. Não fazer merge. Não executar durante jogo Magic.

## Pré-requisitos
- ESP32-S3 compatível conectado por USB à COM4 (confirmar porta no Windows);
- cartão SD com `/mons/p778.bin` se quiser testar Mimikyu real; demais espécies não exigidas;
- computador Windows, repositório local `C:\dev\sistema-mz\knobby-mtg-life-counter`, Arduino CLI instalado;
- fechar o monitor serial antes do upload, pois a COM4 só pode ter um programa usando a porta;
- **não** formatar SPIFFS/SD, não executar `PG CLEAR`, `PG RESET` ou apagamentos de flash.

## Atualizar, compilar e gravar — operação do usuário
Abrir PowerShell no repositório e **confirmar primeiro** que não há alterações locais não commitadas que possam se perder:

```powershell
cd C:\dev\sistema-mz\knobby-mtg-life-counter
# Abort if the local checkout has any unstaged, staged or untracked changes.
if ((git status --porcelain).Length -gt 0) {
    throw "Working tree is not clean. Review local files before switching branches."
}
git fetch origin feat/game-events-turn-manager
if ($LASTEXITCODE -ne 0) { throw "git fetch failed" }
git switch feat/game-events-turn-manager
if ($LASTEXITCODE -ne 0) { throw "git switch failed" }
git merge --ff-only origin/feat/game-events-turn-manager
if ($LASTEXITCODE -ne 0) { throw "Fast-forward failed. Do not force-update branch." }
git rev-parse --short HEAD
```

O script interrompe em alterações locais, falhas de Git ou divergências de histórico. O `git merge --ff-only` atualiza **apenas a branch local** com o commit remoto da mesma branch; **não realiza merge da PR no GitHub**.

```powershell
$cli = (Get-Command arduino-cli -ErrorAction Stop).Source
$fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=custom"
& $cli compile --fqbn $fqbn .\knobby
if ($LASTEXITCODE -ne 0) { throw "Compilation failed: do not upload" }
```

Inspecionar o resumo: o binário precisa caber na partição app 12,5 MiB, e a compilação precisa terminar com status 0. O upload por USB é **uma ação separada e intencional** do usuário:

```powershell
& $cli compile --upload -p COM4 --fqbn $fqbn .\knobby
```

O segundo comando recompila e envia ao dispositivo. **Evitar opções de erase-all/format**. Confirmar que a placa, porta e partição estão corretas antes de gravar.

## Diagnósticos — monitor 115200
Depois do upload e boot:

```powershell
& $cli monitor -p COM4 -c baudrate=115200
```

Enviar `PK LAB DIAG` e `PK LAB STATUS` fora de qualquer jogo. Registrar as linhas do terminal. `PK LAB DIAG` não escreve em flash nem monta SD: lê apenas contadores da sessão e metadados de RAM/cache.

## Roteiro touch + SD
1. Abrir **Pokémon → STATS → TEST LAB**. Confirmar que níveis/contadores continuam simulados e testar RATES antes e depois de lotes de 100 aberturas.
2. Abrir POKE, GREAT, ULTRA e MASTER, primeiro `x1`, depois `x100`. Conferir que o contador sobe somente pelo número de draws pedido e que Master revela tier LENDÁRIO.
3. Verificar rótulo `SD PREVIEW` somente quando a espécie normal sorteada possui Idle TPK2 real; `LAB ART` nos casos não instalados ou Shiny. Quando vier Mimikyu #778 e houver TPK2 instalado, inspecionar a animação e o recorte do card.
4. Executar `PK LAB DIAG` antes/depois de 20 aberturas; comparar `psram_free` e `psram_largest` sem assumir vazamento por uma amostra única. Não extrapolar para stress de horas.
5. Com um card revelado, enviar `PK LAB RESET`: resultado e taxas antigas devem sumir. Repetir enviando RESET durante os 6 quadros de abertura e confirmando que não aparece uma revelação de sessão antiga.
6. Voltar ao Companion: Mimikyu #778 deve aparecer normalmente. Fazer a alternância LAB → Companion várias vezes. Confirmar que Magic funciona como antes.
7. Se houver travamento, boot loop, falhas de SD ou perda progressiva marcante de PSRAM, interromper e conservar o log serial completo. **Não apagar perfis/SD para diagnosticar.**

## TEST LAB ampliado — 251 Pokémon (simulação)

O LAB agora usa as espécies #001–#251, com **nomes da National Dex** e sprites Idle TPK2 que já estiverem no cartão SD. As probabilidades gerais de cada bola não mudaram: POKE 75/20/4,5/0,5%; GREAT 55/30/13/2%; ULTRA 30/40/24/6%; MASTER 0/0/0/100% (comum/raro/épico/lendário).

A tela `RATES` mostra duas colunas de percentuais: **TARGET** (probabilidade configurada) e **ACTUAL** (frequência observada nos sorteios desde o último START/RESET de sessão). Mostra também o total de sorteios e quantos foram marcados Shiny no simulador, cuja probabilidade-alvo é 1/4096. O resultado pode divergir bastante nos primeiros lotes; os percentuais referem-se às **raridades**, não à chance individual de cada espécie.

As 251 espécies estão classificadas **provisoriamente** para testar distribuição: 114 comuns, 103 raras, 23 épicas e 11 lendárias/míticas. Não são probabilidades oficiais de captura. Dentro de cada categoria, os Pokémon são escolhidos uniformemente; por isso a chance individual de uma espécie depende do número de espécies daquele grupo.

**Shiny**: o LAB pode sortear o marcador Shiny mas os pacotes instalados são versões normais. A tela revelará o sprite normal, identificado como `SHINY ROLL - NORMAL SD ART`; não afirma exibir um sprite Shiny real.

Nenhuma dessas probabilidades ou sprites concede Pokémon, gasta Pokébolas ou altera as contas reais do Playgroup. A seleção de parceiro continua sendo uma prévia em RAM, não uma mudança persistente de equipe.

## O que registrar
- SHA instalado (`git rev-parse --short HEAD`);
- as saídas `PK LAB DIAG` antes e depois e `PK LAB STATUS`;
- foto/vídeo do card e do sprite, especialmente quando mostrar `SD PREVIEW`;
- quaisquer linhas `[Pokemon] TPK2 Idle`, mensagens de erro/stack trace e a ação feita imediatamente antes;
- resultado visual de cada um dos sete pontos.

**Limite de validação:** CI e host mocks não demonstram estabilidade real da tela, PSRAM, SD ou touch. LAB continua RAM-only, sem ID Playgroup, recompensas ou inventário real.
