#ifndef AERONAVE_H
#define AERONAVE_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>
#include "../include/utils.h"

typedef struct aeronave_t {
    int id;
    unsigned int prioridade;
    unsigned int prioridade_original;
    int *rota;
    int comprimento_rota;
    int setor_atual;
    int setor_destino;
    struct timespec tempo_solicitacao;
    time_t tempo_entrada;
    double *tempo_espera;
    int total_espera;
    sem_t sem_aeronave;
    pthread_t thread;
    bool precisa_recuar;
    int contador_recuos;
    int contador_esperas_longas;
} aeronave_t;


aeronave_t *aeronave_criar(int id, int total_setores);
void aeronave_destruir(aeronave_t *aeronave);
void *aeronave_executa(void *arg);
void aeronave_imprimir_status(aeronave_t *aeronave);
void aeronave_registro_tempo_espera(aeronave_t *aeronave, struct timespec inicio);
double aeronave_calcular_media_espera(aeronave_t *aeronave);

#endif // AERONAVE_H