#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

/* ===== DEFINIÇÕES E CONSTANTES ===== */
#define MAX_CHAMADAS 100
#define MAX_ELEVADORES 10
#define MAX_ANDARES 50
#define TAM_BUFFER 10
#define TRUE 1
#define FALSE 0
#define THREAD_NUM 8

sem_t semEmpty;
sem_t semFull;

pthread_mutex_t mutexBuffer;


/* ===== ESTRUTURAS DE DADOS ===== */
// Chamada para elevador
typedef struct {
    int origem;
    int destino;
    int atendida;
} Chamada;

// Elevador
typedef struct {
    int id;10
    int andar_atual;
    int ocupado;
    // pthread_mutex_t lock;
} Elevador;

Chamada buffer[TAM_BUFFER];
int contador_buffer = 0;

/* ===== PADRAO PRODUTOR-CONSUMIDOR ===== */
// PRODUTOR: Thread do andar (gera chamadas aleatórias)
void* thread_andar(void* arg) {
    
    int id = *(int*)arg;
    
    while (1) {
        // Acesso a regiao critica (buffer)
        sem_wait(&semEmpty); // aguarda pelo menos uma posicao livre no buffer
        pthread_mutex_lock(&mutexBuffer); // adquire tranca do buffer

        // Criar nova chamada com origem neste andar e destino aleatório
        Chamada c;
        c.origem = id;
        do {
            c.destino = rand() % n_andares;
        } while (c.destino == c.origem); // Garante que destino seja diferente da origem
        c.atendida = FALSE;

        // Adiciona chamada ao buffer
        buffer[contador_buffer] = c;
        contador_buffer++;
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);
    }
}

void* consumer(void* args) {
    while (1) {
        int y;

        // Remove from the buffer
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        y = buffer[count - 1];
        count--;
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semEmpty);

        // Consume
        printf("Got %d\n", y);
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexBuffer, NULL);
    sem_init(&semEmpty, 0, TAM_BUFFER);
    sem_init(&semFull, 0, 0);
    int i;
    for (i = 0; i < THREAD_NUM; i++) {
        if (i > 0) {
            if (pthread_create(&th[i], NULL, &thread_andar, NULL) != 0) {
                perror("Failed to create thread");
            }
        } else {
            if (pthread_create(&th[i], NULL, &consumer, NULL) != 0) {
                perror("Failed to create thread");
            }
        }
    }
    for (i = 0; i < THREAD_NUM; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }
    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}