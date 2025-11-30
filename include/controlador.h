#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "fila_prioridade.h"
#include "aeronave.h"

extern int total_setores;
extern int total_aeronaves;
extern int *setores_ocupados;
extern fila_prioridade_t *fila_setores;
extern aeronave_t **aeronaves;
extern sem_t mutex_ctrl;
extern sem_t mutex_console;
extern pthread_t thread_controlador;


void atc_init(int setores, int aeronaves);
void atc_finalizar();
int atc_solicitar_setor(aeronave_t *aeronave, int setor_destino);
void atc_liberar_setor(aeronave_t *aeronave, int setor_liberado);
void *controlador_central_executar(void *arg);
void liberar_setor_emergencia(aeronave_t *aeronave);
void controlador_processar_solicitacao();
bool verificar_deadlock(aeronave_t *aeronave);
void imprimir_estado_setores();
void imprimir_fila_espera();

#endif // CONTROLADOR_H