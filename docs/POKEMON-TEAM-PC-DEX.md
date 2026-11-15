# DIAL DOS PRIMOS: Pokémon PC, seis no time e Pokédex

**Status:** núcleo puro de regras criado. Ainda NÃO existe vínculo do PC com
NVS/SPIFFS, nem botões para modificar perfis reais; **não há migração**.
PR draft; nenhuma alteração é feita em dados existentes.

## Decisão de produto
- Um jogador pode manter até **seis Pokémon no time**, com **um principal**
  (`active_index`). O XP das partidas confirmadas é atribuído ao principal.
- **PC**: contém indivíduos realmente capturados e fora do time, com ID
  da espécie, Shiny, nível e XP. Dois Pokémon da mesma espécie podem
  existir separadamente. Trocar/substituir preserva atributos.
- **Pokédex**: registra espécies já descobertas/capturadas, incluindo
  forma Shiny. Registro sozinho não concede posse. A galeria de
  sprites instalada no SD é outro conceito, igualmente sem posse.
- O LAB permanece simulação, sem nenhuma captura, recompensa, PC ou
  registro de Pokédex real.

## Segurança técnica
- `pokemon_pc.c`: motor sem I/O, instâncias em buffer fornecido pelo
  chamador; testa validação, troca, depósito, retirada e escolha de
  principal sem criar perfis. Quem chamar deve autenticar o dono.
- `pk_profile_t` v1 e os registros existentes NVS/SPIFFS permanecem
  intocados. PC persistente deve usar **outro arquivo** com backups
  A/B, geração, checksum e recuperação de falhas; transações entre
  o perfil/time e PC precisam de coordenação e testes de power loss.
- Nunca confundir Playgroup seat com user ID nem produzir Pokémon
  reais a partir de sprite instalado ou sorteio do LAB.

## Escopo dos próximos incrementos
1. Identidade validada do jogador no menu Pokémon fora de uma partida.
2. Leitura do perfil + PC persistente isolado; TEAM/PC/DEX navegáveis
   com seis slots, status de propriedade e troca do principal.
3. Download local de sprites normal/Shiny da Pokédex #1..1025,
   com fallback quando não houver GIF Shiny, nomes e checks.
4. Renderização Shiny real e efeito final da raridade
   (branco, verde, roxo, dourado, brilho extra Shiny).
5. Gacha real somente depois do inventário persistente, trava de
   autenticação, log de transações e testes de recuperação.

Até a espécie #1025, 2 versões por espécie são **2.050 arquivos**.
Com base em 3,43 MiB para 251 sprites: estimativa ~28,0 MiB.
A variação real depende dos GIFs; 300 MiB disponíveis no SD são
mais que suficientes para esse total estimado.
