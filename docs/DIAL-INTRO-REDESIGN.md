# Nova identidade do splash

A antiga imagem estática da moeda foi substituída na tela inicial por
um emblema de dial construído com objetos LVGL em resolução nativa.
Paleta: azul-marinho, turquesa e dourado. O nome **DIAL DOS PRIMOS**
permanece por enquanto, podendo ser rebatizado sem alteração de dados.

- Animação por opacidade, sem zoom/flip de bitmap e sem arte quadrada.
- Duração aproximada 1,9 s; depois abre o mesmo pré-jogo.
- O arquivo local opcional `dial_intro_asset.h` não é apagado, só deixa
  de ser exibido no splash. Coin-flip e dados ficam intactos.
- Confirmar visual no hardware; CI não valida estética ou legibilidade.
