#include "unix.h"
#include <pthread.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../include/uthash.h"

#define BUFFER_PRODUCER_CONSUMER_SIZE 5
#define BUFFER_SOCKET_MESSAGE_SIZE 512
#define maxLineSizeInCsv 165

typedef struct gamedata{
    int id;                          
    char partialSolution[81];       
    char totalSolution[81];         
    UT_hash_handle hh; 
} gameData;

typedef struct QueueEntry
{
    int gameId;
    int rowSelected;
    int columnSelected;
    int clientAnswer;
}queueEntry;

typedef struct QueueFifo
{   
    queueEntry items[BUFFER_PRODUCER_CONSUMER_SIZE];
    int front;
    int rear;
    int numberOfItemsInQueue;
}queueFifo;

gameData *gamesHashTable = NULL;

pthread_mutex_t mutexProducerConsumerQueueFifo; pthread_mutex_t mutexGameData;
pthread_cond_t producerQueueFifoConditionVariable ; pthread_cond_t consumerQueueFifoConditionVariable;

bool isQueueEmpty(queueFifo *queue) {return queue->numberOfItemsInQueue == 0;}
bool isQueueFull(queueFifo *queue) {return queue->numberOfItemsInQueue == BUFFER_PRODUCER_CONSUMER_SIZE;}

void inicializeQueue(queueFifo *queue)
{
    queue->front = 0;
    queue->rear = 0;
    queue->numberOfItemsInQueue = 0;
}

void addItemToQueue(queueFifo *queue , queueEntry item)
{
    queue->items[queue->rear] = item;
    queue->rear = (queue->rear + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
    queue->numberOfItemsInQueue++;
}

queueEntry removeItemFromQueue(queueFifo *queue)
{
    queueEntry itemRemoved = queue->items[queue->front];
    queue->front = (queue->front + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
    queue->numberOfItemsInQueue--;

    return itemRemoved;
}

void readGamesFromCSV()
{
    FILE *file = fopen("../Servidor/jogos.csv", "r");  // Abre CSV para leitura

    if (file == NULL)
    {
        printf("Erro : Não foi possível abrir o ficheiro onde estão armazenados os jogos\n");
        writeLogf("../Servidor/log_servidor.csv","Não foi possível abrir o ficheiro onde estão armazenados os jogos");
        return;
    }

    char line[maxLineSizeInCsv];  // Buffer para ler cada linha do CSV

    while (fgets(line, maxLineSizeInCsv, file))  // Lê linha a linha
    {
        char *dividedLine = strtok(line, ",");  // Separa usando vírgula
        if (dividedLine == NULL) continue;

        gameData *currentGame = malloc(sizeof(gameData));  // Cria a estrutura do jogo
        currentGame->id = atoi(dividedLine);               // Converte ID

        dividedLine = strtok(NULL, ",");  // Sudoku parcial
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->partialSolution, dividedLine);

        dividedLine = strtok(NULL, ",");  // Sudoku solução total
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->totalSolution, dividedLine);

        HASH_ADD_INT(gamesHashTable, id, currentGame);
    }

    fclose(file);
}

void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer)
{   
    gameData *gameSentToClient;

    pthread_mutex_lock(&mutexGameData);
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient);
    pthread_mutex_unlock(&mutexGameData);

    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    if (!gameSentToClient)
        strcpy(messageToClient , "Error : Server could not find a game with the id sent from the client\n");
    
    else if (rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8)
        strcpy(messageToClient , "Error : Server could not process the sudoku coordinates sent from the client\n");

    else
        gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0' ? strcpy(messageToClient, "Correct\n") : strcpy(messageToClient, "Wrong\n");

    if (writeSocket(socket, messageToClient, strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client or the client disconnected from the socket");
        exit(1);
    }
}

void producer(int socket , queueFifo *queue)
{   
    while(1)
    {
        char messageFromClient[BUFFER_SOCKET_MESSAGE_SIZE];
        int bytesReceived = readSocket(socket , messageFromClient , sizeof(messageFromClient));

        if(bytesReceived < 0)
        {
            perror("Error : Server could not read the answer of the client or the client disconnected from the socket");
            exit(1);
        }

        queueEntry queueFifoEntry;

        if(sscanf(messageFromClient , "%d,%d,%d,%d" , &queueFifoEntry.gameId , &queueFifoEntry.rowSelected , &queueFifoEntry.columnSelected , &queueFifoEntry.clientAnswer) != 4)
        {
            char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];
            strcpy(messageToClient , "Error : Server could not process the client answer\n");

            if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
            {
                perror("Error : Server could not write error message regarding processing the cliente answer or the client disconnected from the socket");
                exit(1);
            }

            continue;
        }

        pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

        while(isQueueFull(queue) == true)
            pthread_cond_wait(&producerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);

        addItemToQueue(queue , queueFifoEntry);

        pthread_cond_signal(&consumerQueueFifoConditionVariable);
        pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);
    }
}

void consumer(int socket , queueFifo *queue)
{
    while(1)
    {
        pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

        while(isQueueEmpty(queue) == true)
            pthread_cond_wait(&consumerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);

        queueEntry item = removeItemFromQueue(queue);

        pthread_cond_signal(&producerQueueFifoConditionVariable);
        pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

        verifyClientSudokuAnswer(socket , item.gameId , item.rowSelected , item.columnSelected , item.clientAnswer);
    }
}