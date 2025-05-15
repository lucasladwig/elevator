#include <stdio.h>
#include <stdlib.h>
#include "controlador.h"
#include "elevador.h"

#define TRUE 1
#define FALSE 0

int parametros_sao_validos(int num_andares, int num_elevadores, int num_chamadas)
{
    if (num_andares < 2 || num_elevadores < 2 || num_chamadas < 2)
    {
        printf("Erro: todos os parametros devem ser maiores que 2.\n");
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Uso: %s <num_andares> <num_elevadores> <num_chamadas>\n", argv[0]);
        return 1;
    }
    int num_andares = atoi(argv[1]);
    int num_elevadores = atoi(argv[2]);
    int num_chamadas = atoi(argv[3]);

    if (!parametros_sao_validos(num_andares, num_elevadores, num_chamadas))
        return 1;

    printf("Inicializando simulação com %d andares, %d elevadores e %d chamadas.\n", num_andares, num_elevadores, num_chamadas);

    // inserir logica das threads
}