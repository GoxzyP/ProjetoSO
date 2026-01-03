#ifndef SOCKETS_UTILS_SERVIDOR_H
#define SOCKETS_UTILS_SERVIDOR_H


// Define o tamanho máximo do buffer para a fila produtor/consumidor
#define BUFFER_PRODUCER_CONSUMER_SIZE 5

// --------------------------------------
// Estrutura da fila FIFO
// --------------------------------------
typedef struct QueueFifo
{
    void *items[BUFFER_PRODUCER_CONSUMER_SIZE]; // Array que armazena os itens da fila
    int front;                                  // Índice do primeiro item na fila
    int rear;                                   // Índice da posição seguinte ao último item
    int numberOfItemsInQueue;                   // Número de itens atualmente na fila
} queueFifo;

// --------------------------------------
// Declarações das funções para manipular a fila
// --------------------------------------
int isQueueEmpty(queueFifo *queue);          // Retorna 1 se a fila estiver vazia, 0 caso contrário
int isQueueFull(queueFifo *queue);           // Retorna 1 se a fila estiver cheia, 0 caso contrário
void inicializeQueue(queueFifo *queue);      // Inicializa os índices e contador da fila
void addItemToQueue(queueFifo *queue, void *item); // Adiciona um item ao final da fila
void *removeItemFromQueue(queueFifo *queue);       // Remove e retorna o item do início da fila

#endif
