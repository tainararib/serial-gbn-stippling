# 0. Compilar os programas (uma vez só)
make

# 1. Converter a foto para PGM em tons de cinza
convert data/images/cafe.jpg -colorspace Gray -resize 512x512 -compress none data/images/cafe.pgm

# 2. Conferir o cabeçalho (a primeira linha tem que ser P2)
head -n 3 data/images/cafe.pgm

# 3. Criar as pastas de saída
mkdir -p results/frames

# 4. Rodar o stippling: 5.000 pontos, 1.000 iterações, gravando os frames
./build/agbn-baseline -o -f results/frames/cafe_ data/images/cafe.pgm 5000 1000 results/cafe.png results/cafe.txt

# 5. Ver o resultado
xdg-open results/cafe.png

# 6. Montar o GIF com os frames
convert -delay 8 -loop 0 results/frames/cafe_*.png results/cafe.gif
xdg-open results/cafe.gif