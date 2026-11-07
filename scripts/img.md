# 0. Compilar os programas (só se ainda não compilou)
make

# 1. Converter a imagem para PGM em tons de cinza
convert data/images/img.png -background white -alpha remove -alpha off -colorspace Gray -resize 512x512 -compress none data/images/img.pgm

# 2. Conferir o cabeçalho (a primeira linha tem que ser P2)
head -n 3 data/images/img.pgm

# 3. Criar as pastas de saída
mkdir -p results/frames/img

# 4. Rodar o stippling: 10.000 pontos, 1.000 iterações, gravando os frames
./build/agbn-baseline -o -f results/frames/img/img_ data/images/img.pgm 10000 1000 results/img.png results/img.txt
# Ou sem gravar os frames
./build/agbn-baseline -o -F 0 data/images/img.pgm 10000 1000 results/img.png results/img.txt

# 5. Ver o resultado
xdg-open results/img.png

# 6. Montar o GIF com os frames
convert -delay 8 -loop 0 results/frames/img/img_*.png results/img.gif
xdg-open results/img.gif

