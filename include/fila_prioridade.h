#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define TEMPO_BASE 1000000
#define PRIORIDADE_MAX 1000

// Forward declaration
struct aeronave_t;

void timestamp_print(); 
int* gerar_rotas_aleatorias(int comprimento, int total_setores);
double calcular_tempo_medio(struct aeronave_t **aeronaves, int total_aeronaves);
void imprimir_timestamp();
int gerar_comprimento_rota(int total_setores);

#endif // UTILS_H