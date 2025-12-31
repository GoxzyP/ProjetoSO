#ifndef CLIENTE_H
#define CLIENTE_H

void inicializeGame(int socket , int gameId , char partialSolution[81]);
void requestGame(int socket , int *gameId , char partialSolution[82]);

#endif