# 0. Compilar os programas (refaz sozinho se algo em src/ mudar)
make

# 1. Converter a foto para PGM em tons de cinza
convert data/images/cafe.jpg -colorspace Gray -resize 512x512 -compress none data/images/cafe.pgm

# 2. Conferir o cabeçalho (a primeira linha tem que ser P2)
head -n 3 data/images/cafe.pgm

# 3. Criar as pastas de saída (uma pasta de frames por experimento)
mkdir -p results/frames/base results/frames/k2 results/frames/k2e

# 4a. Baseline (código do artigo). -I 2 = inicialização determinística, para comparar
./build/agbn-baseline -I 2 -o -f results/frames/base/cafe_ data/images/cafe.pgm 5000 1000 results/cafe_base.png results/cafe_base.txt

# 4b. K = 2: média das 2 distâncias, depois ao quadrado
./build/agbn-serial -K 2 -I 2 -o -f results/frames/k2/cafe_ data/images/cafe.pgm 5000 1000 results/cafe_k2.png results/cafe_k2.txt

# 4c. K = 2 com -e: média dos quadrados das distâncias
./build/agbn-serial -K 2 -e -I 2 -o -f results/frames/k2e/cafe_ data/images/cafe.pgm 5000 1000 results/cafe_k2e.png results/cafe_k2e.txt

# 5. Ver os resultados
xdg-open results/cafe_base.png
xdg-open results/cafe_k2.png
xdg-open results/cafe_k2e.png

# 6. Montar os GIFs com os frames
convert -delay 8 -loop 0 results/frames/base/cafe_*.png results/cafe_base.gif
convert -delay 8 -loop 0 results/frames/k2/cafe_*.png results/cafe_k2.gif
convert -delay 8 -loop 0 results/frames/k2e/cafe_*.png results/cafe_k2e.gif

# 7. (opcional) Conferir que -K 1 reproduz o artigo: tem que imprimir IDENTICAL
#    -F 0 desliga a gravação de frames
./build/agbn-baseline -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/a.txt
./build/agbn-serial -K 1 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/b.txt
cmp results/a.txt results/b.txt && echo IDENTICAL