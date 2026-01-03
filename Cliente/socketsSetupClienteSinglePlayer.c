#include "../unix.h"
#include <time.h>

#include "socketsUtilsCliente.h"
#include "socketsUtilsClienteSinglePlayer.h"
#include "singlePlayerSolucaoCompleta.h"
#include "../log.h"

// Stats globais definidas em socketsUtilsClienteSinglePlayer.c
extern ClientStats clientStats;

int main(void)
{
    int serverSocket;
    int serverAddressLength;
    struct sockaddr_un serverAddress;

    srand(time(NULL));

    if((serverSocket = socket(AF_UNIX , SOCK_STREAM , 0)) < 0)
    {
        perror("Error: could not open client stream socket");
        exit(1);
    }

    bzero((char *)&serverAddress , sizeof(serverAddress));
    serverAddress.sun_family = AF_UNIX;
    strcpy(serverAddress.sun_path , UNIXSTR_PATH);
    serverAddressLength = strlen(serverAddress.sun_path) + sizeof(serverAddress.sun_family);

    if((connect(serverSocket , (struct sockaddr *)  &serverAddress, serverAddressLength)) < 0)
    {
        perror("Error: client cannot connect to the server");
        exit(1);
    }

    int clientId = getpid(); // Usar PID como ID do cliente
    char logPath[256];
    snprintf(logPath, sizeof(logPath), "../Cliente/log_cliente%d.txt", clientId);

    int partialSolutionMode = rand() % 2;

    while(1) 
    {
        int gameId;
        char partialSolution[82];

        requestGame(serverSocket , &gameId , partialSolution, logPath, clientId);

        printf("Cliente %d: Recebeu o jogo com o id -> %d\n" , clientId, gameId);
        printf("Cliente %d: Recebeu o jogo com a solução parcial %s\n" , clientId, partialSolution);
        writeLogf(logPath, "Cliente %d: Recebeu o jogo com o id -> %d", clientId, gameId);
        writeLogf(logPath, "Cliente %d: Recebeu o jogo com a solução parcial %s", clientId, partialSolution);

        if(partialSolutionMode == 1)
            inicializeGame(serverSocket , gameId , partialSolution, clientId, logPath);

        else
            solveSudokuUsingCompleteSolution(serverSocket , gameId , partialSolution, clientId, logPath);
        
        // Escrever estatísticas após cada jogo
        writeClientStats(&clientStats, logPath, clientId);
        printf("\n=== Estatísticas atualizadas: Cliente %d, Jogos: %d, Acertos: %d, Erros: %d ===\n\n", 
               clientId, clientStats.totalJogos, clientStats.totalAcertos, clientStats.totalErros);
        writeLogf(logPath, "Estatísticas atualizadas: Cliente %d, Jogos: %d, Acertos: %d, Erros: %d", 
               clientId, clientStats.totalJogos, clientStats.totalAcertos, clientStats.totalErros);
        
        break; // Sair após completar um jogo
    }
    
    close(serverSocket);
    exit(0);
}
