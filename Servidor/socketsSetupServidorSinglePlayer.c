#include "../unix.h"  // Inclui definições e constantes para sockets UNIX
#include "../util.h"  // Inclui funções utilitárias, como writeSocket
#include "../log.h"   // Para sistema de logs

#include "socketsUtilsServidorSinglePlayer.h" // Inclui funções e estruturas específicas do servidor single player
#include "servidorGameManager.h"                 // Inclui funções de gerenciamento de jogos

#include <stdio.h>      // Inclui funções de input/output padrão (printf, perror)
#include <stdlib.h>     // Inclui funções padrão (exit, malloc, srand, rand)
#include <string.h>     // Inclui funções de manipulação de strings (strcpy, bzero)
#include <pthread.h>    // Inclui threads POSIX
#include <sys/epoll.h>  // Inclui funções epoll para multiplexação de sockets


int epollFd; // Descriptor global do epoll

int main(void)
{
    int serverSocket;               // Socket do servidor
    struct sockaddr_un serverAddr;  // Estrutura do endereço do socket UNIX

    srand(time(NULL));              // Inicializa gerador de números aleatórios

    // Carrega jogos do CSV para a hash table
    readGamesFromCSV();
    
    // Inicializa sistema de estatísticas
    initStatsSystem();

    // Inicializa filas de tarefas
    inicializeQueue(&normalQueue);
    inicializeQueue(&priorityQueue);

    // Inicializa mutexes e variáveis de condição
    pthread_mutex_init(&mutexProducerConsumerQueueFifo, NULL);
    pthread_mutex_init(&mutexGameData, NULL);
    pthread_mutex_init(&mutexMultiplayerRoom, NULL);
    pthread_cond_init(&producerQueueFifoConditionVariable, NULL);
    pthread_cond_init(&consumerQueueFifoConditionVariable, NULL);

    // Cria o socket do servidor
    if ((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Error: Could not open server socket"); // Mensagem de erro
        exit(1);                                        // Sai do programa
    }

    bzero(&serverAddr, sizeof(serverAddr));           // Zera a estrutura do endereço
    serverAddr.sun_family = AF_UNIX;                  // Define tipo de socket UNIX
    strcpy(serverAddr.sun_path, UNIXSTR_PATH);        // Define o caminho do socket
    unlink(UNIXSTR_PATH);                             // Remove socket antigo se existir

    // Faz o bind do socket
    if (bind(serverSocket,
             (struct sockaddr *)&serverAddr,
             strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family)) < 0)
    {
        perror("Error: bind failed"); // Mensagem de erro
        exit(1);                      // Sai do programa
    }

    listen(serverSocket, 10); // Coloca o socket em modo de escuta (backlog 10)

    epollFd = epoll_create1(0); // Cria epoll descriptor

    // Cria threads consumidoras
    for (int i = 0; i < NUMBER_OF_CONSUMERS; i++)
        pthread_create(&consumers[i], NULL, consumer, NULL);

    // Cria thread de IO
    pthread_create(&ioThread, NULL, ioHandler, NULL);

    // Loop principal: aceita clientes
    while (1)
    {
        int clientSocket = accept(serverSocket, NULL, NULL); // Aceita novo cliente
        
        // Incrementa contador de clientes ativos
        incrementActiveClients();
        int clientesAtivos = getActiveClientsCount();
        printf("Novo cliente conectado. Clientes ativos: %d\n", clientesAtivos);
        writeLogf("../Servidor/log_servidor.txt", "Novo cliente conectado (socket %d). Total de clientes ativos: %d", clientSocket, clientesAtivos);

        struct epoll_event ev;       // Estrutura para registro no epoll
        ev.events = EPOLLIN;         // Monitora eventos de leitura
        ev.data.fd = clientSocket;   // Armazena descriptor do cliente

        epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &ev); // Adiciona socket ao epoll
    }
}
