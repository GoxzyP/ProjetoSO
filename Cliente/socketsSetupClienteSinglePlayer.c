#include "../unix.h"

#include "socketsUtilsClienteSinglePlayer.h"
#include "singlePlayerSolucaoCompleta.h"

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

    int partialSolutionMode = 0;

    while(1) 
    {
        int gameId;
        char partialSolution[82];

        requestGame(serverSocket , &gameId , partialSolution);

        printf("Recebeu o jogo com o id -> %d\n" , gameId);
        printf("Recebeu o jogo com a solução parcial %s\n" , partialSolution);

        if(partialSolutionMode == 1)
            inicializeGame(serverSocket , gameId , partialSolution);

        else
            solveSudokuUsingCompleteSolution(serverSocket , gameId , partialSolution);
    }
    
    close(serverSocket);
    exit(0);
}
