#ifndef CLIENTE_H
#define CLIENTE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string.h>

// Funções implementadas em cliente.c

// Estrutura que contém os detalhes necessários de um Jogo Soduku para escrita no ficheiro CSV
typedef struct {
    int idJogo;
    char horaInicio[20];
    char horaFim[20];
    int failedAttempts;
} LogJogo;

// Mostra o Sudoku no ecrã com coordenadas (x, y)
void displaySudokuWithCoords(int sudoku[9][9]);

// Permite ao utilizador preencher manualmente o Sudoku
void fillSudoku(int sudoku[9][9]);

// Fornece a hora atual ( inicio ou fim do jogo )
static void currentTime(char *buffer, size_t size);

// Registra numa certa estrutura LogJogo a hora que iniciou o jogo
static void registerGameStart(LogJogo *log, int id);

// Registra numa certa estrutura LogJogo a hora que terminou o jogo e escreve no ficheiro Log
static void registerGameEndWithLog(LogJogo *log);


#endif
