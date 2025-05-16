/*
Simulador de Elevadores Concorrente em C
Requisitos atendidos:
- Programação concorrente com threads.
- Padrões de projeto Scheduler e Produtor-Consumidor.
- Sincronização com mutex e semáforos.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define MAX_CALLS 100
#define MAX_ELEVATORS 10
#define MAX_FLOORS 100

typedef struct {
    int origem;
    int destino;
    int atendida;
} Chamada;

int n_andares, m_elevadores, total_chamadas;
Chamada chamadas[MAX_CALLS];
int chamadas_pendentes = 0;

pthread_mutex_t mutex_chamadas;
sem_t sem_chamadas;

// Estados dos elevadores
typedef struct {
    int id;
    int andar_atual;
} Elevador;

Elevador elevadores[MAX_ELEVATORS];

// Função para gerar chamadas aleatórias
void* thread_andar(void* arg) {
    int id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&mutex_chamadas);
        if (chamadas_pendentes >= total_chamadas) {
            pthread_mutex_unlock(&mutex_chamadas);
            break;
        }

        // Criar nova chamada
        Chamada c;
        c.origem = id;
        do {
            c.destino = rand() % n_andares;
        } while (c.destino == c.origem);
        c.atendida = 0;

        chamadas[chamadas_pendentes++] = c;
        printf("[Andar %d] Nova chamada criada: destino %d\n\n", id, c.destino);

        sem_post(&sem_chamadas); // Produtor avisa consumidor
        pthread_mutex_unlock(&mutex_chamadas);
        sleep(rand() % 3 + 1); // Espera aleatória
    }
    free(arg);
    return NULL;
}

// Scheduler escolhe o elevador mais próximo
int escolher_elevador(int origem) {
    int menor_distancia = INT_MAX;
    int escolhidos[MAX_ELEVATORS];
    int count = 0;

    for (int i = 0; i < m_elevadores; i++) {
        int distancia = abs(elevadores[i].andar_atual - origem);
        if (distancia < menor_distancia) {
            menor_distancia = distancia;
            count = 0;
            escolhidos[count++] = i;
        } else if (distancia == menor_distancia) {
            escolhidos[count++] = i;
        }
    }

    return escolhidos[rand() % count];
}

// Elevador consumidor
void* thread_elevador(void* arg) {
    int id = *(int*)arg;
    elevadores[id].id = id;
    elevadores[id].andar_atual = 0;

    while (1) {
        sem_wait(&sem_chamadas);
        pthread_mutex_lock(&mutex_chamadas);

        int encontrada = -1;
        for (int i = 0; i < chamadas_pendentes; i++) {
            if (!chamadas[i].atendida) {
                int escolhido = escolher_elevador(chamadas[i].origem);
                if (escolhido == id) {
                    chamadas[i].atendida = 1;
                    encontrada = i;
                    break;
                }
            }
        }

        pthread_mutex_unlock(&mutex_chamadas);

        if (encontrada != -1) {
            Chamada c = chamadas[encontrada];
            printf("[Elevador %d] Atendendo chamada: de %d para %d\n\n", id, c.origem, c.destino);
            sleep(1);
            elevadores[id].andar_atual = c.origem;
            sleep(1);
            elevadores[id].andar_atual = c.destino;
            printf("[Elevador %d] Chegou ao destino %d\n\n", id, c.destino);
        } else {
            sem_post(&sem_chamadas); // Devolve semáforo se não for sua vez
            break;
        }
    }
    free(arg);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Uso: %s <n_andares> <m_elevadores> <total_chamadas>\n\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    n_andares = atoi(argv[1]);
    m_elevadores = atoi(argv[2]);
    total_chamadas = atoi(argv[3]);

    pthread_mutex_init(&mutex_chamadas, NULL);
    sem_init(&sem_chamadas, 0, 0);

    pthread_t threads_andares[n_andares];
    pthread_t threads_elevadores[m_elevadores];

    for (int i = 0; i < n_andares; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads_andares[i], NULL, thread_andar, id);
    }

    for (int i = 0; i < m_elevadores; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads_elevadores[i], NULL, thread_elevador, id);
    }

    for (int i = 0; i < n_andares; i++) {
        pthread_join(threads_andares[i], NULL);
    }

    // Posta semáforo extra para garantir que todos elevadores sejam acordados
    for (int i = 0; i < m_elevadores; i++) {
        sem_post(&sem_chamadas);
    }

    for (int i = 0; i < m_elevadores; i++) {
        pthread_join(threads_elevadores[i], NULL);
    }

    pthread_mutex_destroy(&mutex_chamadas);
    sem_destroy(&sem_chamadas);

    printf("\nSimulação finalizada. Todas as chamadas foram atendidas.\n");
    return 0;
}
