#ifndef CLIENTE_H
#define CLIENTE_H


int* getPossibleAnswers(char partialSolution[81], int rowSelected, int columnSelected , int *size);
// Mostra o Sudoku no ecrã com coordenadas (x, y)
void displaySudokuWithCoords(char sudoku[81]);

void shufflePositionsInArray(int positions[][2] , int count);

// Permite ao utilizador preencher manualmente o Sudoku
void fillSudoku(int socket , char sudoku[81] , int gameId);

void requestGame(int socket , int* gameId , char partialSolution[81]);

#endif
