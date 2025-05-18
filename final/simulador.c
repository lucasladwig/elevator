#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>


/* === DEFINIÇÕES E CONSTANTES === */
#define MAX_CHAMADAS 100
#define MAX_ANDARES 50
#define MAX_ELEVADORES 10
#define TAM_BUFFER 10
#define TRUE 1
#define FALSE 0


/* === ESTRUTURAS DE DADOS === */
// Chamada para elevador
typedef struct 
{
    // int id;
    int origem;
    int destino;
} Chamada;

// Buffer de chamadas
typedef struct 
{
    Chamada chamadas[TAM_BUFFER];
    int inicio;
    int fim;
    int contador;
} BufferChamadas;

// Elevador
typedef struct 
{
    int id;
    int andar_atual;
    int chamadas_atendidas;
    sem_t sem_elevador_ocupou;
    Chamada chamada_atual;
    int ocupado;
} Elevador;


/* === VARIÁVEIS GLOBAIS === */
// Parâmetros da simulação (definidos pelo usuário)
int n_andares, n_elevadores, n_chamadas;
int id_chamada = 0;
int chamadas_geradas = 0;

Elevador elevadores[MAX_ELEVADORES];

// Estruturas de sincronizacao
BufferChamadas buffer;
pthread_mutex_t mutex_buffer, mutex_chamada, mutex_chamadas_geradas;
sem_t sem_buffer_ocupou, sem_buffer_liberou;


/* === PADRAO SCHEDULER === */
// SCHEDULER: Thread que gerencia o fluxo de chamadas entre andares e elevadores
void* funcao_scheduler(void* arg)
{
    while (TRUE) {
        // Aguarda ter chamada no buffer e adquire tranca
        sem_wait(&sem_buffer_ocupou);
        pthread_mutex_lock(&mutex_buffer);

        // Garante que o scheduler nunca tente acessar o buffer vazio
        if (buffer.contador == 0) {
            pthread_mutex_unlock(&mutex_buffer);
            continue;
        }

        // Acessa a primeira chamada do buffer (FIFO)
        Chamada c = buffer.chamadas[buffer.inicio];
        buffer.inicio = (buffer.inicio + 1) % TAM_BUFFER;
        buffer.contador--;

        // Libera tranca e sinaliza espaco livre no buffer
        pthread_mutex_unlock(&mutex_buffer);
        sem_post(&sem_buffer_liberou);

        // Escolhe o elevador mais próximo do andar de origem
        int melhor_id = -1;
        int menor_dist = MAX_ANDARES;

        // Percorre array de elevadores e escolhe o mais proximo
        for (int i = 0; i < n_elevadores; i++) {
            if (!elevadores[i].ocupado) {
                int dist = abs(elevadores[i].andar_atual - c.origem);
                if (dist < menor_dist) {
                    menor_dist = dist;
                    melhor_id = i;
                }
            }
        }

        // Designa chamada para elevador mais proximo
        if (melhor_id != -1) {
            elevadores[melhor_id].chamada_atual = c;
            elevadores[melhor_id].ocupado = TRUE;
            printf("[Scheduler] Chamada para elevador %d\n", melhor_id);

            // Sinaliza que elevador ocupou
            sem_post(&elevadores[melhor_id].sem_elevador_ocupou);
        } else {
            // Nenhum elevador disponível
            //, pode implementar fila ou esperar (simplesmente descarta neste exemplo)
            printf("[Scheduler] Nenhum elevador disponível para a chamada\n");
        }

        usleep(250);
    }
    return 0;
}


/* === PADRAO PRODUTOR-CONSUMIDOR === */
// PRODUTOR: Threads dos andares produtores de chamadas
void* funcao_andar(void* arg) {
    int origem = *(int*)arg;
    free(arg);

    while (TRUE) {
        // Espera espaco livre no buffer e adquire tranca
        sem_wait(&sem_buffer_liberou);
        pthread_mutex_lock(&mutex_buffer);
        
        // Verifica se já atingimos o limite de chamadas
        if (chamadas_geradas >= n_chamadas) {
            pthread_mutex_unlock(&mutex_buffer);
            break;
        }

        // Garante que andar destino e diferente de origem
        int destino;
        do {
            destino = rand() % n_andares;
        } while (destino == origem); 

        // Cria nova chamada
        pthread_mutex_lock(&mutex_chamada);
        Chamada c = {origem, destino};
        pthread_mutex_unlock(&mutex_chamada);

        chamadas_geradas++;

        // Insere chamada no buffer
        buffer.chamadas[buffer.fim] = c;
        buffer.fim = (buffer.fim + 1) % TAM_BUFFER;
        buffer.contador++;

        printf("[Andar %d] Nova chamada: %d -> %d\n", origem, c.origem, c.destino);

        // Libera tranca e sinaliza que  há chamada disponível
        pthread_mutex_unlock(&mutex_buffer);
        sem_post(&sem_buffer_ocupou);

        // Aguarda um tempo aleatorio (1-4seg) ate criar nova chamada
        sleep(rand() % 3 + 1);
    }
    return 0;
}

// CONSUMIDOR: Threads dos elevadores consumidores de chamadas
void* funcao_elevador(void* arg) {
    Elevador* e = (Elevador*)arg;

    while (TRUE) {
        // Aguarda sinal do scheduler
        sem_wait(&e->sem_elevador_ocupou);

        // Designa nova chamada
        Chamada c = e->chamada_atual;

        // Simula movimento de andar atual para origem da chamada
        printf("[Elevador %d] De %d para %d (atendendo origem da chamada)\n", e->id, e->andar_atual, c.origem);
        sleep(abs(e->andar_atual - c.origem));  
        e->andar_atual = c.origem;

        // Simula movimento de andar origem para destino da chamada
        printf("[Elevador %d] De %d para %d (indo para destino da chamada)\n", e->id, e->andar_atual, c.destino);
        sleep(abs(c.destino - e->andar_atual));
        e->andar_atual = c.destino;

        // Atualiza estado do elevador
        e->chamadas_atendidas++;
        e->ocupado = FALSE;

        // Atualiza chamadas concluidas
        pthread_mutex_lock(&mutex_chamadas_geradas);
        chamadas_geradas++;
        if (chamadas_geradas >= n_chamadas) {
            pthread_mutex_unlock(&mutex_chamadas_geradas);
            break;
        }
        pthread_mutex_unlock(&mutex_chamadas_geradas);

        printf("[Elevador %d] Chamada concluida. Subtotal atendidas: %d\n", e->id, e->chamadas_atendidas);
    }

    return 0;
}


/* === FUNCAO PRINCIPAL === */
int main (int argc, char* argv[]) 
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

    // Incializa semente aleatoria
    srand(time(NULL));

    // Inicializa threads e arrays
    pthread_t threads_andares[n_andares];
    pthread_t threads_elevadores[n_elevadores];
    pthread_t thread_scheduler;

    // Inicializa buffer de chamadas
    buffer.inicio = 0;
    buffer.fim = 0;
    buffer.contador = 0;

    // Inicializa estruturas de sincronizacao do buffer
    pthread_mutex_init(&mutex_buffer, NULL);
    sem_init(&sem_buffer_ocupou, 0, 0);
    sem_init(&sem_buffer_liberou, 0, TAM_BUFFER);

    // Cria threads dos elevadores
    for (int i = 0; i < n_elevadores; i++) {
        elevadores[i].id = i;
        elevadores[i].andar_atual = 0;
        elevadores[i].chamadas_atendidas = 0;
        elevadores[i].ocupado = 0;
        sem_init(&elevadores[i].sem_elevador_ocupou, 0, 0);
        pthread_create(&threads_elevadores[i], NULL, funcao_elevador, &elevadores[i]);
    }

    // Cria threads dos andares
    for (int i = 0; i < n_andares; i++) {
        int* andar = malloc(sizeof(int));
        *andar = i;
        pthread_create(&threads_andares[i], NULL, funcao_andar, andar);
    }

    // Cria thread scheduler
    pthread_create(&thread_scheduler, NULL, funcao_scheduler, NULL);

    // Aguarda todas as threads de andares terminarem
    for (int i = 0; i < n_andares; i++) {
        pthread_join(threads_andares[i], NULL);
    }
    printf("Todas as threads de andares foram encerradas.\n");
    printf("Total de %d chamadas foram geradas.\n\n", chamadas_geradas);

    // Aguarda todas as threads de elevadores terminarem
    for (int i = 0; i < n_elevadores; i++) {
        pthread_join(threads_elevadores[i], NULL);
    }
    printf("Todas as threads de elevadores foram encerradas.\n");

    // Libera os recursos de sincronização
    pthread_mutex_destroy(&mutex_buffer);
    pthread_mutex_destroy(&mutex_chamada);
    pthread_mutex_destroy(&mutex_chamadas_geradas);
    sem_destroy(&sem_buffer_ocupou);
    sem_destroy(&sem_buffer_liberou);

    // Estatísticas finais
    printf("\n=== SIMULAÇÃO FINALIZADA ===\n");
    printf("Todas as %d chamadas foram atendidas com sucesso.\n", n_chamadas);
    printf("Posição final dos elevadores:\n");
    for (int i = 0; i < n_elevadores; i++) {
        printf("- Elevador %d: andar %d\n", elevadores[i].id, elevadores[i].andar_atual);
        printf("- Elevador %d: chamadas atendidas: %d\n", elevadores[i].id, elevadores[i].chamadas_atendidas);
    }

    return 0;
    
    
}