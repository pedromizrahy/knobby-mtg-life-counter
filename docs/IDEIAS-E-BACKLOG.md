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

Feedback: **a arte da moeda e a tipografia da intro atual estão ruins**. Redesenhar ambos, preservando o nome exato **DIAL DOS PRIMOS**.

Diagnóstico no firmware atual (`knobby/src/intro.c`):
- Fundo preto, moeda central de até 280 px, label "DIAL DOS" em Montserrat 32, texto "P R I M O S" em Montserrat 14, linha dourada; intro ~36 frames x 55 ms.
- A moeda de alta qualidade depende de asset local (`dial_intro_asset.h` / `dial_sprite`), não versionado no GitHub, segundo `docs/visual-assets-v5.md`; não presumir que git pull altere essa arte.
- Há histórico de borda branca/pixelada inferior, tela preta/arte degradada e problema com flip. Evitar reintroduzir esses defeitos.

Direção proposta para validação visual:
- Criar uma **identidade única**: símbolo/moeda de alto contraste, material dourado controlado, linhas e silhueta legíveis na tela redonda 360×360.
- Redesenhar lettering de "DIAL DOS PRIMOS" como uma peça tipográfica única e hierarquizada, em vez de texto genérico quebrado em Montserrat com espaçamento artificial; garantir acentos e leitura no hardware.
- Duas ou três propostas **visuais completas**, comparadas em tamanho real (360 px) antes de substituir assets.
- Preferir intro estática de alta qualidade com movimento sutil, sem flip obrigatório, sem aumento por interpolação e sem bordas de asset.
- Garantir consistência de composição, safe-area circular, anti-aliasing/rgb565 e budget de RAM/flash.
- Não modificar Magic gameplay, arte da moeda em outros modos, timings de inicialização ou assets locais sem inspeção e validação física.

## Prioridade e sequência

1. **Concluir teste físico do firmware atual:** garantir sprite Mimikyu visível, animando e cenários/botões utilizáveis.
2. Criar/validar opções de **nova logo e lettering da intro**, sem alterar firmware em uso.
3. Implementar badges dos tipos Pokémon com maior fidelidade visual.
4. Investigar dados reais do Playgroup.gg e estrutura de ranking por grupo/Favorite Deck.
5. Implementar conquistas/coroa conforme dados válidos.

**Registrado como ideias**, não funcionalidades concluídas.
