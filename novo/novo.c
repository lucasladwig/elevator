#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
// #include <math.h>

#include <pthread.h>
#include <semaphore.h>


/* === DEFINIÇÕES E CONSTANTES === */
#define MAX_CHAMADAS 100
#define MAX_ELEVADORES 10
#define MAX_ANDARES 50
#define TRUE 1
#define FALSE 0

/* === ESTRUTURAS DE DADOS === */
// Chamada para elevador
typedef struct {
    int origem;
    int destino;
} Chamada;

// Elevador
typedef struct {
    int id;
    int andar_atual;
    int ocupado;
    pthread_mutex_t lock;
} Elevador;

/* === VARIÁVEIS GLOBAIS === */
// Parâmetros da simulação (definidos pelo usuário)
int n_andares;
int n_elevadores;
int n_chamadas;

Chamada chamadas_pendentes[MAX_CHAMADAS];
int chamadas_count = 0;

pthread_mutex_t chamadas_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t chamadas_sem;

Elevador elevadores[MAX_ELEVADORES];

// Simula o movimento do elevador
void simular_movimento(Elevador *e, int destino) {
    int tempo = abs(e->andar_atual - destino);
    printf("Elevador %d: indo de %d para %d (%ds)\n", e->id, e->andar_atual, destino, tempo);
    sleep(tempo);
    e->andar_atual = destino;
}

// Adiciona chamada à lista
void adicionar_chamada(Chamada c) {
    pthread_mutex_lock(&chamadas_mutex);
    if (chamadas_count < MAX_CHAMADAS) {
        chamadas_pendentes[chamadas_count++] = c;
        sem_post(&chamadas_sem);
    }
    pthread_mutex_unlock(&chamadas_mutex);
}

// Escolhe a chamada mais próxima de qualquer elevador livre
int escolher_chamada(int *idx_escolhida, Elevador **elevador_escolhido) {
    pthread_mutex_lock(&chamadas_mutex);

    int melhor_idx = -1;
    int menor_dist = 1e9;
    Elevador *melhor_elev = NULL;

    for (int i = 0; i < chamadas_count; i++) {
        Chamada c = chamadas_pendentes[i];
        for (int j = 0; j < n_elevadores; j++) {
            pthread_mutex_lock(&elevadores[j].lock);
            if (!elevadores[j].ocupado) {
                int dist = abs(elevadores[j].andar_atual - c.origem);
                if (dist < menor_dist) {
                    menor_dist = dist;
                    melhor_idx = i;
                    melhor_elev = &elevadores[j];
                }
            }
            pthread_mutex_unlock(&elevadores[j].lock);
        }
    }

    if (melhor_idx != -1) {
        *idx_escolhida = melhor_idx;
        *elevador_escolhido = melhor_elev;
    }

    pthread_mutex_unlock(&chamadas_mutex);
    return melhor_idx != -1;
}

// Remove chamada da lista por índice
Chamada remover_chamada(int idx) {
    pthread_mutex_lock(&chamadas_mutex);
    Chamada c = chamadas_pendentes[idx];
    for (int i = idx; i < chamadas_count - 1; i++) {
        chamadas_pendentes[i] = chamadas_pendentes[i + 1];
    }
    chamadas_count--;
    pthread_mutex_unlock(&chamadas_mutex);
    return c;
}

// Thread dos andares (produtores)
void* thread_andar(void *arg) {
    int id = *(int*)arg;
    free(arg);

    for (int i = 0; i < 2; i++) {
        int destino;
        do {
            destino = rand() % n_andares;
        } while (destino == id);

        Chamada c = { .origem = id, .destino = destino };
        printf("Andar %d: gerou chamada para andar %d\n", id, destino);
        adicionar_chamada(c);
        sleep(rand() % 3 + 1);
    }
    return NULL;
}

// Thread do scheduler (consumidor)
void* thread_scheduler(void *arg) {
    int chamadas_esperadas = n_andares * 2;
    for (int i = 0; i < chamadas_esperadas; i++) {
        sem_wait(&chamadas_sem);

        int idx;
        Elevador *e;

        // Espera por elevador livre
        while (!escolher_chamada(&idx, &e)) {
            usleep(100000);
        }

        pthread_mutex_lock(&e->lock);
        e->ocupado = 1;
        pthread_mutex_unlock(&e->lock);

        Chamada c = remover_chamada(idx);

        simular_movimento(e, c.origem);
        printf("Elevador %d pegou passageiro no andar %d\n", e->id, c.origem);

        simular_movimento(e, c.destino);
        printf("Elevador %d deixou passageiro no andar %d\n", e->id, c.destino);

        pthread_mutex_lock(&e->lock);
        e->ocupado = 0;
        pthread_mutex_unlock(&e->lock);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
        
    // Validação dos argumentos de linha de comando
    if (argc != 4) {
        printf("Erro: chamada do programa deve estar no formato %s <n_andares> <n_elevadores> <n_chamadas>\n", argv[0]);
        printf("Exemplo: %s 10 3 20\n\n", argv[0]);
        return 1;
    }

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
    // Leitura dos parâmetros da simulação
    n_andares = atoi(argv[1]);
    n_elevadores = atoi(argv[2]);
    n_chamadas = atoi(argv[3]);

    // Inicialização do gerador de números aleatórios
    srand(time(NULL));
    
    pthread_t threads_andares[n_andares];
    pthread_t scheduler;

    sem_init(&chamadas_sem, 0, 0);

    for (int i = 0; i < n_elevadores; i++) {
        elevadores[i].id = i;
        elevadores[i].andar_atual = 0;
        elevadores[i].ocupado = FALSE;
        pthread_mutex_init(&elevadores[i].lock, NULL);
    }

    for (int i = 0; i < n_andares; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads_andares[i], NULL, thread_andar, id);
    }

    pthread_create(&scheduler, NULL, thread_scheduler, NULL);

    for (int i = 0; i < n_andares; i++) {
        pthread_join(threads_andares[i], NULL);
    }
    pthread_join(scheduler, NULL);

    sem_destroy(&chamadas_sem);
    pthread_mutex_destroy(&chamadas_mutex);
    for (int i = 0; i < n_elevadores; i++) {
        pthread_mutex_destroy(&elevadores[i].lock);
    }

    return 0;
}
