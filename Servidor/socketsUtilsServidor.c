#include "socketsUtilsServidor.h"  // Inclui definições e estruturas para filas FIFO e buffer produtor/consumidor

// --------------------------------------
// Verifica se a fila está vazia
// --------------------------------------
int isQueueEmpty(queueFifo *queue)
{
    return queue->numberOfItemsInQueue == 0; // Retorna 1 se não houver itens, 0 caso contrário
}

// --------------------------------------
// Verifica se a fila está cheia
// --------------------------------------
int isQueueFull(queueFifo *queue)
{
    return queue->numberOfItemsInQueue == BUFFER_PRODUCER_CONSUMER_SIZE; 
    // Retorna 1 se o número de itens na fila atingir o tamanho máximo do buffer
}

// --------------------------------------
// Inicializa uma fila FIFO
// --------------------------------------
void inicializeQueue(queueFifo *queue)
{
    queue->front = 0;                 // Índice do primeiro elemento
    queue->rear = 0;                  // Índice do último elemento
    queue->numberOfItemsInQueue = 0;  // Inicializa o contador de itens
}

// --------------------------------------
// Adiciona um item à fila
// --------------------------------------
void addItemToQueue(queueFifo *queue, void *item)
{
    queue->items[queue->rear] = item;                                   // Armazena item na posição 'rear'
    queue->rear = (queue->rear + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;    // Incrementa rear circularmente
    queue->numberOfItemsInQueue++;                                       // Incrementa contador de itens
}

// --------------------------------------
// Remove um item da fila
// --------------------------------------
void *removeItemFromQueue(queueFifo *queue)
{
    void *item = queue->items[queue->front];                              // Pega o item na posição 'front'
    queue->front = (queue->front + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;   // Incrementa front circularmente
    queue->numberOfItemsInQueue--;                                        // Decrementa contador de itens
    return item;                                                           // Retorna o item removido
}
