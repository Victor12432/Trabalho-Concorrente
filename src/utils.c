#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "../include/utils.h"
#include "../include/aeronave.h"

/**
 * Imprime o timestamp atual no formato HH:MM:SS.microseconds
 */
void imprimir_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t now = tv.tv_sec;
    struct tm *tm_info = localtime(&now);
    
    printf("[%02d:%02d:%02d.%06ld] ", 
           tm_info->tm_hour, 
           tm_info->tm_min, 
           tm_info->tm_sec, 
           tv.tv_usec);
}

/**
 * Gera uma rota aleatória para uma aeronave
 * Otimizado para reduzir chamadas a rand() e operações de módulo
 * @param comprimento: Tamanho da rota (número de setores)
 * @param total_setores: Número total de setores disponíveis no espaço aéreo
 * @return Ponteiro para array de inteiros contendo a rota
 */
int* gerar_rota_aleatoria(int comprimento, int total_setores) {
    if (comprimento <= 0 || total_setores <= 0) {
        return NULL;
    }
    
    int *rota = malloc(sizeof(int) * comprimento);
    if (!rota) {
        perror("malloc rota");
        return NULL;
    }
    
    // Gera uma rota começando de um setor aleatório
    int setor_atual = rand() % total_setores;
    rota[0] = setor_atual;
    
    // Gera o resto da rota de forma sequencial ou com pequenos saltos
    for (int i = 1; i < comprimento; i++) {
        // Otimização: uma única chamada rand() por iteração
        int aleatorio = rand();
        int tipo_movimento = aleatorio % 100;
        
        if (tipo_movimento < 70) {
            // 70% de chance: Move para setor adjacente (mais eficiente)
            setor_atual = (setor_atual + 1) % total_setores;
        } else if (tipo_movimento < 90) {
            // 20% de chance: Move para setor adjacente anterior
            setor_atual = (setor_atual + total_setores - 1) % total_setores;
        } else {
            // 10% de chance: Faz um salto aleatório pequeno
            int salto = ((aleatorio >> 8) % 3) + 1;  // Reutiliza bits do rand()
            int direcao = (aleatorio & 1) ? 1 : -1;
            setor_atual = (setor_atual + (salto * direcao) + total_setores) % total_setores;
        }
        rota[i] = setor_atual;
    }
    
    return rota;
}

/**
 * Gera um comprimento de rota aleatório baseado no total de setores
 * @param total_setores: Número total de setores no espaço aéreo
 * @return Comprimento da rota gerado aleatoriamente
 */
int gerar_comprimento_rota(int total_setores) {
    if (total_setores <= 0) {
        return 3; // Valor padrão mínimo
    }
    
    // Gera rota entre 50% e 150% do total de setores
    int minimo = (total_setores / 2) > 3 ? (total_setores / 2) : 3;
    int maximo = (total_setores * 3 / 2) > minimo ? (total_setores * 3 / 2) : minimo + 5;
    
    return minimo + (rand() % (maximo - minimo + 1));
}

/**
 * Calcula o tempo médio de espera de todas as aeronaves
 * Otimizado para evitar divisão desnecessária
 * @param aeronaves: Array de ponteiros para aeronaves
 * @param total_aeronaves: Número total de aeronaves
 * @return Tempo médio de espera em segundos
 */
double calcular_tempo_medio(aeronave_t **aeronaves, int total_aeronaves) {
    if (!aeronaves || total_aeronaves <= 0) {
        return 0.0;
    }
    
    double tempo_total = 0.0;
    int aeronaves_validas = 0;
    
    // Percorre apenas uma vez o array
    for (int i = 0; i < total_aeronaves; i++) {
        if (aeronaves[i]) {
            double media_aeronave = aeronave_calcular_media_espera(aeronaves[i]);
            tempo_total += media_aeronave;
            aeronaves_validas++;
        }
    }
    
    // Evita divisão se não houver aeronaves válidas
    return (aeronaves_validas > 0) ? (tempo_total / aeronaves_validas) : 0.0;
}
