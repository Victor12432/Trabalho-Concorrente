#include "../include/controlador.h"
#include "../include/fila_prioridade.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

bool fila_remover_aeronave(fila_prioridade_t *fila, aeronave_t *aeronave);
int total_setores;
int total_aeronaves;
int *setores_ocupados; //Array que guarda o ID da aeronave no setor(ou -1 se livre)
fila_prioridade_t *fila_setores; //Array de filas: fila de espera para cada setor
aeronave_t **aeronaves; //Array de ponteiros para todas as aeronaves
sem_t mutex_ctrl; //Mutex para proteger a logica do controlador
sem_t mutex_console; //Mutex para proteger a escrita na tela
pthread_t thread_controlador; //Thread do controlador central
int simulacao_ativa = 1; //Flag para parar loop controlador

//Função inicializacao
void atc_init(int setores, int n_aeronaves){
    total_setores = setores;
    total_aeronaves = n_aeronaves;
    
    //Alocação de memoria
    setores_ocupados = (int*)malloc(sizeof(int) * total_setores);
    fila_setores = (fila_prioridade_t *)malloc(sizeof(fila_prioridade_t)* total_setores);

    if (setores_ocupados == NULL || fila_setores == NULL) {
        fprintf(stderr, "ERRO: Falha na alocação de memória inicial\n");
        return;
    }

    sem_init(&mutex_ctrl, 0, 1);
    sem_init(&mutex_console, 0, 1);
    for(int i = 0; i < total_setores; i++){
        setores_ocupados[i] = -1;
        fila_inicializar(&fila_setores[i]);
    }
}
//Função finalização
void atc_finalizar(){
    simulacao_ativa = 0;

    for(int i = 0; i < total_setores; i++){
        fila_destruir(&fila_setores[i]);
    }
    
    free(setores_ocupados);
    free(fila_setores);

    sem_destroy(&mutex_ctrl);
    sem_destroy(&mutex_console);
}

int atc_solicitar_setor(aeronave_t *aeronave, int setor_destino){
    sem_wait(&mutex_ctrl);

    if(setor_destino < 0 || setor_destino >= total_setores){
        sem_post(&mutex_ctrl);
        return 0;
    }
    if (aeronave->setor_atual == setor_destino) {
        sem_post(&mutex_ctrl);
        return 1;
    }

    //A aeronave espera se o setor já tem alguém (e não é ela mesma)
    bool setor_ocupado = (setores_ocupados[setor_destino] != -1 && 
                          setores_ocupados[setor_destino] != aeronave->id);

    if (setor_ocupado) {
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %d (P:%d) aguardando setor %d (OCUPADO por %d)\n", 
               aeronave->id, aeronave->prioridade, setor_destino, setores_ocupados[setor_destino]);
        sem_post(&mutex_console);

        fila_inserir(&fila_setores[setor_destino], aeronave);
        time_t inicio = time(NULL);
        aeronave_registro_tempo_espera(aeronave, inicio);

        sem_post(&mutex_ctrl);
        
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 3;
        
        int wait_result = sem_timedwait(&aeronave->sem_aeronave, &timeout);
        
        if (wait_result == -1 && errno == ETIMEDOUT) {
            sem_wait(&mutex_console);
            imprimir_timestamp();
            printf("*** TIMEOUT! Aeronave %d (P:%d) recuando de S%d para evitar deadlock ***\n", 
                   aeronave->id, aeronave->prioridade, setor_destino);
            sem_post(&mutex_console);
            
            sem_wait(&mutex_ctrl);
            fila_remover_aeronave(&fila_setores[setor_destino], aeronave);
            sem_post(&mutex_ctrl);
            
            if (aeronave->setor_atual >= 0) {
                int setor_antigo = aeronave->setor_atual;
                aeronave->setor_atual = -1;
                atc_liberar_setor(aeronave, setor_antigo);
            }
            
            struct timespec pausa = {.tv_sec = 0, .tv_nsec = 500000000};
            nanosleep(&pausa, NULL);
            return atc_solicitar_setor(aeronave, setor_destino);
        }
        
        //Quando acordar, ela já assumiu o setor (foi processada no liberar_setor)
        return 1; 

    } else {
        // --- CAMINHO LIVRE ---
        //Ocupa o setor
        setores_ocupados[setor_destino] = aeronave->id;
        
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %d assumiu setor %d\n", aeronave->id, setor_destino);
        sem_post(&mutex_console);

        sem_post(&mutex_ctrl); //Sai da região crítica
        return 1;
    }
}
// Função interna que não pega mutex (assume que chamador já tem)
static void atc_liberar_setor_interno(aeronave_t *aeronave, int setor_liberado) {
    if (setor_liberado < 0 || setor_liberado >= total_setores) {
        return;
    }

    // Marcar setor livre
    setores_ocupados[setor_liberado] = -1;
    
    // Remove a próxima aeronave da fila (maior prioridade)
    aeronave_t *proxima_aeronave = fila_remover(&fila_setores[setor_liberado]);

    if (proxima_aeronave != NULL) {
        setores_ocupados[setor_liberado] = proxima_aeronave->id;
        sem_post(&proxima_aeronave->sem_aeronave);

        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Controle: Setor %d liberado por %d e repassado para %d\n", 
               setor_liberado, aeronave->id, proxima_aeronave->id);
        sem_post(&mutex_console);
    } else {
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %d liberou setor %d (Setor livre agora)\n", 
               aeronave->id, setor_liberado);
        sem_post(&mutex_console);
    }
}

void atc_liberar_setor(aeronave_t *aeronave, int setor_liberado) {
    sem_wait(&mutex_ctrl);
    atc_liberar_setor_interno(aeronave, setor_liberado);
    sem_post(&mutex_ctrl);
}

//-------Algumas funções auxiliares------

//Função verificar deadlock usando algoritmo do banqueiro
//retorna true se deadlock detectado
//retorna false se for seguro para prosseguir
bool verificar_deadlock(aeronave_t *solicitante, int setor_desejado) {
    int *simulacao_setores = (int *)malloc(total_setores * sizeof(int));
    bool *aeronave_concluiu = (bool *)calloc(total_aeronaves, sizeof(bool));

    if (simulacao_setores == NULL || aeronave_concluiu == NULL) {
        free(simulacao_setores);
        free(aeronave_concluiu);
        
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("ERRO: Falha na alocação de memória para verificação de deadlock\n");
        sem_post(&mutex_console);
        
        return true;
    }

    int aeronaves_restantes = 0;
    for (int i = 0; i < total_setores; i++) {
        simulacao_setores[i] = setores_ocupados[i];
    }

    // Identifica quais aeronaves estão voando (ativas) para simular apenas elas
    for (int i = 0; i < total_aeronaves; i++) {
        //Se a aeronave já terminou ou nem começou, marcamos como concluída para ignorar
        //Ajuste essa lógica conforme sua implementação de status da aeronave)
        //Aqui assumimos: se ela está em um setor ou é a solicitante, ela conta.
        bool esta_voando = false;
        
        //Verifica se a aeronave está em algum setor atualmente
        if (aeronaves[i]->setor_atual != -1) {
            esta_voando = true;
        }
        
        if (aeronaves[i]->id == solicitante->id) {
            esta_voando = true;
        }

        if (!esta_voando) {
            aeronave_concluiu[i] = true;
        } else {
            aeronaves_restantes++;
        }
    }

    simulacao_setores[setor_desejado] = solicitante->id;
    bool progresso = true;
    while (progresso && aeronaves_restantes > 0) {
        progresso = false;

        //Tenta encontrar uma aeronave que consiga se mover na simulação
        for (int i = 0; i < total_aeronaves; i++) {
            if (!aeronave_concluiu[i]) {
                aeronave_t *aero = aeronaves[i];
                
                int proximo_setor_necessario = -1;

                if (aero->id == solicitante->id) {
                     int idx_desejado = -1;
                     for(int r=0; r < aero->comprimento_rota; r++) {
                         if(aero->rota[r] == setor_desejado) {
                            idx_desejado = r;
                            break;
                         }
                     }
                     
                     if (idx_desejado == aero->comprimento_rota - 1) {
                         proximo_setor_necessario = -1;
                     } else if (idx_desejado != -1) {
                         proximo_setor_necessario = aero->rota[idx_desejado + 1];
                     }
                } else {
                    for (int r = 0; r < aero->comprimento_rota - 1; r++) {
                        if (aero->rota[r] == aero->setor_atual) {
                            proximo_setor_necessario = aero->rota[r + 1];
                            break;
                        }
                    }
                }

                bool pode_avancar = false;
                
                if (proximo_setor_necessario == -1) {
                    pode_avancar = true;
                } else if (simulacao_setores[proximo_setor_necessario] == -1) {
                    pode_avancar = true;
                }

                if (pode_avancar) {
                    for (int k = 0; k < total_setores; k++) {
                        if (simulacao_setores[k] == aero->id) {
                            simulacao_setores[k] = -1;
                        }
                    }
                    
                    aeronave_concluiu[i] = true;
                    aeronaves_restantes--;
                    progresso = true;
                }
            }
        }
    }

    free(simulacao_setores);
    free(aeronave_concluiu);

    if (aeronaves_restantes > 0) {
        return true;
    } else {
        return false;
    }
}

void imprimir_estado_setores(){
    sem_wait(&mutex_console);
    printf("ESTADO DOS SETORES:\n");
    for(int i = 0; i < total_setores; i++){
        if(setores_ocupados[i] == -1){
            printf("Setor %d: LIVRE\n", i);
        }
        else{
            printf("Setor %d: OCUPADO por Aeronave %d\n", i, setores_ocupados[i]);
        }
    }
    sem_post(&mutex_console);
}

void *controlador_central_executar(void *arg){
    while(simulacao_ativa){
        imprimir_estado_setores();
        sleep(3);
    }
    return NULL;
}

void liberar_setor_emergencia(aeronave_t *aeronave) {
    sem_wait(&mutex_ctrl);
    
    int setor_encontrado = -1;
    for (int i = 0; i < total_setores; i++) {
        if (setores_ocupados[i] == aeronave->id) {
            setor_encontrado = i;
            break;
        }
    }
    
    if (setor_encontrado != -1) {
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("!!! EMERGÊNCIA !!! Aeronave %d (P:%d) liberando forçadamente setor %d\n", 
               aeronave->id, aeronave->prioridade, setor_encontrado);
        sem_post(&mutex_console);
        
        atc_liberar_setor_interno(aeronave, setor_encontrado);
    } else {
        sem_wait(&mutex_console);
        printf("Erro: Aeronave %d tentou liberação de emergência mas não ocupa setores.\n", 
               aeronave->id);
        sem_post(&mutex_console);
    }
    
    sem_post(&mutex_ctrl);
}

void imprimir_fila_espera(){
    sem_wait(&mutex_ctrl);
    sem_wait(&mutex_console);

    int filas_vazias = 1;
    printf("-----FILAS DE ESPERA POR SETOR:-----\n");
    for(int i = 0; i < total_setores; i++){
        if(!fila_vazio(&fila_setores[i])){
            printf("Setor %02d: ", i);
            fila_imprimir(&fila_setores[i]);

            filas_vazias = 0;
        }
    }
    if(filas_vazias){
        printf("Nenhuma aeronave aguardando em fila de espera\n");
    }
    
    sem_post(&mutex_console);
    sem_post(&mutex_ctrl);

}
