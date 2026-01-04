#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "../include/uthash.h" // <--- isto garante que UT_hash_handle é conhecido
#include <pthread.h>

// Estrutura que representa um jogo do Sudoku
typedef struct gamedata{
    int id;                          // identificador do jogo
    char partialSolution[81];       // array bidimensional com a solução parcial (jogo inicial)
    char totalSolution[81];         // array bidimensional com a solução completa
    UT_hash_handle hh; 
} gameData;


// Estrutura que representa um pedido de validação enviado por um cliente
typedef struct PedidoValidacao {
    int idPedido;          // ID único do pedido (incrementado globalmente)
    int socketCliente;     // Socket do cliente que enviou o pedido
    int clientId;          // ID do cliente que enviou o pedido
    int gameId;            // ID do jogo Sudoku ao qual o pedido pertence
    int row;               // Linha da célula do Sudoku que está sendo verificada
    int col;               // Coluna da célula do Sudoku que está sendo verificada
    int answer;            // Valor proposto pelo cliente para a célula
    struct PedidoValidacao *next; // Ponteiro para o próximo pedido na fila (para formar lista encadeada)
} PedidoValidacao;


// Estrutura que representa uma fila de pedidos de validação
typedef struct {
    PedidoValidacao *head; // Ponteiro para o primeiro pedido da fila
    PedidoValidacao *tail; // Ponteiro para o último pedido da fila
} FilaPedidos;


#define MAX_CLIENTES_SALA 3

typedef struct {
    int socket;
    int clientId;
} ClienteSala;

typedef struct {
    int gameId;
    char sudoku[81];        // estado atual
    char solution[81];      // solução completa
    int emptyCells;         // quantas células faltam

    ClienteSala clientes[MAX_CLIENTES_SALA];
    int numClientes;
    
    pthread_mutex_t mutex;
    pthread_barrier_t barrier;
    int barrierInicializada;
} Sala;

// Funções implementadas em socketUtils.c
void readGamesFromCSV();
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId);
void sendGameToClient(int socket, int clientId,int numeroJogadoresSala);
void enqueuePedido(PedidoValidacao *p);
PedidoValidacao* dequeuePedido();
void* workerValidacoes(void *arg);


#endif
