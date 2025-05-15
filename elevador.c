#include <pthread.h>
// incluir logica dos elevadores

typedef struct
{
  int id;
  int andar_atual;
  pthread_t thread;
} Elevador;
