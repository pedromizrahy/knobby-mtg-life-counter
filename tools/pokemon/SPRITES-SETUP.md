# Pokémon sprites para o DIAL — preparação local (sem incluir artes no firmware)

O firmware usa **TPK2** no cartão SD em `/mons/pNNN.bin`. O único arquivo comprovadamente instalado e testado até agora é `p778.bin` (Mimikyu). O código do TEST LAB não inclui sprites reais dos outros Pokémon; é necessário preparar e enviar os arquivos para o SD. **Não dê upload do firmware esperando que isso instale os sprites.**

## Fonte e direitos

`prepare_sprites.py` baixa imagens do repositório público [PokeAPI/sprites](https://github.com/PokeAPI/sprites), preferindo GIFs animados da comunidade Pokémon Showdown e utilizando PNG simples como fallback. **Disponibilidade pública não significa licença livre para redistribuição.** Respeite direitos de Pokémon, Smogon e artistas; este script destina-se a uso privado/local, não a publicar um pacote de sprites. A ferramenta registra URLs em `sprite_sources.json`. Nenhuma imagem ou TPK2 é adicionada automaticamente ao Git.

## Preparação no Windows

No PowerShell, com o repositório atualizado na branch `feat/game-events-turn-manager`:

```powershell
cd C:\dev\sistema-mz\knobby-mtg-life-counter
py -3 -m pip install Pillow pyserial
# Três primeiros Pokémon para validar a revelação fullscreen
py -3 tools/pokemon/prepare_sprites.py --out-dir assets/pokemon --species 25 149 150
```

Os arquivos resultantes serão `assets/pokemon/p25.bin`, `p149.bin` e `p150.bin`. Eles contêm Idle de até seis frames 48×48, alinhados em TPK2 compatível com o limite de PSRAM do renderer. Os frames são selecionados do GIF de origem, não produzidos artificialmente; PNG oferece um único frame parado.

Para gerar os **13 candidatos restantes além do Mimikyu**:

```powershell
py -3 tools/pokemon/prepare_sprites.py --out-dir assets/pokemon
```

O conversor não substitui `p778.bin` nem arquivos locais existentes por padrão.

## Auditoria e envio ao cartão

O comando abaixo **não altera o DIAL**: faz dry run e valida arquivos TPK2.

```powershell
py -3 tools/pokemon/upload_batch.py --mons-dir assets/pokemon --species 25 149 150
```

Depois de conferir os arquivos, fechar qualquer monitor serial usando COM4 e **escolher executar o envio**:

```powershell
py -3 tools/pokemon/upload_batch.py --mons-dir assets/pokemon --species 25 149 150 --port COM4 --execute
```

O upload usa o protocolo existente `PK PMD PUT`, CRC32 e verificação `PK PMD CHECK`. Confirme primeiro se o firmware atualizado está instalado. Não sobrescrever o Mimikyu. Para enviar todos os 13 candidatos, remova `--species 25 149 150` depois de preparar todo o diretório.

Se algum download não estiver disponível, o script imprime `FAILED`; nesse caso **não faça upload parcial presumindo que todos os 13 estão prontos**. Confira os IDs. Não apagar/formatar o SD para corrigir faltas.

## Como usar os sprites

- **TEST LAB**: selecionar Pokébola, abrir na tela exclusiva, revelar nome + número Dex e exibir o TPK2 real se instalado. O LAB é simulado e não concede posse.
- **Companion**: os controles anteriores/próximos percorrem os 14 candidatos conhecidos. Só é possível mostrar a arte das espécies com TPK2 instalado; a escolha atual é uma prévia na RAM, **não seleção persistente de um parceiro de uma conta Playgroup**.
- Um resultado Shiny continua sem sprite próprio até existir um fluxo dedicado de TPK2 Shiny validado. Não confundir o sprite normal com a versão Shiny.
- A aparência final da animação de Pokébola ainda exige teste físico; preparar sprites não resolve sozinho o design visual.

## Expansão opcional: Kanto + Johto — 251 Pokémon

Para preparar **toda a 1ª geração (Dex #1–151) e a 2ª geração (Dex #152–251)**, sem copiar os binários para o firmware:

```powershell
py -3 tools/pokemon/prepare_sprites.py --gen1-2 --out-dir assets/pokemon
```

O programa baixa os GIFs quando disponíveis, usa PNG parado como fallback, gera TPK2 localmente e resume o total de bytes e IDs com erro. O download **não instala automaticamente nenhum arquivo no DIAL**; sprites privados/de terceiros não devem ser incluídos no Git. O conversor não apaga os arquivos existentes por padrão e nunca substitui `p778.bin`.

Os 251 arquivos têm no máximo `251 × (7 + 480 + 4 + 12 + 6 × 48 × 48) = 3.596.077 bytes` (aprox. **3,43 MiB**). Esse cálculo é de **armazenamento SD**, não de RAM. Com 6 frames na dimensão máxima de 48×48, apenas o sprite carregado pode reservar **373.248 bytes (~365 KiB) de PSRAM**, dentro do limite de 512 KiB do cache Idle. Se a imagem original tiver só um frame, ocupa menos.

Auditar o lote **antes** de transferir:

```powershell
py -3 tools/pokemon/upload_batch.py --gen1-2 --mons-dir assets/pokemon
```

Caso algum ID falte, o dry run para com erro em vez de afirmar que a geração inteira está instalada. Para testar poucos IDs inicialmente:

```powershell
py -3 tools/pokemon/upload_batch.py --gen1-2 --mons-dir assets/pokemon --species 1 25 151 152 251
```

Após validar cada arquivo, fechar o monitor serial e executar deliberadamente a transferência do lote escolhido:

```powershell
py -3 tools/pokemon/upload_batch.py --gen1-2 --mons-dir assets/pokemon --species 1 25 151 152 251 --port COM4 --execute
```

Para enviar todos de uma vez, remova o argumento `--species`, **somente quando os 251 arquivos estiverem gerados, auditados e o firmware com recepção genérica estiver instalado**. Os envios usam ACK por blocos de 256 bytes e verificação posterior; podem levar bastante tempo. Se precisar continuar após interrupção, **não force sobrescrita**: verifique os arquivos existentes e a resposta do dispositivo antes de reenviar.

Na tela Companion, os botões anterior/próximo percorrem os arquivos `p1.bin` a `p251.bin` realmente encontrados no cartão (além do Mimikyu #778). A busca é feita uma vez ao abrir Companion, sem tentar carregar as 251 animações em RAM. **É uma prévia, não uma seleção permanente de Pokémon de uma conta.**

**Importante:** essa expansão de arquivos **não configura automaticamente o gacha real** para sortear os 251 Pokémon. Regras de raridade, taxa de Shiny, posse, recompensa, persistência e confirmação do usuário permanecem separadas e desativadas, assim como antes.
