#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/fila_prioridade.h"
#include "../include/aeronave.h"

/**
 * Inicializa uma fila de prioridade com valores padrão
 * @param fila: Ponteiro para a estrutura da fila de prioridade
*/
void fila_inicializar(fila_prioridade_t *fila)
{
    if (!fila) return;
    fila->inicio = NULL;
    fila->fim = NULL;
    fila->tamanho = 0;
}

/**
 * Insere uma aeronave na fila de prioridade mantendo a ordem por prioridade
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 * @param aeronave: Ponteiro para a aeronave a ser inserida
 */
void fila_inserir(fila_prioridade_t *fila, aeronave_t *aeronave) {
    if (!fila || !aeronave) return;

    no_fila_t *novo = malloc(sizeof(no_fila_t));
    if (!novo) {
        perror("malloc no_fila");
        return;
    }
    novo->aeronave = aeronave;
    novo->proximo = NULL;

    if (fila->inicio == NULL) {
        fila->inicio = novo;
        fila->fim = novo;
    } else if (aeronave->prioridade > fila->inicio->aeronave->prioridade) {
        novo->proximo = fila->inicio;
        fila->inicio = novo;
    } else if (fila->fim != NULL && aeronave->prioridade <= fila->fim->aeronave->prioridade) {
        fila->fim->proximo = novo;
        fila->fim = novo;
    } else {
        no_fila_t *atual = fila->inicio;
        // Percorre a fila até encontrar posição correta (maior prioridade primeiro)
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
 * Remove e retorna a aeronave com maior prioridade da fila
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 * @return Ponteiro para a aeronave removida ou NULL se a fila estiver vazia
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
 * Verifica se a fila de prioridade está vazia
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 * @return true se a fila estiver vazia, false caso contrário
 */
bool fila_vazio(fila_prioridade_t *fila)
{
    return (fila == NULL || fila->inicio == NULL);
}

/**
 * Libera toda a memória alocada para a fila de prioridade
 * @param fila: Ponteiro para a estrutura da fila de prioridade
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
 * Imprime o conteúdo da fila de prioridade no formato [A1(P:5), A2(P:3), ...]
 * @param fila: Ponteiro para a estrutura da fila de prioridade
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

/**
 * Retorna a aeronave com maior prioridade sem removê-la da fila
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 * @return Ponteiro para a aeronave no início da fila ou NULL se vazia
 */
aeronave_t *fila_espiar(fila_prioridade_t *fila)
{
    if (!fila || fila->inicio == NULL) return NULL;
    return fila->inicio->aeronave;
}

/**
 * Rotaciona a fila movendo o primeiro elemento para o final
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 */
void fila_rotacionar(fila_prioridade_t *fila)
{
    if (!fila || fila->tamanho < 2) return;
    
    no_fila_t *primeiro = fila->inicio;
    fila->inicio = primeiro->proximo;
    primeiro->proximo = NULL;
    fila->fim->proximo = primeiro;
    fila->fim = primeiro;
}

/**
 * Remove uma aeronave específica da fila de prioridade
 * @param fila: Ponteiro para a estrutura da fila de prioridade
 * @param aeronave: Ponteiro para a aeronave a ser removida
 * @return true se a aeronave foi encontrada e removida, false caso contrário
 */
bool fila_remover_aeronave(fila_prioridade_t *fila, aeronave_t *aeronave)
{
    if (!fila || !aeronave || fila->inicio == NULL) return false;
    
    if (fila->inicio->aeronave->id == aeronave->id) {
        no_fila_t *removido = fila->inicio;
        fila->inicio = fila->inicio->proximo;
        if (fila->inicio == NULL) {
            fila->fim = NULL;
        }
        free(removido);
        fila->tamanho--;
        return true;
    }
    
    no_fila_t *anterior = fila->inicio;
    while (anterior->proximo != NULL) {
        if (anterior->proximo->aeronave->id == aeronave->id) {
            no_fila_t *removido = anterior->proximo;
            anterior->proximo = removido->proximo;
            if (removido == fila->fim) {
                fila->fim = anterior;
            }
            free(removido);
            fila->tamanho--;
            return true;
        }
        anterior = anterior->proximo;
    }
    
    return false;
}