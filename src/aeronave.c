#include "../include/aeronave.h"
#include "../include/controlador.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern int total_setores;
extern int total_aeronaves;
extern aeronave_t **aeronaves;
extern sem_t mutex_ctrl;
extern sem_t mutex_console;


aeronave_t *aeronave_criar(int id, int total_setores) {
    aeronave_t *a = malloc(sizeof(aeronave_t));
    if (a == NULL) return NULL;
    a->id = id;
    a->prioridade = 1 + (rand() % 1000);
    a->setor_atual = -1;
    a->setor_destino = -1;
    a->tempo_solicitacao = 0;
    a->tempo_entrada = time(NULL);
    a->total_espera = 0;
    if (total_setores < 2) total_setores = 2;
    a->comprimento_rota = 2 + (rand() % (total_setores - 1));
    
    a->rota = malloc(a->comprimento_rota * sizeof(int));
    a->tempo_espera = malloc(a->comprimento_rota * sizeof(time_t));
    
    if (a->rota == NULL || a->tempo_espera == NULL) {
        free(a->rota);
        free(a->tempo_espera);
        free(a);
        return NULL;
    }
    
    memset(a->tempo_espera, 0, a->comprimento_rota * sizeof(time_t));
    
    for (int i = 0; i < a->comprimento_rota; i++) {
        a->rota[i] = rand() % total_setores;
    }
    
    if (sem_init(&a->sem_aeronave, 0, 0) != 0) {
        free(a->rota);
        free(a->tempo_espera);
        free(a);
        return NULL;
    }
    
    return a;
}

void aeronave_destruir(aeronave_t *aeronave) {
    if (aeronave == NULL) return;
    
    free(aeronave->rota);
    free(aeronave->tempo_espera);
    sem_destroy(&aeronave->sem_aeronave);
    free(aeronave);
}

void aeronave_imprimir_status(aeronave_t *aeronave) {
    if (aeronave == NULL) return;
    
    sem_wait(&mutex_console);
    imprimir_timestamp();
    printf("Aeronave %3d [Prio:%4u] | Setor: S%-3d | Destino: S%-3d\n",
           aeronave->id, aeronave->prioridade, 
           aeronave->setor_atual, aeronave->setor_destino);
    sem_post(&mutex_console);
}

void aeronave_registro_tempo_espera(aeronave_t *aeronave, time_t inicio) {
    if (aeronave == NULL || inicio == 0) return;
    
    time_t fim = time(NULL);
    if (aeronave->total_espera < aeronave->comprimento_rota) {
        aeronave->tempo_espera[aeronave->total_espera] = fim - inicio;
        aeronave->total_espera++;
    }
}

double aeronave_calcular_media_espera(aeronave_t *aeronave) {
    if (aeronave == NULL || aeronave->total_espera == 0) return 0.0;
    
    double soma = 0.0;
    for (int i = 0; i < aeronave->total_espera; i++) {
        soma += aeronave->tempo_espera[i];
    }
    return soma / aeronave->total_espera;
}

void *aeronave_executa(void *arg) {
    aeronave_t *a = (aeronave_t *)arg;
    if (a == NULL) pthread_exit(NULL);
    
    sem_wait(&mutex_console);
    imprimir_timestamp();
    printf("Aeronave %3d [Prio:%4u] Iniciou - Rota: ", a->id, a->prioridade);
    for (int i = 0; i < a->comprimento_rota; i++) {
        printf("S%d", a->rota[i]);
        if (i < a->comprimento_rota - 1) printf(" -> ");
    }
    printf("\n");
    sem_post(&mutex_console);
    
    for (int pos = 0; pos < a->comprimento_rota - 1; pos++) {
        int setor_origem = a->rota[pos];
        int setor_destino = a->rota[pos + 1];
        
        time_t inicio_espera = time(NULL);
        
        int sucesso = atc_solicitar_setor(a, setor_destino);
        if (!sucesso) {
            sem_wait(&mutex_console);
            imprimir_timestamp();
            printf("Aeronave %3d Falha ao acessar S%d\n", a->id, setor_destino);
            sem_post(&mutex_console);
            break;
        }
        
        aeronave_registro_tempo_espera(a, inicio_espera);
        
        if (pos > 0) {
            atc_liberar_setor(a, setor_origem);
        }
        
        int tempo_voo = 1000000 + (rand() % 500000);
        sleep(tempo_voo);
        
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %3d Concluiu S%d (%d ms)\n", a->id, setor_destino, tempo_voo / 1000);
        sem_post(&mutex_console);
        
        a->setor_atual = setor_destino;
    }
    
    if (a->setor_atual >= 0) {
        atc_liberar_setor(a, a->setor_atual);
    }
    
    sem_wait(&mutex_console);
    imprimir_timestamp();
    printf("Aeronave %3d Concluída! Tempo médio espera: %.2fs\n", a->id, aeronave_calcular_media_espera(a));
    sem_post(&mutex_console);
    
    pthread_exit(NULL);
}