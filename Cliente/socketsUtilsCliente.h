#ifndef CLIENTE_H
#define CLIENTE_H

//Pthread.h é onde está definido as funções de manipulação de threads
#include <pthread.h>

//BUFFER_SOCKET_MESSAGE_SIZE define o tamanho máximo de mensagems que podem passar pelos sockets
#define BUFFER_SOCKET_MESSAGE_SIZE 512

//Enum que define o estado das células do sudoku
//IDLE - Célula não preenchida ou tentativa gerada pelo produtor está errada
//WAITING - Célula a aguardar a validação de resposta gerada pelo consumidor
//FAILED - Erro na comunicação com o servidor sobre a tentativa gerada pelo produtor fazendo ele reenviar a mesma tentativa para o buffer
//FINISHED - Célula preenchida
enum cellState 
{
    IDLE ,
    WAITING ,
    FAILED ,
    FINISHED
};

//SudokuCell define a estrutura de cada célula do sudoku
//Value - O valor da célula , 0 caso esta estiver por preencher ou outro valor de 1 a 9 caso tiver preenchida
//Producer - Indica a thread produtora que está a lidar com a célula
//Mutex - Irá proteger o acesso da célula
//ConditionVariable - Irá servir para sinalizar a mudança do estado da célula
//State - Explicado acima no enum
typedef struct SudokuCell 
{
    int value;
    pthread_t producer;
    pthread_mutex_t mutex;
    pthread_cond_t conditionVariable;
    enum cellState state;
}sudokuCell;

void displaySudokuWithCoords(sudokuCell (*sudoku)[9][9]);
void shufflePositionsInArray(int positions[][2] , int count);
int* getPossibleAnswers(sudokuCell (*sudoku)[9][9] , int rowSelected, int columnSelected , int *size);
void requestGame(int socket , int *gameId , char partialSolution[81]);

#endif
