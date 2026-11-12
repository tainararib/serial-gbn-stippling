# Usa ">" como prefixo de receita para evitar problemas com TAB ao copiar/colar
.RECIPEPREFIX := >

CXX      ?= g++
CXXFLAGS ?= -O3
LDLIBS   := -lcairo

all: build/agbn-baseline build/agbn-serial

# Código original do artigo, sem modificações (baseline)
build/agbn-baseline: third_party/ahmed2024/agbn-serial.cpp third_party/ahmed2024/common.h third_party/ahmed2024/2048.h
> @mkdir -p build
> $(CXX) $(CXXFLAGS) -o $@ $< $(LDLIBS)

# Versão a ser modificada (-fopenmp habilita o modo paralelo -P com várias threads)
build/agbn-serial: src/agbn-serial.cpp src/common.h src/2048.h
> @mkdir -p build
> $(CXX) $(CXXFLAGS) -fopenmp -o $@ $< $(LDLIBS)

clean:
> rm -rf build

.PHONY: all clean