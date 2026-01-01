#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <semaphore.h>

// Estrutura que representa um jogo do Sudoku
typedef struct gamedata{
    int id;                          // identificador do jogo
    char partialSolution[81];       // array bidimensional com a solução parcial (jogo inicial)
    char totalSolution[81];         // array bidimensional com a solução completa
    UT_hash_handle hh; 
} gameData;

// Estrutura de dados compartilhada entre processos
typedef struct {
    // Padrão Leitor/Escritor
    sem_t mutexLeitores;      // Protege numLeitores
    sem_t mutexEscritores;    // Escritor exclusivo
    int numLeitores;          // Contador de leitores ativos
    
    int clientesAtivos;
    int clienteIds[1000];
    // Estatísticas globais acumuladas
    int totalAcertos;
    int totalErros;
    int totalJogos;
    int totalJogadas;
    // Estatísticas por cliente (arrays paralelos a clienteIds)
    int clienteAcertos[1000];
    int clienteErros[1000];
    int clienteJogos[1000];
    int clienteOnline[1000];  // 1 = online, 0 = desconectado
} SharedMemory;

// Estrutura para estatísticas por cliente
typedef struct clientstats {
    int idCliente;
    int acertos;
    int erros;
    int jogos;
    UT_hash_handle hh;
} ClientStatsServer;

// Estrutura para estatísticas globais do servidor
typedef struct {
    int totalAcertos;
    int totalErros;
    int totalJogos;
    int totalClientes;
} ServerGlobalStats;

// Funções implementadas em socketUtils.c
void readGamesFromCSV();
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId, ClientStatsServer **clientsHash, ServerGlobalStats *globalStats);
void sendGameToClient(int socket , int clientId, ClientStatsServer **clientsHash, ServerGlobalStats *globalStats);
void writeServerStats(ServerGlobalStats *globalStats, ClientStatsServer *clientsHash);
void updateRealtimeStats();
void initSharedMemory();
void cleanupSharedMemory();
void incrementActiveClients();
void decrementActiveClients();
int getActiveClientsCount();
void registerUniqueClient(int clientId);
void unregisterClientOnline(int clientId);

#endif
