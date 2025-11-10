#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <stdio.h>   // Bibliotecas para operações de input/output
#include <string.h>  // Para manipulação de strings (strtok, etc)
#include <stdlib.h>  // Para funções gerais (malloc, atoi, etc)
#include "../include/uthash.h"

// Estrutura que representa um jogo do Sudoku
typedef struct gamedata{
    int id;                          // identificador do jogo
    int partialSolution[9][9];       // array bidimensional com a solução parcial (jogo inicial)
    int totalSolution[9][9];         // array bidimensional com a solução completa
    UT_hash_handle hh; 
} gameData;

// Funções implementadas em servidor.c
void readGamesFromCSV();
char* verifyClientSudokuAnswer(int clientID , int gameId , int rowSelected , int columnSelected , int clientAnswer);
gameData* sendGameToClient(int clientId);

#endif
