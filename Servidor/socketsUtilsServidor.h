#ifndef SERVIDOR_H
#define SERVIDOR_H

// Estrutura que representa um jogo do Sudoku
typedef struct gamedata{
    int id;                          // identificador do jogo
    char partialSolution[81];       // array bidimensional com a solução parcial (jogo inicial)
    char totalSolution[81];         // array bidimensional com a solução completa
    UT_hash_handle hh; 
} gameData;

// Funções implementadas em socketUtils.c
void readGamesFromCSV();
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId);
void sendGameToClient(int socket , int clientId);

#endif
