#ifndef CLIENTE_H
#define CLIENTE_H

//Pthread.h é onde está definido as funções de manipulação de threads
#include <pthread.h>

//BUFFER_SOCKET_MESSAGE_SIZE define o tamanho máximo de mensagems que podem passar pelos sockets
#define BUFFER_SOCKET_MESSAGE_SIZE 512

// Estrutura para estatísticas do cliente
typedef struct {
    int totalAcertos;
    int totalErros;
    int totalJogadas;
    int totalJogos;
    int totalCelulasPreenchidas;
} ClientStats;

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
    enum cellState state;
}sudokuCell;

void displaySudokuWithCoords(void *sudoku , int (*getValueFunction)(void* , int , int));
void shufflePositionsInArray(int positions[][2] , int count);
int getValueFromSudokuCell(void *sudoku , int rowSelected , int columnSelected);
int getValueFromCharGrid(void *sudoku , int rowSelected , int columnSelected);
int* getPossibleAnswers(void *sudoku , int rowSelected, int columnSelected , int *size , int (*getValueFunction)(void* , int , int));
void requestGame(int socket , int *gameId , char partialSolution[81], char logPath[256]);
void fillSudoku(int socket, char sudoku[81], int gameId, char logPath[256], ClientStats *stats, int clientId);
void writeClientStats(ClientStats *stats, char logPath[256], int clientId);

#endif
