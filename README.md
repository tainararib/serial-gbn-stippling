# Serial GBN Stippling — Estimativa de área por k vizinhos e multithreading

Projeto de Pesquisa da disciplina de Computação Gráfica (IFNMG - Campus Montes Claros, 2026/2).

**Título:** Estimativa de área por k vizinhos e multithreading no Serial Gaussian-Blue-Noise Stippling: um estudo experimental

**Equipe:** Gabriel Vinícius Borges Ferreira e Tainara Ribeiro Santos  
**Professor:** Wagner Ferreira de Barros

## 1. Artigo-base

A. G. M. Ahmed. *Serial Gaussian-Blue-Noise Stippling*. Computer Graphics and Visual Computing (CGVC), 2024.
DOI: [10.2312/cgvc.20241226](https://doi.org/10.2312/cgvc.20241226)

O código dos autores está em `third_party/ahmed2024/` (licença CC BY 4.0), **sem nenhuma modificação**, e é usado como baseline.

## 2. Questão de investigação e modificações

O artigo-base gera pontilhados (stippling) com distribuição de ruído azul usando uma regra local: o fator de forma de cada ponto depende da distância ao seu vizinho mais próximo. O projeto investiga duas modificações:

1. **Estimativa de área por k vizinhos.** A distância usada na regra local passa a ser calculada com os `K` vizinhos mais próximos, em vez de apenas o mais próximo. Há duas variantes:
   - média das distâncias, elevada ao quadrado (padrão);
   - média dos quadrados das distâncias (opção `-e`).

   Com `K = 1`, o resultado é idêntico ao do artigo (verificado com `cmp`, ver seção 6).
2. **Multithreading com OpenMP** no modo simultâneo (`-P`, `-J <threads>`). O resultado é idêntico para qualquer número de threads (verificado, ver seção 6).

Pergunta: usar `K > 1` melhora a qualidade da distribuição? Qual o ganho de tempo com threads, sem alterar o resultado?

## 3. Estrutura do repositório

| Caminho | Conteúdo |
|---|---|
| `third_party/ahmed2024/` | Código original do artigo (CC BY 4.0), sem modificações (baseline) |
| `src/` | Versão modificada pela equipe (estimador por k vizinhos e OpenMP) |
| `data/images/` | Imagens de entrada (`cafe.jpg`, `img.png`) e suas versões PGM ASCII (P2) |
| `scripts/cafe.md`, `scripts/img.md` | Passo a passo dos experimentos (comandos prontos) |
| `Makefile` | Compila `build/agbn-baseline` e `build/agbn-serial` |
| `results/` | Saídas dos experimentos (não versionada, gerada pelos scripts) |
| `artigo/` | **[A ADICIONAR até 29/11]** Fontes LaTeX do artigo, figuras e `.bib` |

## 4. Dependências e instalação (Ubuntu)

    sudo apt install build-essential libcairo2-dev imagemagick
    make

- `build-essential`: compilador `g++` com suporte a OpenMP (`-fopenmp`) e `make`.
- `libcairo2-dev`: gravação de PNG/PDF/EPS.
- `imagemagick`: conversão de imagens para PGM e montagem de GIFs (`convert`).

A compilação gera dois executáveis:

- `build/agbn-baseline`: código original (`-O3`).
- `build/agbn-serial`: versão da equipe (`-O3 -fopenmp`).

**Ambiente usado nos experimentos:** [A PREENCHER: CPU e número de núcleos, memória, sistema operacional, versão do g++].

## 5. Execução

    ./build/agbn-baseline [opções] <imagem.pgm> <nº de pontos> <iterações> [saida.png saida.txt]
    ./build/agbn-serial   [opções] <imagem.pgm> <nº de pontos> <iterações> [saida.png saida.txt]

Opções adicionadas pela equipe (apenas em `agbn-serial`):

| Opção | Efeito |
|---|---|
| `-K <k>` | Número de vizinhos usados no fator de forma. `K = 1` reproduz o artigo. **O padrão do programa é `K = 2`**; passe `-K 1` para o baseline |
| `-e` | Com `-K`: usa a média dos quadrados das distâncias, em vez do quadrado da média |
| `-P` | Modo simultâneo: atualiza os pontos de uma vez (como em GPU), em paralelo com OpenMP |
| `-J <threads>` | Número de threads no modo `-P` (padrão: todos os núcleos) |

Opções importantes herdadas do código original: `-I <modo>` (inicialização), `-o` (PNG opaco), `-f <prefixo>` (grava frames), `-F 0` (não grava frames). A lista completa aparece ao rodar o programa sem argumentos.

Os comandos prontos para as imagens estão em `scripts/cafe.md` e `scripts/img.md`. Exemplo mínimo:

    mkdir -p results
    ./build/agbn-serial -K 2 -P -J 4 -I 2 -o -F 0 data/images/cafe.pgm 5000 1000 results/cafe_k2.png results/cafe_k2.txt

O tempo impresso ao final (`Total time`) é o tempo real (wall-clock).

## 6. Como reproduzir os resultados

### Parâmetros, sementes e determinismo

- Todos os experimentos usam `-I 2`, inicialização **determinística** do código original. Com ela, a mesma entrada gera sempre a mesma saída. As inicializações `-I 0` (aleatória) e `-I 1` (aleatória ponderada) não aceitam semente e não são usadas nas comparações.
- Imagens de entrada: `cafe.pgm` e `img.pgm`, 512 × 512 (conversão descrita em `scripts/*.md`).
- Nos experimentos preliminares: 2.000 a 10.000 pontos e 1.000 iterações.

### Verificações de corretude (cada uma deve imprimir `IDENTICAL`)

    # K = 1 reproduz o artigo
    ./build/agbn-baseline -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/a.txt
    ./build/agbn-serial -K 1 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/b.txt
    cmp results/a.txt results/b.txt && echo IDENTICAL

    # o modo paralelo dá o mesmo resultado com qualquer número de threads
    ./build/agbn-serial -K 2 -P -J 1 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/p1.txt
    ./build/agbn-serial -K 2 -P -J 4 -I 2 -F 0 data/images/cafe.pgm 5000 1000 results/p4.txt
    cmp results/p1.txt results/p4.txt && echo IDENTICAL

### Experimentos

1. **Baseline** (artigo): bloco 4a de `scripts/cafe.md`.
2. **Varredura de K** (`K = 1, 2, 3, …`, com e sem `-e`): bloco 4b de `scripts/cafe.md`, alterando `K`, `AVG` e `TAG`.
3. **Efeito das threads:** rodar o mesmo experimento com `-P -J 1` e com `-P -J $(nproc)` e comparar o `Total time`. Para separar o efeito do algoritmo do efeito das threads, compare sempre com o mesmo `K`.


## 7. Ferramentas de IA utilizadas

Conforme as regras da disciplina, a equipe registra as ferramentas usadas e o tipo de apoio. A equipe é responsável por entender, testar e validar tudo o que foi incorporado.

- **Claude (Anthropic):** apoio na compreensão do artigo-base; revisão do código modificado (estimador por k vizinhos e OpenMP), incluindo testes de corretude e de memória; sugestão de experimentos e métricas; apoio à escrita e revisão do artigo em LaTeX, dos slides e dos roteiros de apresentação; preenchimento de documentos de cadastro do projeto.

## 8. Licenças

- `third_party/ahmed2024/`: código de A. G. M. Ahmed, licença CC BY 4.0.
