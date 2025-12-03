#ifndef FILA_PRIORIDADE_H
#define FILA_PRIORIDADE_H

#include "../include/aeronave.h"
#include <stdbool.h>

typedef struct no_fila {
    aeronave_t *aeronave;
    struct no_fila *proximo;
} no_fila_t;

typedef struct {
    no_fila_t *inicio;
    no_fila_t *fim;
    int tamanho;
} fila_prioridade_t;


aeronave_t *fila_remover(fila_prioridade_t *fila);
aeronave_t *fila_espiar(fila_prioridade_t *fila);
void fila_inicializar(fila_prioridade_t *fila);
void fila_inserir(fila_prioridade_t *fila, aeronave_t *aeronave);
bool fila_vazio(fila_prioridade_t *fila);
void fila_destruir(fila_prioridade_t *fila);
void fila_imprimir(fila_prioridade_t *fila);
void fila_rotacionar(fila_prioridade_t *fila);
bool fila_remover_aeronave(fila_prioridade_t *fila, aeronave_t *aeronave);

#endif // FILA_PRIORIDADE_H