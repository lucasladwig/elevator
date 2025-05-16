#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

/* ======================================================================
 * DEFINIÇÕES E CONSTANTES
 * ====================================================================== */
#define MAX_CALLS 100      // Número máximo de chamadas
#define MAX_ELEVATORS 10   // Número máximo de elevadores
#define MAX_FLOORS 100     // Número máximo de andares

/* ======================================================================
 * ESTRUTURAS DE DADOS
 * ====================================================================== */
/**
 * Representa uma chamada de elevador
 */
typedef struct {
    int origem;    // Andar de origem da chamada
    int destino;   // Andar de destino da chamada
    int atendida;  // Flag indicando se a chamada já foi atendida (0=não, 1=sim)
} Chamada;

/**
 * Representa um elevador
 */
typedef struct {
    int id;             // Identificador do elevador
    int andar_atual;    // Andar atual onde o elevador se encontra
} Elevador;

/* ======================================================================
 * VARIÁVEIS GLOBAIS
 * ====================================================================== */
// Parâmetros da simulação (definidos pelo usuário)
int n_andares;          // Número total de andares no edifício
int m_elevadores;       // Número total de elevadores no edifício
int total_chamadas;     // Número total de chamadas a serem geradas

// Estado da simulação
Chamada chamadas[MAX_CALLS];        // Lista de todas as chamadas
int chamadas_pendentes = 0;         // Contador de chamadas criadas
Elevador elevadores[MAX_ELEVATORS]; // Estado dos elevadores

// Mecanismos de sincronização
pthread_mutex_t mutex_chamadas;     // Mutex para proteger o acesso às chamadas
sem_t sem_chamadas;                 // Semáforo para sinalizar novas chamadas (Produtor-Consumidor)

/* ======================================================================
 * IMPLEMENTAÇÃO DO PADRÃO PRODUTOR-CONSUMIDOR
 * ====================================================================== */
/**
 * Thread que representa um andar (PRODUTOR)
 * Gera chamadas aleatórias de elevador
 */
void* thread_andar(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Loop principal de geração de chamadas
    while (1) {
        // Seção crítica - acesso à lista de chamadas
        pthread_mutex_lock(&mutex_chamadas);
        
        // Verifica se já atingimos o limite de chamadas
        if (chamadas_pendentes >= total_chamadas) {
            pthread_mutex_unlock(&mutex_chamadas);
            break;
        }

        // Criar nova chamada com origem neste andar e destino aleatório
        Chamada c;
        c.origem = id;
        do {
            c.destino = rand() % n_andares;
        } while (c.destino == c.origem); // Garante que destino seja diferente da origem
        c.atendida = 0;

        // Adiciona a chamada à lista
        chamadas[chamadas_pendentes++] = c;
        printf("[Andar %d] Nova chamada criada: destino %d\n", id, c.destino);
        printf("--- Total de chamadas pendentes: %d/%d\n\n", chamadas_pendentes, total_chamadas);

        // Sinaliza ao consumidor (elevador) que há uma nova chamada disponível
        sem_post(&sem_chamadas); 
        
        // Fim da seção crítica
        pthread_mutex_unlock(&mutex_chamadas);
        
        // Espera aleatória antes de gerar próxima chamada
        sleep(rand() % 3 + 1);
    }
    
    return NULL;
}

/* ======================================================================
 * IMPLEMENTAÇÃO DO PADRÃO SCHEDULER
 * ====================================================================== */
/**
 * Implementa o algoritmo de escalonamento para escolher o elevador mais próximo
 * @param origem Andar de origem da chamada
 * @return ID do elevador escolhido
 */
int escolher_elevador(int origem) {
    int menor_distancia = INT_MAX;
    int escolhidos[MAX_ELEVATORS];
    int count = 0;

    // Encontra o(s) elevador(es) mais próximo(s)
    for (int i = 0; i < m_elevadores; i++) {
        int distancia = abs(elevadores[i].andar_atual - origem);
        
        if (distancia < menor_distancia) {
            // Encontrou elevador mais próximo - limpa lista anterior
            menor_distancia = distancia;
            count = 0;  // Reset do contador para começar uma nova lista
            escolhidos[count++] = i;
        } 
        else if (distancia == menor_distancia) {
            // Elevador com mesma distância - adiciona à lista de candidatos
            escolhidos[count++] = i;
        }
    }

    // Se houver múltiplos elevadores na mesma distância, escolhe um aleatoriamente
    return escolhidos[rand() % count];
}

/**
 * Thread que representa um elevador (CONSUMIDOR)
 * Atende às chamadas geradas pelos andares
 */
void* thread_elevador(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Inicializa o estado do elevador
    elevadores[id].id = id;
    elevadores[id].andar_atual = 0;  // Todos elevadores começam no térreo
    
    printf("[Elevador %d] Iniciado no andar 0\n\n", id);

    // Loop principal de atendimento às chamadas
    while (1) {
        // Espera ser sinalizado que há uma chamada disponível
        sem_wait(&sem_chamadas);
        
        // Seção crítica - acesso à lista de chamadas
        pthread_mutex_lock(&mutex_chamadas);

        // Busca por uma chamada não atendida que este elevador deve atender
        int encontrada = -1;
        for (int i = 0; i < chamadas_pendentes; i++) {
            if (!chamadas[i].atendida) {
                // Usa o Scheduler para decidir qual elevador deve atender
                int escolhido = escolher_elevador(chamadas[i].origem);
                
                if (escolhido == id) {
                    chamadas[i].atendida = 1;
                    encontrada = i;
                    break;
                }
            }
        }
        
        // Fim da seção crítica
        pthread_mutex_unlock(&mutex_chamadas);

        if (encontrada != -1) {
            // Atende a chamada encontrada
            Chamada c = chamadas[encontrada];
            
            printf("[Elevador %d] Atendendo chamada: de %d para %d (distância: %d andares)\n", 
                id, c.origem, c.destino, abs(elevadores[id].andar_atual - c.origem));
            
            // Desloca para o andar de origem
            printf("[Elevador %d] Movendo do andar %d para andar %d (origem)\n", 
                id, elevadores[id].andar_atual, c.origem);
            sleep(abs(elevadores[id].andar_atual - c.origem));  // Tempo proporcional à distância
            elevadores[id].andar_atual = c.origem;
            printf("[Elevador %d] Chegou ao andar de origem %d\n", id, c.origem);
            
            // Desloca para o andar de destino
            printf("[Elevador %d] Movendo do andar %d para andar %d (destino)\n", 
                id, c.origem, c.destino);
            sleep(abs(c.origem - c.destino));  // Tempo proporcional à distância
            elevadores[id].andar_atual = c.destino;
            printf("[Elevador %d] Chegou ao destino %d\n\n", id, c.destino);
        } 
        else {
            // Nenhuma chamada para este elevador, devolve semáforo
            sem_post(&sem_chamadas);
            break;
        }
    }
    
    printf("[Elevador %d] Encerrado no andar %d\n\n", id, elevadores[id].andar_atual);
    return NULL;
}

/* ======================================================================
 * FUNÇÃO PRINCIPAL
 * ====================================================================== */
int main(int argc, char* argv[]) {
    // Verificação dos argumentos de linha de comando
    if (argc != 4) {
        printf("Uso: %s <n_andares> <m_elevadores> <total_chamadas>\n", argv[0]);
        printf("Exemplo: %s 10 3 20\n\n", argv[0]);
        return 1;
    }

    // Inicialização do gerador de números aleatórios
    srand(time(NULL));

    // Leitura dos parâmetros da simulação
    n_andares = atoi(argv[1]);
    m_elevadores = atoi(argv[2]);
    total_chamadas = atoi(argv[3]);
    
    printf("=== SIMULADOR DE ELEVADORES ===\n");
    printf("Edifício com %d andares\n", n_andares);
    printf("Sistema com %d elevadores\n", m_elevadores);
    printf("Total de %d chamadas a serem atendidas\n\n", total_chamadas);

    // Validação dos parâmetros
    if (n_andares <= 0 || n_andares > MAX_FLOORS) {
        printf("Erro: número de andares deve estar entre 1 e %d\n", MAX_FLOORS);
        return 1;
    }
    if (m_elevadores <= 0 || m_elevadores > MAX_ELEVATORS) {
        printf("Erro: número de elevadores deve estar entre 1 e %d\n", MAX_ELEVATORS);
        return 1;
    }
    if (total_chamadas <= 0 || total_chamadas > MAX_CALLS) {
        printf("Erro: número de chamadas deve estar entre 1 e %d\n", MAX_CALLS);
        return 1;
    }

    // Inicialização dos mecanismos de sincronização
    pthread_mutex_init(&mutex_chamadas, NULL);
    sem_init(&sem_chamadas, 0, 0);  // Semáforo inicializado em 0 (nenhuma chamada disponível)

    // Criação das threads de andares (produtores)
    pthread_t threads_andares[n_andares];
    printf("Iniciando %d threads de andares (produtores)...\n", n_andares);
    for (int i = 0; i < n_andares; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads_andares[i], NULL, thread_andar, id);
    }

    // Criação das threads de elevadores (consumidores)
    pthread_t threads_elevadores[m_elevadores];
    printf("Iniciando %d threads de elevadores (consumidores)...\n\n", m_elevadores);
    for (int i = 0; i < m_elevadores; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads_elevadores[i], NULL, thread_elevador, id);
    }

    // Aguarda todas as threads de andares terminarem (todas as chamadas geradas)
    for (int i = 0; i < n_andares; i++) {
        pthread_join(threads_andares[i], NULL);
    }
    printf("Todas as threads de andares foram encerradas.\n");
    printf("Total de %d chamadas foram geradas.\n\n", chamadas_pendentes);

    // Sinaliza a todas as threads de elevadores para verificarem se há trabalho pendente
    // e depois encerrarem
    for (int i = 0; i < m_elevadores; i++) {
        sem_post(&sem_chamadas);
    }

    // Aguarda todas as threads de elevadores terminarem
    for (int i = 0; i < m_elevadores; i++) {
        pthread_join(threads_elevadores[i], NULL);
    }
    printf("Todas as threads de elevadores foram encerradas.\n");

    // Libera os recursos de sincronização
    pthread_mutex_destroy(&mutex_chamadas);
    sem_destroy(&sem_chamadas);

    // Estatísticas finais
    printf("\n=== SIMULAÇÃO FINALIZADA ===\n");
    printf("Todas as %d chamadas foram atendidas com sucesso.\n", total_chamadas);
    printf("Posição final dos elevadores:\n");
    for (int i = 0; i < m_elevadores; i++) {
        printf("- Elevador %d: andar %d\n", i, elevadores[i].andar_atual);
    }
    
    return 0;
}