# Serial GBN Stippling — Projeto de Pesquisa (CG, IFNMG 2026/2)

Investigação baseada em: A. G. M. Ahmed, *Serial Gaussian-Blue-Noise Stippling*,
CGVC 2024. DOI: 10.2312/cgvc.20241226

## Estrutura
- `third_party/ahmed2024/` — código original do autor (CC BY 4.0), sem modificações; usado como baseline
- `src/` — versão a ser modificada 
- `data/images/` — imagens de entrada (PGM ASCII, P2)


## Dependências (Ubuntu)
    sudo apt install build-essential libcairo2-dev imagemagick

## Compilação
    make

## Execução
    ./build/agbn-baseline <imagem.pgm> <nº pontos> <iterações> [saida.png saida.txt]

## Ferramentas de IA utilizadas
(a preencher ao longo do projeto)