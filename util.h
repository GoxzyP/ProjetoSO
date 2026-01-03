#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>   // Bibliotecas para operações de input/output
#include <string.h>  // Para manipulação de strings (strtok, etc)
#include <stdlib.h>  // Para funções gerais (malloc, atoi, etc)
#include "../include/uthash.h"

// Funções implementadas em socketUtils.c
int writeSocket(int socket , char *buffer , int numeroBytes);
int readSocket(int socket, char *buffer, int tamanhoMaximo);

#endif