#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

/* ===== DEFINIÇÕES E CONSTANTES ===== */
#define MAX_CHAMADAS 100
#define MAX_ELEVADORES 10
#define MAX_ANDARES 50
#define TAM_BUFFER 10
#define TRUE 1
#define FALSE 0

/* ===== ESTRUTURAS DE DADOS ===== */
// Chamada para elevador
typedef struct {
    int origem;
    int destino;
    int atendida;
} Chamada;

// Elevador
typedef struct {
    int id;
    int andar_atual;
    int ocupado;
    // pthread_mutex_t lock;
} Elevador;


/* ===== VARIÁVEIS GLOBAIS ===== */
// Parâmetros da simulação (definidos pelo usuário)
int n_andares;
int n_elevadores;
int n_chamadas;

// Inicializacao de arrays de chamadas e elevadores
Chamada chamadas[MAX_CHAMADAS];
Elevador elevadores[MAX_ELEVADORES];
int chamadas_pendentes = 0;

// Estruturas de sincronizacao
pthread_mutex_t mutex_chamadas;  // mutex do buffer de chamadas
sem_t sem_chamadas;  // semaforo do buffer de chamadas
sem_t sem_buffer_vazios;  // semaforo das posicoes vazias do buffer
sem_t sem_buffer_ocupados;  // semaforo das posicoes vazias do buffer


/* ===== PADRAO PRODUTOR-CONSUMIDOR ===== */
// PRODUTOR: Thread do andar (gera chamadas aleatórias)
void* thread_andar(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Loop principal de geração de chamadas
    while (TRUE) {
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

// CONSUMIDOR: Thread do elevador (atende às chamadas geradas pelos andares)
void* thread_elevador(void* arg) {
    int id = *(int*)arg;
    free(arg); // Libera a memória alocada para o ID
    
    // Inicializa o estado do elevador
    elevadores[id].id = id;
    elevadores[id].andar_atual = 0;  // Todos elevadores começam no térreo
    
    // Loop de atendimento às chamadas
    while (TRUE) {
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
            printf("[Elevador %d] Chegou ao destino %d\n", id, c.destino);
        } 
        else {
            // Nenhuma chamada para este elevador, devolve semáforo
            sem_post(&sem_chamadas);
            break;
        }

        // Espera aleatória antes de pegar próxima chamada
        sleep(rand() % 3 + 1);
    }
    
    printf("\n[Elevador %d] Encerrado no andar %d\n", id, elevadores[id].andar_atual);
    return NULL;
}


/* ===== PADRÃO SCHEDULER ===== */
// Algoritmo de escalonamento para escolher o elevador livre mais próximo
int escolher_elevador(int origem) {
    int menor_distancia = MAX_ANDARES;
    int escolhidos[MAX_ELEVADORES];
    int contador = 0;

    // Encontra o elevador livre mais próximo
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


/* ===== FUNCAO PRINCIPAL ===== */
int main(int argc, char* argv[])
{
    // Validação dos argumentos de linha de comando
    if (argc != 4) {
        printf("Erro: chamada do programa deve estar no formato %s <n_andares> <n_elevadores> <n_chamadas>\n", argv[0]);
        printf("Exemplo: %s 10 3 20\n", argv[0]);
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

    // Inicio das saidas no terminal
    printf("===== SIMULADOR DE ELEVADORES =====\n");
    printf("%d andares, %d elevadores, %d chamadas\n", n_andares, n_elevadores, n_chamadas);
    sleep(2);

    // Inicialização do semaforo e mutex do buffer de chamadas
    pthread_mutex_init(&mutex_chamadas, NULL);
    sem_init(&sem_chamadas, 0, 0);  // Semáforo inicializado em 0 (buffer vazio)

    // Criação das threads de andares (produtores)
    pthread_t threads_andares[n_andares];
    int ids_andares[n_andares];
    printf("Criando %d threads de andares (produtores)...\n", n_andares);
    for (int i = 0; i < n_andares; i++) {
        ids_andares[i] = i;
        // Checa erro na criacao da thread
        if(pthread_create(&threads_andares[i], NULL, thread_andar, &ids_andares[i]) != 0) {
            perror("Falha na criacao da thread do andar\n");
        }
    }

    // Criação das threads de elevadores (consumidores)
    pthread_t threads_elevadores[n_elevadores];
    int ids_elevadores[n_elevadores];
    printf("Criando %d threads de elevadores (consumidores)...\n", n_elevadores);
    for (int i = 0; i < n_andares; i++) {
        ids_elevadores[i] = i;
        // Checa erro na criacao da thread
        if(pthread_create(&threads_elevadores[i], NULL, thread_elevador, &ids_elevadores[i]) != 0) {
            perror("Falha na criacao da thread do elevador.\n");
        }
    }

    // Aguarda todas as threads de andares terminarem (todas as chamadas geradas)
    for (int i = 0; i < n_andares; i++) {
        if (pthread_join(threads_andares[i], NULL) != 0) {
            perror("Falha no join da thread do andar.");
        }
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
        if (pthread_join(threads_elevadores[i], NULL) != 0) {
            perror("Falha no join da thread do elevador.");
        }
    }
    printf("Todas as threads de elevadores foram encerradas.\n");

    // Libera os recursos de sincronização
    pthread_mutex_destroy(&mutex_chamadas);
    sem_destroy(&sem_chamadas);

    printf("\n===== SIMULAÇÃO FINALIZADA =====\n");

}
