#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include "include/controlador.h"
#include "include/aeronave.h"
#include "include/utils.h"

/* ---------- Variáveis Globais ---------- */
extern aeronave_t **Aeronaves;

/* ---------- Manipulador de sinais ---------- */
void trata_sinal(int sinal) {
    printf("\n\n[SISTEMA] Recebido sinal %d - Finalizando graciosamente...\n", sinal);
    
    // Para threads de aeronaves
    for (int i = 0; i < total_aeronaves && aeronaves != NULL; i++) {
        if (aeronaves[i] != NULL) {
            pthread_cancel(aeronaves[i]->thread);
        }
    }
    
    // Finaliza sistema ATC
    atc_finalizar();
    
    // Libera memória das aeronaves
    if (aeronaves != NULL) {
        for (int i = 0; i < total_aeronaves; i++) {
            if (aeronaves[i] != NULL) {
                aeronave_destruir(aeronaves[i]);
            }
        }
        free(aeronaves);
    }
    
    printf("[SISTEMA] Finalizado com sucesso.\n");
    exit(0);
}

/* ---------- Função Principal ---------- */
int main(int argc, char *argv[]) {
    // Configurar tratamento de sinais
    signal(SIGINT, trata_sinal);
    signal(SIGTERM, trata_sinal);
    
    // Verificar argumentos
    if (argc != 3) {
        printf("Uso: %s [NUM_SETORES] [NUM_AERONAVES]\n", argv[0]);
        printf("Exemplo: %s 5 8\n", argv[0]);
        return 1;
    }
    
    int num_setores = atoi(argv[1]);
    int num_aeronaves = atoi(argv[2]);
    
    // Validação de parâmetros
    if (num_setores <= 0 || num_aeronaves <= 0) {
        printf("Erro: Os números devem ser positivos!\n");
        return 1;
    }
    
    if (num_setores < 2) {
        printf("Aviso: Mínimo de 2 setores. Ajustando para 2...\n");
        num_setores = 2;
    }
    
    // Seed para números aleatórios
    srand(time(NULL));
    
    printf("\n");
    printf("===============================================\n");
    printf("  SIMULADOR DE CONTROLE DE TRÁFEGO AÉREO (ATC)\n");
    printf("===============================================\n");
    printf("Setores: %d | Aeronaves: %d\n", num_setores, num_aeronaves);
    printf("Prioridade: 1-%d (maior = mais prioritário)\n", PRIORIDADE_MAX);
    printf("Pressione Ctrl+C para encerrar\n");
    printf("===============================================\n\n");
    
    // Inicializar sistema ATC
    printf("[MAIN] Inicializando sistema ATC...\n");
    atc_init(num_setores, num_aeronaves);
    
    // Alocar array de aeronaves
    aeronaves = malloc(num_aeronaves * sizeof(aeronave_t*));
    if (aeronaves == NULL) {
        perror("Erro ao alocar array de aeronaves");
        atc_finalizar();
        return 1;
    }
    
    // Inicializar array com NULLs
    for (int i = 0; i < num_aeronaves; i++) {
        aeronaves[i] = NULL;
    }
    
    // Criar aeronaves
    printf("[MAIN] Criando %d aeronaves...\n", num_aeronaves);
    for (int i = 0; i < num_aeronaves; i++) {
        aeronaves[i] = aeronave_criar(i, num_setores);
        if (aeronaves[i] == NULL) {
            fprintf(stderr, "Erro ao criar aeronave %d\n", i);
            trata_sinal(SIGTERM);
            return 1;
        }
    }
    
    // Iniciar threads das aeronaves
    printf("[MAIN] Iniciando voos...\n");
    for (int i = 0; i < num_aeronaves; i++) {
        if (pthread_create(&aeronaves[i]->thread, NULL, aeronave_executa, aeronaves[i]) != 0) {
            perror("Erro ao criar thread da aeronave");
            aeronave_destruir(aeronaves[i]);
            aeronaves[i] = NULL;
        }
        // Pequena pausa entre criação de threads
    }
    
    printf("\n[MAIN] Todas as aeronaves iniciadas. Sistema operacional.\n");
    printf("[MAIN] Aguardando conclusão das rotas...\n\n");
    
    // AGUARDAR TÉRMINO SIMPLES - apenas pthread_join em todas
    for (int i = 0; i < num_aeronaves; i++) {
        if (aeronaves[i] != NULL) {
            pthread_join(aeronaves[i]->thread, NULL);
            printf("[MAIN] Aeronave %d concluiu sua rota\n", i);
            aeronave_destruir(aeronaves[i]);
            aeronaves[i] = NULL;
        }
    }
    
    // Gerar relatório final
    printf("\n===============================================\n");
    printf("            RELATÓRIO FINAL\n");
    printf("===============================================\n");
    
    printf("Setores configurados: %d\n", num_setores);
    printf("Aeronaves simuladas: %d\n", num_aeronaves);
    printf("Todas as aeronaves completaram suas rotas!\n");
    printf("Sistema finalizado com sucesso.\n");
    printf("===============================================\n");
    
    // Finalizar sistema
    atc_finalizar();
    
    // Liberar array de aeronaves (já liberadas individualmente)
    free(aeronaves);
    
    return 0;
}