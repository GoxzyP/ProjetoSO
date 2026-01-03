#ifndef SOCKETS_UTILS_SERVIDOR_SINGLEPLAYER_H
#define SOCKETS_UTILS_SERVIDOR_SINGLEPLAYER_H

#include "socketsUtilsServidor.h"
#include "../include/uthash.h"
#include "pthread.h" // Para threads, mutex e cond vars

// Define o número de threads consumidoras
#define NUMBER_OF_CONSUMERS 10

// Estrutura para estatísticas por cliente
typedef struct ClientStatsServer {
    int idCliente;
    int acertos;
    int erros;
    int jogos;
    UT_hash_handle hh;
} ClientStatsServer;

// Estrutura para estatísticas globais do servidor
typedef struct ServerGlobalStats {
    int totalAcertos;
    int totalErros;
    int totalJogos;
    int totalClientes;
    int clientesAtivos;
} ServerGlobalStats;

// Estrutura que representa um pedido parcial de solução do cliente
typedef struct QueueEntry
{
    int gameId;        // ID do jogo
    int rowSelected;   // Linha escolhida pelo cliente
    int columnSelected; // Coluna escolhida pelo cliente
    int clientAnswer;  // Valor que o cliente tentou colocar
    int clientId;      // ID do cliente
}queueEntry;

// Estrutura para armazenar solução completa enviada pelo cliente
typedef struct PriorityQueueEntry
{
    int gameId;                        // ID do jogo
    char clientCompleteSolution[82];   // Solução completa enviada pelo cliente
    int clientId;                      // ID do cliente
}priorityQueueEntry;

// Estrutura que representa uma mensagem do cliente
typedef struct ClientRequest
{
    int socket;     // Socket do cliente que enviou a mensagem
    int requestType; // Tipo do pedido (1 = parcial, 2 = completo)
    union {
        queueEntry partialSolution;       // Dados para pedido parcial
        priorityQueueEntry completeSolution; // Dados para pedido completo
    } data;
}clientRequest;

// Filas globais para pedidos normais e prioritários
extern queueFifo normalQueue;
extern queueFifo priorityQueue;

// Mutex e variáveis de condição para sincronização produtor/consumidor
extern pthread_mutex_t mutexProducerConsumerQueueFifo;
extern pthread_cond_t producerQueueFifoConditionVariable;
extern pthread_cond_t consumerQueueFifoConditionVariable;

// Arrays de threads globais
extern pthread_t consumers[];
extern pthread_t ioThread;

// Funções das threads
void *ioHandler(void *);
void *consumer(void *);

// Funções de gestão de estatísticas
void writeServerStats();
void incrementActiveClients();
void decrementActiveClients();
int getActiveClientsCount();
void initStatsSystem();
void handleClientDisconnection(int socket);

#endif
