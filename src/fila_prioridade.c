#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/fila_prioridade.h"
#include "../include/aeronave.h"

/**
 * Inicializa uma fila de prioridade vazia
 * @param fila: Ponteiro para a fila a ser inicializada
 */
void fila_inicializar(fila_prioridade_t *fila)
{
    if (!fila) return;
    fila->inicio = NULL;
    fila->fim = NULL;
    fila->tamanho = 0;
}

/**
 * Insere uma aeronave na fila de prioridade de forma ordenada
 * Aeronaves com maior prioridade ficam no início da fila
 * Otimizado para verificar inserção no final rapidamente
 * @param fila: Ponteiro para a fila
 * @param aeronave: Ponteiro para a aeronave a ser inserida
 */
void fila_inserir(fila_prioridade_t *fila, aeronave_t *aeronave)
{
    if (!fila || !aeronave) return;

    no_fila_t *novo = malloc(sizeof(no_fila_t));
    if (!novo) {
        perror("malloc no_fila");
        return;
    }
    novo->aeronave = aeronave;
    novo->proximo = NULL;

    /* Inserir ordenado por prioridade (maior prioridade primeiro) */
    if (fila->inicio == NULL) {
        /* Fila vazia */
        fila->inicio = novo;
        fila->fim = novo;
    } else if (aeronave->prioridade > fila->inicio->aeronave->prioridade) {
        /* Inserir no início (maior prioridade) - O(1) */
        novo->proximo = fila->inicio;
        fila->inicio = novo;
    } else if (aeronave->prioridade <= fila->fim->aeronave->prioridade) {
        /* Otimização: inserir no final se prioridade menor que o último - O(1) */
        fila->fim->proximo = novo;
        fila->fim = novo;
    } else {
        /* Procurar posição correta na fila - O(n) apenas quando necessário */
        no_fila_t *atual = fila->inicio;
        while (atual->proximo != NULL && 
               atual->proximo->aeronave->prioridade >= aeronave->prioridade) {
            atual = atual->proximo;
        }
        novo->proximo = atual->proximo;
        atual->proximo = novo;
        if (novo->proximo == NULL) {
            fila->fim = novo;
        }
    }
    fila->tamanho++;
}

/**
 * Remove e retorna a aeronave de maior prioridade da fila
 * @param fila: Ponteiro para a fila
 * @return Ponteiro para a aeronave removida, ou NULL se a fila estiver vazia
 */
aeronave_t *fila_remover(fila_prioridade_t *fila)
{
    if (!fila || fila->inicio == NULL) return NULL;

    no_fila_t *removido = fila->inicio;
    aeronave_t *aeronave = removido->aeronave;
    
    fila->inicio = fila->inicio->proximo;
    if (fila->inicio == NULL) {
        fila->fim = NULL;
    }
    
    free(removido);
    fila->tamanho--;
    return aeronave;
}

/**
 * Verifica se a fila está vazia
 * @param fila: Ponteiro para a fila
 * @return true se a fila estiver vazia, false caso contrário
 */
bool fila_vazio(fila_prioridade_t *fila)
{
    return (fila == NULL || fila->inicio == NULL);
}

/**
 * Destroi a fila e libera toda a memória alocada
 * Nota: não libera as aeronaves, apenas os nós da fila
 * @param fila: Ponteiro para a fila a ser destruída
 */
void fila_destruir(fila_prioridade_t *fila)
{
    if (!fila) return;
    
    no_fila_t *atual = fila->inicio;
    while (atual != NULL) {
        no_fila_t *proximo = atual->proximo;
        free(atual);
        atual = proximo;
    }
    fila->inicio = NULL;
    fila->fim = NULL;
    fila->tamanho = 0;
}

/**
 * Imprime o conteúdo da fila de prioridade para debug
 * @param fila: Ponteiro para a fila a ser impressa
 */
void fila_imprimir(fila_prioridade_t *fila)
{
    if (!fila || fila->inicio == NULL) {
        printf("(vazia)\n");
        return;
    }
    
    no_fila_t *atual = fila->inicio;
    printf("[");
    while (atual != NULL) {
        printf("A%d(P:%u)", atual->aeronave->id, atual->aeronave->prioridade);
        atual = atual->proximo;
        if (atual != NULL) printf(", ");
    }
    printf("]\n");
}
