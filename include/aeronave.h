#ifdef AERONAVE_H
#define AERONAVE_H

#include <pthread.h>
#include <semaphore.h>
#include "utils.h"

typedef struct {
    int id;
    unsigned int prioridade;
    int *rota;
    int comprimento_rota;
    int setor_atual;
    int setor_destino;
    time_t tempo_solicitacao;
    time_t tempo_entrada;
    time_t *tempo_espera;
    int total_espera;
    sem_t sem_aeronave;
    pthread_t thread;
} aeronave_t;


aeronave_t *aeronave_criar(int id, int total_setores);
void aeronave_destruir(aeronave_t *aeronave);
void *aeronave_executa(void *arg);
void aeronave_imprimir_status(aeronave_t *aeronave);
void aeronave_registro_tempo_espera(aeronave_t *aeronave, time_t inicio);
doble aeronave_calcular_media_espera(aeronave_t *aeronave);

#endif // AERONAVE_H