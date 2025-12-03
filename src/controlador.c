#include "../include/controlador.h"
#include "../include/fila_prioridade.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

// Constantes para prevenção de starvation
#define MAX_RECUOS_CONSECUTIVOS 2    // Após 2 recuos, ganha boost
#define BOOST_PRIORIDADE 700         // Valor adicionado à prioridade
#define TEMPO_ESPERA_LONGO 3.0       // 3 segundos é considerado espera longa

int total_setores;
int total_aeronaves;
int *setores_ocupados; //Array que guarda o ID da aeronave no setor(ou -1 se livre)
fila_prioridade_t *fila_setores; //Array de filas: fila de espera para cada setor
aeronave_t **aeronaves; //Array de ponteiros para todas as aeronaves
sem_t mutex_ctrl; //Mutex para proteger a logica do controlador
sem_t mutex_console; //Mutex para proteger a escrita na tela
pthread_t thread_controlador; //Thread do controlador central
int simulacao_ativa = 1; //Flag para parar loop controlador

// Estatísticas da execução
static int total_deadlocks_detectados = 0;
static int total_recuos_forcados = 0;
static int total_boosts_aplicados = 0;
static struct timespec tempo_inicio_simulacao;

/**
 * Inicializa o sistema de controle de tráfego aéreo
 * @param setores: Número total de setores no espaço aéreo
 * @param n_aeronaves: Número total de aeronaves que participarão da simulação
 */
void atc_init(int setores, int n_aeronaves){
    total_setores = setores;
    total_aeronaves = n_aeronaves;
    
    // Marca início da simulação
    clock_gettime(CLOCK_REALTIME, &tempo_inicio_simulacao);
    
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

/**
 * Finaliza o sistema de controle de tráfego aéreo e exibe estatísticas da execução
 */
void atc_finalizar(){
    simulacao_ativa = 0;
    
    // Calcula tempo total de execução
    struct timespec tempo_fim;
    clock_gettime(CLOCK_REALTIME, &tempo_fim);
    double tempo_total = (tempo_fim.tv_sec - tempo_inicio_simulacao.tv_sec) + 
                         (tempo_fim.tv_nsec - tempo_inicio_simulacao.tv_nsec) / 1e9;
    
    // Exibe estatísticas da execução
    printf("\n[ATC] ========== ESTATÍSTICAS DA EXECUÇÃO ==========\n");
    printf("[ATC] Tempo total de simulação: %.2f segundos\n", tempo_total);
    printf("[ATC] Total de deadlocks detectados: %d\n", total_deadlocks_detectados);
    printf("[ATC] Total de recuos forçados: %d\n", total_recuos_forcados);
    printf("[ATC] Total de boosts aplicados: %d\n", total_boosts_aplicados);
    printf("[ATC] Taxa de contenção: %.2f deadlocks/segundo\n", 
           tempo_total > 0 ? total_deadlocks_detectados / tempo_total : 0);
    printf("[ATC] ================================================\n\n");

    for(int i = 0; i < total_setores; i++){
        fila_destruir(&fila_setores[i]);
    }
    
    free(setores_ocupados);
    free(fila_setores);

    sem_destroy(&mutex_ctrl);
    sem_destroy(&mutex_console);
}

/**
 * Solicita acesso a um setor específico para uma aeronave
 * @param aeronave: Ponteiro para a aeronave que está solicitando o setor
 * @param setor_desejado: Índice do setor que a aeronave deseja acessar
 * @return 1 se o setor foi obtido com sucesso, 0 se ocorreu um erro
 */
int atc_solicitar_setor(aeronave_t *aeronave, int setor_desejado) {
    sem_wait(&mutex_ctrl);

    if(setor_desejado < 0 || setor_desejado >= total_setores){
        sem_post(&mutex_ctrl);
        return 0;
    }
    if (aeronave->setor_atual == setor_desejado) {
        sem_post(&mutex_ctrl);
        return 1;
    }

    // A aeronave espera se o setor já tem alguém (e não é ela mesma)
    bool setor_ocupado = (setores_ocupados[setor_desejado] != -1 && 
                          setores_ocupados[setor_desejado] != aeronave->id);
    bool vai_travar = verificar_deadlock(aeronave, setor_desejado);
    
    if (setor_ocupado || vai_travar) {
        sem_wait(&mutex_console);
        imprimir_timestamp();
        if (setor_ocupado && !vai_travar) {
            printf("Aeronave %d (P:%d) aguardando setor %d (OCUPADO por %d)\n", 
                   aeronave->id, aeronave->prioridade, setor_desejado, setores_ocupados[setor_desejado]);
        } else if (vai_travar) {
            printf("Aeronave %d (P:%d) BLOQUEADO em S%d - liberando setor atual S%d para evitar deadlock\n", 
                   aeronave->id, aeronave->prioridade, setor_desejado, aeronave->setor_atual);
        }
        sem_post(&mutex_console);

        // Se for bloqueio de deadlock, libera setor atual e aguarda um tempo
        if (vai_travar) {
            int setor_liberar = aeronave->setor_atual;
            aeronave->setor_atual = -1;
            sem_post(&mutex_ctrl);
            
            if (setor_liberar >= 0) {
                atc_liberar_setor(aeronave, setor_liberar);
            }
            
            // Aguarda um pouco antes de tentar novamente
            struct timespec pausa = {.tv_sec = 0, .tv_nsec = 100000000}; // 100ms
            nanosleep(&pausa, NULL);
            
            // Tenta novamente
            return atc_solicitar_setor(aeronave, setor_desejado);
        }

        // Entra na fila
        fila_inserir(&fila_setores[setor_desejado], aeronave);
        
        // Captura início da espera com alta precisão
        struct timespec inicio;
        clock_gettime(CLOCK_REALTIME, &inicio);
        
        sem_post(&mutex_ctrl);
        
        // Aguarda sem timeout - mantém prioridade na fila
        sem_wait(&aeronave->sem_aeronave);
        
        // Verifica se foi acordado para RECUAR (deadlock)
        sem_wait(&mutex_ctrl);
        if (aeronave->precisa_recuar) {
            aeronave->precisa_recuar = false;
            aeronave->contador_recuos++;
            
            // Anti-starvation: após muitos recuos, aumenta prioridade temporariamente
            if (aeronave->contador_recuos >= MAX_RECUOS_CONSECUTIVOS && 
                aeronave->prioridade == aeronave->prioridade_original) {
                aeronave->prioridade = aeronave->prioridade_original + BOOST_PRIORIDADE;
                total_boosts_aplicados++;
                sem_wait(&mutex_console);
                imprimir_timestamp();
                printf(">>> A%d (P:%u) recebeu BOOST de prioridade -> %u (após %d recuos) <<<\n", 
                       aeronave->id, aeronave->prioridade_original, 
                       aeronave->prioridade, aeronave->contador_recuos);
                sem_post(&mutex_console);
            }
            
            sem_post(&mutex_ctrl);
            
            total_recuos_forcados++;
            sem_wait(&mutex_console);
            imprimir_timestamp();
            printf("*** A%d recuando de S%d devido a deadlock (recuo #%d) ***\n", 
                   aeronave->id, aeronave->setor_atual, aeronave->contador_recuos);
            sem_post(&mutex_console);
            
            // Volta ao início da função para tentar novamente
            return atc_solicitar_setor(aeronave, setor_desejado);
        }
        sem_post(&mutex_ctrl);
        
        // Registra tempo de espera após receber acesso
        aeronave_registro_tempo_espera(aeronave, inicio);
        
        // Verifica se foi uma espera longa e aplica boost se necessário
        struct timespec fim;
        clock_gettime(CLOCK_REALTIME, &fim);
        double tempo_esperado = (fim.tv_sec - inicio.tv_sec) + 
                               (fim.tv_nsec - inicio.tv_nsec) / 1000000000.0;
        
        sem_wait(&mutex_ctrl);
        if (tempo_esperado > TEMPO_ESPERA_LONGO) {
            aeronave->contador_esperas_longas++;
            
            // Boost após esperas longas
            if (aeronave->contador_esperas_longas >= 2 && 
                aeronave->prioridade == aeronave->prioridade_original) {
                aeronave->prioridade = aeronave->prioridade_original + BOOST_PRIORIDADE;
                total_boosts_aplicados++;
                sem_wait(&mutex_console);
                imprimir_timestamp();
                printf(">>> A%d (P:%u) recebeu BOOST -> %u (esperas longas: %.1fs) <<<\n", 
                       aeronave->id, aeronave->prioridade_original, 
                       aeronave->prioridade, tempo_esperado);
                sem_post(&mutex_console);
            }
        }
        
        // Reseta contadores após sucesso (conseguiu o setor)
        aeronave->contador_recuos = 0;
        sem_post(&mutex_ctrl);
        
        return 1;
        
    } else {
        // --- CAMINHO LIVRE ---
        // Ocupa o setor imediatamente
        setores_ocupados[setor_desejado] = aeronave->id;
        
        sem_wait(&mutex_console);
        imprimir_timestamp();
        printf("Aeronave %d assumiu setor %d\n", aeronave->id, setor_desejado);
        sem_post(&mutex_console);

        sem_post(&mutex_ctrl);
        return 1;
    }
}

/**
 * Libera internamente um setor (função auxiliar chamada por outras funções)
 * @param aeronave: Ponteiro para a aeronave que está liberando o setor
 * @param setor_liberado: Índice do setor que está sendo liberado
 */
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

/**
 * Libera um setor ocupado por uma aeronave e passa o controle para a próxima aeronave na fila
 * @param aeronave: Ponteiro para a aeronave que está liberando o setor
 * @param setor_liberado: Índice do setor que está sendo liberado
 */
void atc_liberar_setor(aeronave_t *aeronave, int setor_liberado) {
    sem_wait(&mutex_ctrl);
    atc_liberar_setor_interno(aeronave, setor_liberado);
    sem_post(&mutex_ctrl);
}

//-------Algumas funções auxiliares------

/**
 * Verifica se a concessão de um setor causaria deadlock usando detecção de ciclos
 * @param solicitante: Ponteiro para a aeronave que está solicitando o setor
 * @param setor_desejado: Índice do setor que está sendo solicitado
 * @return true se deadlock for detectado, false se for seguro prosseguir
 */
bool verificar_deadlock(aeronave_t *solicitante, int setor_desejado) {
    // Detecta ciclos de espera usando busca em profundidade
    // Segue a cadeia: solicitante -> ocupante -> ocupante2 -> ... até encontrar ciclo ou fim
    
    if (setores_ocupados[setor_desejado] == -1) {
        return false; // Setor livre, sem deadlock
    }
    
    int ocupante_id = setores_ocupados[setor_desejado];
    if (ocupante_id == solicitante->id) {
        return false; // A própria aeronave já ocupa
    }
    
    // Verificar se solicitante está segurando algum setor
    if (solicitante->setor_atual < 0) {
        return false; // Solicitante não segura recursos, não pode causar deadlock
    }
    
    // Busca por ciclo: segue a cadeia de dependências
    int visitados[total_aeronaves];
    for (int i = 0; i < total_aeronaves; i++) visitados[i] = 0;
    
    aeronave_t *menor_prioridade = solicitante;
    unsigned int min_prioridade = solicitante->prioridade;
    
    int atual_id = ocupante_id;
    visitados[solicitante->id] = 1;
    
    // Segue a cadeia de espera
        while (atual_id != -1) {
        if (atual_id == solicitante->id) {
            // CICLO ENCONTRADO!
            total_deadlocks_detectados++;
            sem_wait(&mutex_console);
            imprimir_timestamp();
            printf("!! DEADLOCK em ciclo: A%d(P:%u) -> ... -> A%d !!\n",
                   solicitante->id, solicitante->prioridade, solicitante->id);
            
            // Sempre bloqueia o SOLICITANTE se ele está no ciclo
            // Ele que está tentando entrar e causando o problema
            // Usa prioridade EFETIVA (pode ter boost anti-starvation)
            if (solicitante->prioridade <= min_prioridade) {
                printf("   -> A%d (P:%u) bloqueado - menor/igual prioridade no ciclo\n", 
                       solicitante->id, solicitante->prioridade);
                sem_post(&mutex_console);
                return true; // Bloqueia o solicitante
            } else {
                char boost_info[100] = "";
                if (solicitante->prioridade > solicitante->prioridade_original) {
                    snprintf(boost_info, sizeof(boost_info), " [BOOST: %u->%u]", 
                            solicitante->prioridade_original, solicitante->prioridade);
                }
                printf("   -> A%d tem alta prioridade%s, forçando recuo de A%d (P:%u)\n",
                       solicitante->id, boost_info, menor_prioridade->id, menor_prioridade->prioridade);
                sem_post(&mutex_console);
                
                // Força a de menor prioridade a recuar
                if (menor_prioridade->id != solicitante->id) {
                    menor_prioridade->precisa_recuar = true;
                    // Procura em qual fila está esperando e remove
                    for (int s = 0; s < total_setores; s++) {
                        if (fila_remover_aeronave(&fila_setores[s], menor_prioridade)) {
                            sem_post(&menor_prioridade->sem_aeronave);
                            break;
                        }
                    }
                }
                return false; // Permite solicitante continuar
            }
        }        if (visitados[atual_id]) {
            break; // Já visitado, mas não forma ciclo com solicitante
        }
        visitados[atual_id] = 1;
        
        // Busca a aeronave atual
        aeronave_t *aero_atual = NULL;
        for (int i = 0; i < total_aeronaves; i++) {
            if (aeronaves[i] != NULL && aeronaves[i]->id == atual_id) {
                aero_atual = aeronaves[i];
                break;
            }
        }
        
        if (aero_atual == NULL) break;
        
        // Atualiza menor prioridade no ciclo
        if (aero_atual->prioridade < min_prioridade) {
            min_prioridade = aero_atual->prioridade;
            menor_prioridade = aero_atual;
        }
        
        // Procura qual setor essa aeronave está esperando
        int proximo_setor = -1;
        for (int s = 0; s < total_setores; s++) {
            if (fila_vazio(&fila_setores[s])) continue;
            
            no_fila_t *no = fila_setores[s].inicio;
            while (no != NULL) {
                if (no->aeronave->id == atual_id) {
                    proximo_setor = s;
                    break;
                }
                no = no->proximo;
            }
            if (proximo_setor >= 0) break;
        }
        
        if (proximo_setor < 0) break; // Não está esperando nada
        
        // Quem ocupa o próximo setor?
        atual_id = setores_ocupados[proximo_setor];
    }
    
    return false; // Sem deadlock detectado
}

/**
 * Imprime o estado atual de ocupação de todos os setores do espaço aéreo
 */
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

/**
 * Função principal do controlador central que monitora e exibe o estado do sistema
 * @param arg: Argumento genérico (não utilizado)
 * @return NULL ao finalizar a execução
 */
void *controlador_central_executar(void *arg){
    while(simulacao_ativa){
        imprimir_estado_setores();
        sleep(3);
    }
    return NULL;
}

/**
 * Libera forçadamente todos os setores ocupados por uma aeronave em situação de emergência
 * @param aeronave: Ponteiro para a aeronave em situação de emergência
 */
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

/**
 * Imprime as filas de espera de todos os setores que possuem aeronaves aguardando acesso
 */
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
