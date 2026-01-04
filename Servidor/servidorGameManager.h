#ifndef SERVIDOR_GAME_MANAGER_H
#define SERVIDOR_GAME_MANAGER_H

#include "../include/uthash.h" // Inclui a biblioteca UTHash para criar hash tables
#include <pthread.h>           // Inclui pthreads para usar mutexes

#define BUFFER_SOCKET_MESSAGE_SIZE 512 // Define o tamanho máximo de mensagens enviadas pelo socket

// Forward declaration das estruturas de stats
typedef struct ClientStatsServer ClientStatsServer;
typedef struct ServerGlobalStats ServerGlobalStats;


// Estrutura que representa os dados de um jogo
typedef struct GameData
{
    int id;                          // ID do jogo
    char partialSolution[82];        // Solução parcial do jogo (81 células + '\0')
    char totalSolution[82];          // Solução completa do jogo (81 células + '\0')
    UT_hash_handle hh;               // Handle usado pela biblioteca UTHash para hash table
} gameData;

// Declaração externa da hash table de jogos
extern gameData *gamesHashTable;
// Declaração externa do mutex para proteger acesso à hash table
extern pthread_mutex_t mutexGameData;

// Função que lê os jogos do CSV e popula a hash table
void readGamesFromCSV();
// Função que envia um jogo aleatório para o cliente via socket
void sendGameToClient(int socket, int clientId);
// Função que verifica a solução parcial do cliente
void verifyClientPartialSolution(int socket, int gameId, int row, int col, int value, int clientId);
// Função que verifica a solução completa do cliente
void verifyClientCompleteSolution(int socket, int gameId, char solution[82], int clientId);

#endif

