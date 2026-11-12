# 0. Compilar (refaz sozinho se algo em src/ ou no Makefile mudar)
make

# 1. Converter a foto para PGM em tons de cinza
convert data/images/cafe.jpg -colorspace Gray -resize 512x512 -compress none data/images/cafe.pgm

# 2. Conferir o cabeçalho (a primeira linha tem que ser P2)
head -n 3 data/images/cafe.pgm

# 3. Criar a pasta de saída
mkdir -p results/frames

# 4a. Baseline (código do artigo): serial, 1 thread. -I 2 = inicialização determinística, para comparar
mkdir -p results/frames/base
./build/agbn-baseline -I 2 -o -f results/frames/base/cafe_ data/images/cafe.pgm 5000 1000 results/cafe_base.png results/cafe_base.txt

# 4b. Experimento configurável: mude as linhas de configuração e cole o bloco inteiro
K=2                  # número de vizinhos (K=1 é o artigo)
AVG=""               # ""  = média das distâncias | "-e" = média dos quadrados das distâncias
MODE=""              # ""  = serial (como no artigo, 1 thread) | "-P" = paralelo (várias threads)
THREADS=$(nproc)     # número de threads; só vale com MODE="-P"
TAG="k2"             # nome do experimento (nomeia os arquivos de saída); mude junto com as opções

mkdir -p results/frames/$TAG
./build/agbn-serial -K $K $AVG $MODE -J $THREADS -I 2 -o -f results/frames/$TAG/cafe_ data/images/cafe.pgm 5000 1000 results/cafe_$TAG.png results/cafe_$TAG.txt

#    Receitas prontas (valores para as linhas de configuração acima):
#      serial,   K=2:             K=2  AVG=""    MODE=""    TAG="k2"
#      serial,   K=2, quadrados:  K=2  AVG="-e"  MODE=""    TAG="k2e"
#      paralelo, K=1:             K=1  AVG=""    MODE="-P"  TAG="k1_par"
#      paralelo, K=2:             K=2  AVG=""    MODE="-P"  TAG="k2_par"
#      paralelo, K=3:             K=3  AVG=""    MODE="-P"  TAG="k3_par"
#
#    O tempo impresso no final ("Total time") é o tempo real (wall-clock).
#    Para medir o ganho das threads: rode o mesmo experimento com THREADS=1 e com THREADS=$(nproc).

# 5. Ver o resultado do experimento
xdg-open results/cafe_$TAG.png

# 6. Montar o GIF com os frames
convert -delay 8 -loop 0 results/frames/$TAG/cafe_*.png results/cafe_$TAG.gif
xdg-open results/cafe_$TAG.gif

# 7. (opcional) Verificações. Cada uma tem que imprimir IDENTICAL.
#    -F 0 desliga a gravação de frames.
#    7a. K=1 serial reproduz o artigo
./build/agbn-baseline -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/a.txt
./build/agbn-serial -K 1 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/b.txt
cmp results/a.txt results/b.txt && echo IDENTICAL
#    7b. o modo paralelo dá o mesmo resultado com qualquer número de threads
./build/agbn-serial -K 2 -P -J 1 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/p1.txt
./build/agbn-serial -K 2 -P -J 4 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/p4.txt
cmp results/p1.txt results/p4.txt && echo IDENTICAL
