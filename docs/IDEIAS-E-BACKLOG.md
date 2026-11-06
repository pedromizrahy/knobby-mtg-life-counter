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
