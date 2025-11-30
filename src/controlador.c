//arquivo controlador
#include "../include/controlador.h"
#include <stdio.h>
#include <stdlib.h>
//variaveis extern implementadas no .h
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
    fila_setores = (fila_prioridade_t*)malloc(sizeof(fila_prioridade_t)* total_setores);

    //Inicializa semaforos
    sem_init(&mutex_ctrl, 0, 1); //Binario
    sem_init(&mutex_console, 0, 1); //Binario

    //Inicializa setores como livres(-1) e cria filas
    for(int i = 0; i < total_setores; i++){
        setores_ocupados[i] = -1;
        fila_inicializar(&fila_setores[i]);
    }
}
//Função finalização
void atc_finalizar(){
    simulacao_ativa = 0; //Sinaliza para parar o loop do controlador

    //Libera filas
    for(int i = 0; i < total_setores; i++){
        fila_destruir(&fila_setores[i]);
    }
    //Liberar memoria alocada
    free(setores_ocupados);
    free(fila_setores);

    //Destruir semaforos
    sem_destroy(&mutex_ctrl);
    sem_destroy(&mutex_console);

}

//Função solicitar setor
int atc_solicitar_setor(aeronave_t *aeronave, int setor_destino){
    sem_wait(&mutex_ctrl); //Entra na regiao critica

    //Verifica se o setor e valido
    if(setor_destino < 0 || setor_destino >= total_setores){//se aeronave completou sua rota ou maior igual que o total
        sem_post(&mutex_ctrl); //Sai da regiao critica
        return 0; //Erro
    }

    //Verificacao de seguranca e disponibilidade
    //Se setor ocupado, aeronave espera
    if(setores_ocupados[setor_destino] != -1){
        //Log de espera
        sem_wait(&mutex_console);
        timestamp_print();
        printf("Aeronave %d (Prioridade: %d) aguardando setor %d (ocupado por aeronave %d)\n", aeronave->id, aeronave->prioridade, setor_destino, setores_ocupados[setor_destino]);
        sem_post(&mutex_console);

        //Insere na fila de prioridade 
        fila_inserir(&fila_setores[setor_destino], aeronave);

        //Registra inicio de espera para estatisticas
        aeronave_registro_tempo_espera(aeronave, time(NULL));
        
        sem_post(&mutex_ctrl); //Sai da regiao critica do controlador antes de dormir

        sem_wait(&aeronave->sem_aeronave); //bloqueia a aeronave ate que o controlador a acorde
        return 1;//Quando acordar, ela ganha o setor no 'atc_liberar_setor'

    }
    else{
        //Se estiver livre, ja ocupa setor
        setores_ocupados[setor_destino] = aeronave->id;

        sem_wait(&mutex_console);
        timestamp_print();
        printf("Aeronave %d assumiu setor %d\n", aeronave->id, setor_destino);
        sem_post(&mutex_console);

        sem_post(&mutex_ctrl); //Sai da regiao critica
        return 1;//Deu certo
    }
}

//Função Liberar setor
void atc_liberar_setor(aeronave_t *aeronave, int setor_liberado){
    sem_wait(&mutex_ctrl); //Entra na regiao critica
    
    if(setor_liberado < 0 || setor_liberado >= total_setores){
        sem_post(&mutex_ctrl); //Sai da regiao critica
        return; //Erro
    }

    //Marcar setor livre por enquanto
    setores_ocupados[setor_liberado] = -1;
    
    //verificar se tem alguem na fila de espera deste setor
    if(!fila_vazio(&fila_setores[setor_liberado])){
        //Remove a aeronave de maior prioridade da fila
        aeronave_t *proxima_aeronave = fila_remover(&fila_setores[setor_liberado]);

        //Marca o setor para a aeronave
        setores_ocupados[setor_liberado] = proxima_aeronave->id;

        //Acorda aeronave que estava dormindo la no solicitar_setor
        sem_post(&proxima_aeronave->sem_aeronave);

        sem_wait(&mutex_console);
        timestamp_print();
        printf("Controle: Setor %d liberado por %d e repassado para %d (PRIORIDADE)\n", setor_liberado, aeronave->id, proxima_aeronave->id);
        sem_post(&mutex_console);
    }
    else{
        //se fila vazia, apenas libera o setor
        //ninguem esperando
        sem_wait(&mutex_console);
        timestamp_print();
        printf("Aeronave %d liberou setor %d(Setor livre agora)\n", aeronave->id, setor_liberado);
        sem_post(&mutex_console);
    }
    sem_post(&mutex_ctrl); //Sai da regiao critica
}

//-------Algumas funções auxiliares------

//Função verificar deadlock usando algoritmo do banqueiro
bool verificar_deadlock(aeronave_t *aeronave){
    //terminar
    return;
}

//Função imprimir estado dos setores
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

//Thread que roda em paralelo so pra mostrar o status(Radar)
void *controlador_central_executar(void *arg){
    while(simulacao_ativa){
        imprimir_estado_setores();
        sleep(3);//Atualiza o radar a cada 3 segundos
        //aq pode ter logica de deadlock
    }
    return NULL;
}

//
void controlador_processar_solicitacao(){
    //nao usado
}

void liberar_setor_emergencia(aeronave_t *aeronave){
    //nao usado tambem, so se tiver alguma logica emergencial 
}

void imprimir_fila_espera(){
    //Bloquear controlador para garantir que a fila nao seja modificada
    sem_wait(&mutex_ctrl);

    //Bloquear console para impressao
    sem_wait(&mutex_console);

    int filas_vazias = 1;
    printf("-----FILAS DE ESPERA POR SETOR:-----\n");
    for(int i = 0; i < total_setores; i++){
        if(!fila_vazio(&fila_setores[i])){
            printf("Setor %02d: ", i);
            //chamar funcao interna da fila para imprimir os nós
            fila_imprimir(&fila_setores[i]);

            filas_vazias = 0;
        }
    }
    if(filas_vazias){
        printf("Nenhuma aeronave aguardando em fila de espera\n");
    }
    //Liberar os semaforos na ordem inversa
    sem_post(&mutex_console);
    sem_post(&mutex_ctrl);

}
