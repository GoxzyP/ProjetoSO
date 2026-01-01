#ifndef CLIENTE_H
#define CLIENTE_H

// Estrutura para estatísticas do cliente
typedef struct {
    int totalAcertos;
    int totalErros;
    int totalJogadas;
    int totalJogos;
    int totalCelulasPreenchidas;
} ClientStats;

// Mostra o Sudoku no ecrã com coordenadas (x, y)
void displaySudokuWithCoords(char sudoku[81]);

// Permite ao utilizador preencher manualmente o Sudoku
void fillSudoku(int socket , char sudoku[81] , int gameId , char logPath[256], ClientStats *stats);

void requestGame(int socket , int* gameId , char partialSolution[81] , char logPath[256]);

// Escreve as estatísticas do cliente no log
void writeClientStats(ClientStats *stats, char logPath[256], int idCliente);

#endif
