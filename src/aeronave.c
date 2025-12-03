#include "../include/aeronave.h"
#include "../include/controlador.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Variáveis globais externas definidas em controlador.c
extern int total_setores;
extern int total_aeronaves;
extern aeronave_t **aeronaves;
extern sem_t mutex_ctrl;
extern sem_t mutex_console;


/**
 * Cria uma nova aeronave com parâmetros aleatórios
 * @param id: Identificador único da aeronave
 * @param total_setores: Número total de setores disponíveis no espaço aéreo
 * @return Ponteiro para a aeronave criada ou NULL em caso de falha
 */
aeronave_t *aeronave_criar(int id, int total_setores) {
    aeronave_t *a = malloc(sizeof(aeronave_t));
    if (a == NULL) return NULL;
    a->id = id;
    a->prioridade = 1 + (rand() % 1000);
    a->setor_atual = -1;
    a->setor_destino = -1;
    a->tempo_solicitacao.tv_sec = 0;
    a->tempo_solicitacao.tv_nsec = 0;
    a->tempo_entrada = time(NULL);
    a->total_espera = 0;
    a->precisa_recuar = false;
    a->prioridade_original = a->prioridade;
    a->contador_recuos = 0;
    a->contador_esperas_longas = 0;
    if (total_setores < 2) total_setores = 2;
    a->comprimento_rota = 2 + (rand() % (total_setores - 1));
    
    a->rota = malloc(a->comprimento_rota * sizeof(int));
    a->tempo_espera = malloc(a->comprimento_rota * sizeof(double));
    
    if (a->rota == NULL || a->tempo_espera == NULL) {
        free(a->rota);
        free(a->tempo_espera);
        free(a);
        return NULL;
    }
    
    memset(a->tempo_espera, 0, a->comprimento_rota * sizeof(double));
    
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

/**
 * Libera toda a memória alocada para uma aeronave
 * @param aeronave: Ponteiro para a aeronave a ser destruída
 */
void aeronave_destruir(aeronave_t *aeronave) {
    if (aeronave == NULL) return;
    
    free(aeronave->rota);
    free(aeronave->tempo_espera);
    sem_destroy(&aeronave->sem_aeronave);
    free(aeronave);
}

/**
 * Imprime o status atual de uma aeronave (ID, prioridade, setor atual e destino)
 * @param aeronave: Ponteiro para a aeronave cujo status será impresso
 */
void aeronave_imprimir_status(aeronave_t *aeronave) {
    if (aeronave == NULL) return;
    
    sem_wait(&mutex_console);
    imprimir_timestamp();
    printf("Aeronave %3d [Prio:%4u] | Setor: S%-3d | Destino: S%-3d\n",
           aeronave->id, aeronave->prioridade, 
           aeronave->setor_atual, aeronave->setor_destino);
    sem_post(&mutex_console);
}

/**
 * Registra o tempo de espera de uma aeronave para acesso a um setor
 * @param aeronave: Ponteiro para a aeronave que está aguardando
 * @param inicio: Timestamp de quando a aeronave começou a aguardar
 */
void aeronave_registro_tempo_espera(aeronave_t *aeronave, struct timespec inicio) {
    if (aeronave == NULL || (inicio.tv_sec == 0 && inicio.tv_nsec == 0)) return;
    
    struct timespec fim;
    clock_gettime(CLOCK_REALTIME, &fim);
    
    if (aeronave->total_espera < aeronave->comprimento_rota) {
        double tempo_decorrido = (fim.tv_sec - inicio.tv_sec) + 
                                  (fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
        aeronave->tempo_espera[aeronave->total_espera] = tempo_decorrido;
        aeronave->total_espera++;
    }
}

/**
 * Calcula a média dos tempos de espera registrados por uma aeronave
 * @param aeronave: Ponteiro para a aeronave cujos tempos serão calculados
 * @return Média dos tempos de espera em segundos
 */
double aeronave_calcular_media_espera(aeronave_t *aeronave) {
    if (aeronave == NULL || aeronave->total_espera == 0) return 0.0;
    
    double soma = 0.0;
    int contagem_valida = 0;
    
    for (int i = 0; i < aeronave->total_espera; i++) {
        // Ignora tempos zerados ou negativos
        if (aeronave->tempo_espera[i] > 0.001) {
            soma += aeronave->tempo_espera[i];
            contagem_valida++;
        }
    }
    
    return (contagem_valida > 0) ? (soma / contagem_valida) : 0.0;
}

/**
 * Função principal de execução de uma aeronave (thread)
 * @param arg: Ponteiro para a estrutura aeronave_t que será executada
 * @return NULL ao finalizar a execução
 */
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
    
    // Percorre toda a rota
    for (int pos = 0; pos < a->comprimento_rota; pos++) {
        int setor_destino = a->rota[pos];
        
        // Pula se já está neste setor (setores duplicados consecutivos)
        if (setor_destino == a->setor_atual) {
            continue;
        }
        
        // Solicita acesso ao próximo setor
        int sucesso = atc_solicitar_setor(a, setor_destino);
        if (!sucesso) {
            sem_wait(&mutex_console);
            imprimir_timestamp();
            printf("Aeronave %3d Falha ao acessar S%d\n", a->id, setor_destino);
            sem_post(&mutex_console);
            break;
        }
        
        // Libera setor anterior (se houver)
        if (a->setor_atual >= 0) {
            atc_liberar_setor(a, a->setor_atual);
        }
        
        // Atualiza posição atual
        a->setor_atual = setor_destino;
        
        // Simula tempo de voo no setor (1-1.5 segundos)
        int tempo_voo_ms = 1000 + (rand() % 500);
        struct timespec ts = {
            .tv_sec = tempo_voo_ms / 1000,
            .tv_nsec = (tempo_voo_ms % 1000) * 1000000
        };
        
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %3d Voando em S%d por %d ms\n", a->id, setor_destino, tempo_voo_ms);
        sem_post(&mutex_console);
        
        nanosleep(&ts, NULL);
    }
    
    // Libera último setor ao concluir
    if (a->setor_atual >= 0) {
        atc_liberar_setor(a, a->setor_atual);
        a->setor_atual = -1;
    }
    
    sem_wait(&mutex_console);
    imprimir_timestamp();
    printf("Aeronave %3d Concluída! Tempo médio espera: %.2fs\n", a->id, aeronave_calcular_media_espera(a));
    sem_post(&mutex_console);
    
    pthread_exit(NULL);
}