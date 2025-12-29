#include "../unix.h"
#include "socketsUtilsServidorSinglePlayer.h"

int main(void)
{
    int serverSocket, clientSocket;
    int childPid;
    int serverAddrSize, clientAddrSize;

    srand(time(NULL));

    struct sockaddr_un serverAddr, clientAddr;

    if ((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Error: Could not open server stream socket");
        exit(1);
    }

    bzero((char *)&serverAddr, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, UNIXSTR_PATH);

    serverAddrSize = strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family);
    unlink(UNIXSTR_PATH);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, serverAddrSize) < 0)
    {
        perror("Error : Server cannot bind to local address");
        exit(1);
    }

    readGamesFromCSV();
    listen(serverSocket, 5);

    while (1)
    {
        clientAddrSize = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);

        if (clientSocket < 0)
        {
            perror("Error : Server could not accept client connection");
            exit(1);
        }

        if ((childPid = fork()) < 0)
        {
            perror("Error : Server could not create child process");
            exit(1);
        }

        else if (childPid == 0)
        {
            close(serverSocket);

            while (1)
                inicializeClientResponseThreads(clientSocket);
        }

        close(clientSocket);
    }
}
