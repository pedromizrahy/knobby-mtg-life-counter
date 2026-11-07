# Playgroup — quatro Pokémon principais por página

**Status:** visualização *read-only* opcional preparada para teste no firmware.
Aparece em `PLAYGROUP > PLAYERS & DECKS > POKEMON` durante a preparação
de uma partida, sem substituir `MULLIGANS` nem os contadores de Magic.

- Cada cartão corresponde a um jogador realmente selecionado no Playgroup.
- No máximo quatro cartões por página; em mesas de cinco ou seis, NEXT
  percorre os demais. BACK retorna exatamente à lista PLAYERS & DECKS.
- Apresenta somente `profile.team[profile.active_index]`: um único
  Pokémon principal por jogador. Os outros cinco continuam no time/PC,
  mas não aparecem nessa tela.
- O Playgroup fornece um user ID explícito por membro selecionado. Guests,
  IDs inválidos/duplicados, membros sem perfil, time vazio ou perfil corrupto
  aparecem `NOT LINKED`. Não se deduz ID a partir de nome ou cadeira.
- Somente **leitura**: procura perfil SPIFFS validado e, se ausente,
  verifica o perfil NVS antigo; perfil SPIFFS corrupto não é sobrescrito
  nem mascarado por NVS. Não inicializa starters.
- Sprites são lidos do cartão SD em uma sessão somente leitura. Cada
  cartão tem uma cópia **estática** do primeiro frame TPK2 em PSRAM; o
  renderer animado do Companion não é tocado. Isso evita que quatro
  cartões compartilhem a mesma imagem ou causem mosaico.
- Para um principal Shiny, tenta primeiro `/mons/p<ID>s.bin` e depois
  `/mons/p<ID>.bin`. A fallback é rotulada `SHINY / ART NORMAL`,
  nunca anunciada como arte Shiny verdadeira.
- O catálogo instalado no SD não concede Pokémon ao jogador. Sorteios
  isolados do LAB também não preenchem o perfil.
- Nenhuma alteração automática em Pokémon/time, perfis, recompensas,
  Playgroup nem game events. Sem UI durante a partida nesta etapa.

**Teste de hardware:** iniciar preparação Playgroup com IDs válidos e
perfis já existentes, tocar POKEMON no resumo, navegar e voltar;
confirmar que a tela MULLIGANS permanece operacional. Um usuário
sem perfil deverá mostrar NOT LINKED, pois o vínculo real de capturas
e criação de perfil ainda é outra etapa.
