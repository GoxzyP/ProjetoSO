#ifndef CLIENTE_H
#define CLIENTE_H



// Mostra o Sudoku no ecrã com coordenadas (x, y)
void displaySudokuWithCoords(char sudoku[81]);

// Permite ao utilizador preencher manualmente o Sudoku
void fillSudoku(int socket , char sudoku[81] , int gameId , char logPath[256]);

void requestGame(int socket , int* gameId , char partialSolution[81] , char logPath[256]);

#endif
