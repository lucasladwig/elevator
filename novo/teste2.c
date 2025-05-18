#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>
#include <semaphore.h>


/* === DEFINIÇÕES E CONSTANTES === */
#define MAX_CHAMADAS 100
#define MAX_ELEVADORES 10
#define MAX_ANDARES 50
#define TRUE 1
#define FALSE 0


/* === ESTRUTURAS DE DADOS === */
// Representa uma chamada para elevador
typedef struct {
    int origem;    // Andar de origem da chamada
    int destino;   // Andar de destino da chamada
    int atendida;  // Indica se a chamada já foi atendida
} Chamada;

// Representa um elevador
typedef struct {
    int id;           // Identificador do elevador
    int andar_atual;  // Andar atual onde o elevador se encontra
} Elevador;


/* === VARIÁVEIS GLOBAIS === */
// Parâmetros da simulação (definidos pelo usuário)
int n_andares;     // Número total de andares no edifício
int n_elevadores;  // Número total de elevadores no edifício
int n_chamadas;    // Número total de chamadas a serem geradas

// Estado da simulação
Chamada chamadas[MAX_CHAMADAS];        // Lista de todas as chamadas
int chamadas_pendentes = 0;         // Contador de chamadas nao atendidas
Elevador elevadores[MAX_ELEVADORES]; // Estado dos elevadores

// Mecanismos de sincronização
pthread_mutex_t mutex_chamadas;     // Mutex para proteger o acesso às chamadas
sem_t sem_chamadas;                 // Semáforo para sinalizar novas chamadas (Produtor-Consumidor)


/* === PRODUTOR-CONSUMIDOR === */
// Thread do andar (PRODUTOR) que gera chamadas aleatórias para um elevador
void* thread_andar(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Loop principal de geração de chamadas
    while (1) {
        // Seção crítica - acesso à lista de chamadas
        pthread_mutex_lock(&mutex_chamadas);
        
        // Verifica se já atingimos o limite de chamadas
        if (chamadas_pendentes >= n_chamadas) {
            pthread_mutex_unlock(&mutex_chamadas);
            break;
        }

        // Criar nova chamada com origem neste andar e destino aleatório
        Chamada c;
        c.origem = id;
        do {
            c.destino = rand() % n_andares;
        } while (c.destino == c.origem); // Garante que destino seja diferente da origem
        c.atendida = FALSE;

        // Adiciona a chamada à lista
        chamadas[chamadas_pendentes++] = c;
        printf("[Andar %d] Nova chamada criada: destino %d\n", id, c.destino);
        printf("--- Total de chamadas pendentes: %d/%d\n\n", chamadas_pendentes, n_chamadas);

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
// Implementa o algoritmo de escalonamento para escolher o elevador mais próximo

int escolher_elevador(int origem) {
    int menor_distancia = MAX_ANDARES;
    int escolhidos[MAX_ELEVADORES];
    int contador = 0;

    // Encontra o elevador mais próximo
    for (int i = 0; i < n_elevadores; i++) {
        int distancia = abs(elevadores[i].andar_atual - origem);
        
        if (distancia < menor_distancia) {
            // Encontrou elevador mais próximo - limpa lista anterior
            menor_distancia = distancia;
            contador = 0;  // Reset do contador para começar uma nova lista
            escolhidos[contador++] = i;
        } 
        else if (distancia == menor_distancia) {
            // Elevador com mesma distância - adiciona à lista de candidatos
            escolhidos[contador++] = i;
        }
    }

    // Se houver múltiplos elevadores na mesma distância, escolhe um aleatoriamente
    return escolhidos[rand() % contador];
}

// Thread do elevador (CONSUMIDOR) que atende às chamadas geradas pelos andares
void* thread_elevador(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Inicializa o estado do elevador
    elevadores[id].id = id;
    elevadores[id].andar_atual = 0;  // Todos elevadores começam no térreo
    
    // Loop de atendimento às chamadas
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
                    chamadas[i].atendida = TRUE;
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
int main(int argc, char* argv[]) 
{
    // Validação dos argumentos de linha de comando
    if (argc != 4) {
        printf("Erro: chamada do programa deve estar no formato %s <n_andares> <n_elevadores> <n_chamadas>\n", argv[0]);
        printf("Exemplo: %s 10 3 20\n\n", argv[0]);
        return 1;
    }

    n_andares = atoi(argv[1]);
    n_elevadores = atoi(argv[2]);
    n_chamadas = atoi(argv[3]);

    if (n_andares < 2 || n_andares > MAX_ANDARES) {
        printf("Erro: número de andares deve estar entre 2 e %d\n", MAX_ANDARES);
        return 1;
    }
    if (n_elevadores < 2 || n_elevadores > MAX_ELEVADORES) {
        printf("Erro: número de elevadores deve estar entre 2 e %d\n", MAX_ELEVADORES);
        return 1;
    }
    if (n_chamadas < 2 || n_chamadas > MAX_CHAMADAS) {
        printf("Erro: número de chamadas deve estar entre 2 e %d\n", MAX_CHAMADAS);
        return 1;
    }

    // Inicialização do gerador de números aleatórios
    srand(time(NULL));
    
    printf("=== SIMULADOR DE ELEVADORES ===\n");
    printf("%d andares, %d elevadores, %d chamadas\n", n_andares, n_elevadores, n_chamadas);
    sleep(2);


    // Inicialização do semaforo e mutex do buffer de chamadas
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
    pthread_t threads_elevadores[n_elevadores];
    printf("Iniciando %d threads de elevadores (consumidores)...\n\n", n_elevadores);
    for (int i = 0; i < n_elevadores; i++) {
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
    for (int i = 0; i < n_elevadores; i++) {
        sem_post(&sem_chamadas);
    }

    // Aguarda todas as threads de elevadores terminarem
    for (int i = 0; i < n_elevadores; i++) {
        pthread_join(threads_elevadores[i], NULL);
    }
    printf("Todas as threads de elevadores foram encerradas.\n");

    // Libera os recursos de sincronização
    pthread_mutex_destroy(&mutex_chamadas);
    sem_destroy(&sem_chamadas);

    // Estatísticas finais
    printf("\n=== SIMULAÇÃO FINALIZADA ===\n");
    printf("Todas as %d chamadas foram atendidas com sucesso.\n", n_chamadas);
    printf("Posição final dos elevadores:\n");
    for (int i = 0; i < n_elevadores; i++) {
        printf("- Elevador %d: andar %d\n", i, elevadores[i].andar_atual);
    }
    
    return 0;
}