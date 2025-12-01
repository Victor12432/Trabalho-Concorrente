//arquivo controlador
#include "../include/controlador.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
    fila_setores = (fila_prioridade_t *)malloc(sizeof(fila_prioridade_t)* total_setores);

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

    //A aeronave espera se:
    //O setor já tem alguém (setores_ocupados[...] != -1)
    //   OU (||)
    //O setor está livre, MAS entrar nele cria um Deadlock (verificar_deadlock returns true)
    bool setor_ocupado = (setores_ocupados[setor_destino] != -1);
    bool vai_travar = verificar_deadlock(aeronave, setor_destino);

    if (setor_ocupado || vai_travar) {
        // --- LOG DE ESPERA ---
        sem_wait(&mutex_console);
        timestamp_print();
        if (setor_ocupado) {
            printf("Aeronave %d (P:%d) aguardando setor %d (OCUPADO por %d)\n", 
                   aeronave->id, aeronave->prioridade, setor_destino, setores_ocupados[setor_destino]);
        } else {
            printf("Aeronave %d (P:%d) aguardando setor %d (BLOQUEIO PREVENTIVO DE DEADLOCK)\n", 
                   aeronave->id, aeronave->prioridade, setor_destino);
        }
        sem_post(&mutex_console);

        //Insere na fila de prioridade
        fila_inserir(&fila_setores[setor_destino], aeronave);
        aeronave_registro_tempo_espera(aeronave, time(NULL));

        sem_post(&mutex_ctrl); //Sai da região crítica antes de dormir
        
        //AERONAVE DORME AQUI
        sem_wait(&aeronave->sem_aeronave); 
        
        //Quando acordar, ela já assumiu o setor (foi processada no liberar_setor)
        return 1; 

    } else {
        // --- CAMINHO LIVRE ---
        //Ocupa o setor
        setores_ocupados[setor_destino] = aeronave->id;
        
        sem_wait(&mutex_console);
        timestamp_print();
        printf("Aeronave %d assumiu setor %d\n", aeronave->id, setor_destino);
        sem_post(&mutex_console);

        sem_post(&mutex_ctrl); //Sai da região crítica
        return 1;
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
    //Tenta encontrar uma aeronave na fila que possa assumir o setor sem causar deadlock
    aeronave_t *proxima_aeronave = NULL;
    int total_na_fila = fila_setores[setor_liberado].tamanho;
    for (int i = 0; i < total_na_fila; i++) {
        //Pega a de maior prioridade sem remover ainda
        aeronave_t *candidata = fila_espiar(&fila_setores[setor_liberado]);
        
        //Verifica se é seguro para a candidata entrar
        if (!verificar_deadlock(candidata, setor_liberado)) {
            proxima_aeronave = fila_remover(&fila_setores[setor_liberado]); // Agora sim, remove
            break; // Encontrou uma aeronave segura
        } else {
            //Se não for seguro, move para o fim da fila para testar a próxima
            fila_rotacionar(&fila_setores[setor_liberado]);
        }
    }

    if (proxima_aeronave != NULL) {
        //Marca o setor para a aeronave
        setores_ocupados[setor_liberado] = proxima_aeronave->id;
        //Acorda aeronave que estava dormindo la no solicitar_setor
        sem_post(&proxima_aeronave->sem_aeronave);

        sem_wait(&mutex_console);
        timestamp_print();
        printf("Controle: Setor %d liberado por %d e repassado para %d (da fila)\n", setor_liberado, aeronave->id, proxima_aeronave->id);
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
//retorna true se deadlock detectado
//retorna false se for seguro para prosseguir
bool verificar_deadlock(aeronave_t *solicitante, int setor_desejado) {
    //Alocações temporárias para a simulação (Vetores de Estado)
    int *simulacao_setores = (int *)malloc(total_setores * sizeof(int));
    bool *aeronave_concluiu = (bool *)calloc(total_aeronaves, sizeof(bool));
    int aeronaves_restantes = 0;

    //Copia o estado real para o estado simulado (Snapshot)
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
            aeronave_concluiu[i] = true; //Ignora na simulação
        } else {
            aeronaves_restantes++;
        }
    }

    //APLICANDO O CENÁRIO (O "E se?"):
    //Simula a aeronave entrando no setor desejado
    simulacao_setores[setor_desejado] = solicitante->id;
    //Não liberamos o setor atual dela ainda, pois no momento da transição
    //ela tecnicamente ocupa (ou bloqueia) ambos até a transição completar.
    //Se sua lógica libera o anterior antes, mude aqui. Mas o mais seguro é reter ambos.

    //Executa o Algoritmo de Segurança (Safety Algorithm)
    bool progresso = true;
    while (progresso && aeronaves_restantes > 0) {
        progresso = false;

        //Tenta encontrar uma aeronave que consiga se mover na simulação
        for (int i = 0; i < total_aeronaves; i++) {
            if (!aeronave_concluiu[i]) {
                aeronave_t *aero = aeronaves[i];
                
                int proximo_setor_necessario = -1;

                //Caso especial: para o solicitante, o próximo setor necessário é o que vem DEPOIS do setor desejado.
                if (aero->id == solicitante->id) {
                    //O solicitante na simulação 'está' no setor_desejado.
                    //O próximo passo dele seria o seguinte na rota.
                    //Se o setor desejado for o último, ele conclui.
                     int idx_desejado = -1;
                     for(int r=0; r < aero->comprimento_rota; r++) {
                         if(aero->rota[r] == setor_desejado) {
                            idx_desejado = r;
                            break;
                         }
                     }
                     
                     if (idx_desejado == aero->comprimento_rota - 1) {
                         proximo_setor_necessario = -1; //Vai sair do sistema
                     } else if (idx_desejado != -1) {
                         proximo_setor_necessario = aero->rota[idx_desejado + 1];
                     }
                } else {
                    //Para as outras aeronaves, o próximo setor é o que vem depois do setor atual delas.
                    //Se a aeronave estiver no seu último setor, o loop não encontrará um próximo,
                    //e proximo_setor_necessario continuará -1, indicando que ela pode sair.
                    for (int r = 0; r < aero->comprimento_rota - 1; r++) {
                        if (aero->rota[r] == aero->setor_atual) {
                            proximo_setor_necessario = aero->rota[r + 1];
                            break;
                        }
                    }
                }

                //Verifica se o recurso necessário está livre (ou se a aeronave vai sair)
                bool pode_avancar = false;
                
                if (proximo_setor_necessario == -1) {
                    //Vai sair do sistema (sucesso)
                    pode_avancar = true;
                } else if (simulacao_setores[proximo_setor_necessario] == -1) {
                    //Setor necessário está livre
                    pode_avancar = true;
                }

                //Se pode avançar, simulamos que ela libera seus recursos e termina
                if (pode_avancar) {
                    //Libera os recursos que ela segurava na simulação
                    for (int k = 0; k < total_setores; k++) {
                        if (simulacao_setores[k] == aero->id) {
                            simulacao_setores[k] = -1;
                        }
                    }
                    
                    aeronave_concluiu[i] = true;
                    aeronaves_restantes--;
                    progresso = true; //Conseguimos avançar, continuamos o loop
                }
            }
        }
    }

    //Limpeza e Resultado
    free(simulacao_setores);
    free(aeronave_concluiu);

    //Se ainda restam aeronaves que não conseguiram concluir, temos um DEADLOCK.
    if (aeronaves_restantes > 0) {
        return true; //Deadlock detectado
    } else {
        return false; //Pode autorizar
    }
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
    sem_wait(&mutex_ctrl);

    //descobrir em qual setor a aeronave esta travada
    int setor_encontrado = -1;

    for(int i = 0; i < total_setores; i++){
        if(setores_ocupados[i] == aeronave->id){
            setor_encontrado = i;
            break;
        }
    }
    sem_post(&mutex_ctrl);// Libera para poder chamar a função abaixo (que pega o mutex de novo)

    if(setor_encontrado != -1){
        //log para ação critica
        sem_wait(&mutex_console);
        timestamp_print();
        printf("!!! EMERGÊNCIA !!! Aeronave %d (P:%d) liberando forçosamente o setor %d\n", 
               aeronave->id, aeronave->prioridade, setor_encontrado);
        sem_post(&mutex_console);

        //reutiliza o codigo padrao para acordar o proximo da fila
        atc_liberar_setor(aeronave, setor_encontrado);
    }
    else{
        //so pra debugar mesmo, tentou liberar mas nao estava em lugar nenhum
        sem_wait(&mutex_console);
        printf("Erro: Aeronave %d tentou liberação de emergência mas não ocupa setores.\n", aeronave->id);
        sem_post(&mutex_console);
    }
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
