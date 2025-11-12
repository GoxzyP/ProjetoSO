#include "string.h"

#include "../unix.h"
#include "../util.h"
#include "socketsUtilsServidor.h"


int main(void)
{   
    int socketServidor , socketCliente;
    int pidFilho;
    int tamanhoServidor , tamanhoCliente;

    srand(time(NULL));

    struct sockaddr_un enderecoServidor , enderecoCliente;

    if((socketServidor = socket(AF_UNIX , SOCK_STREAM , 0)) < 0)
    {
        perror("Erro não foi possível abrir o socket stream do servidor");
        exit(1);
    }

    bzero((char *)&enderecoServidor , sizeof(enderecoServidor));
    enderecoServidor.sun_family = AF_UNIX;
    strcpy(enderecoServidor.sun_path , UNIXSTR_PATH);

    tamanhoServidor = strlen(enderecoServidor.sun_path) + sizeof(enderecoServidor.sun_family);
    unlink(UNIXSTR_PATH);

    if(bind(socketServidor , (struct sockaddr *)&enderecoServidor , tamanhoServidor) < 0)
    {
        perror("Servidor não consegue fazer bind do endereço local");
        exit(1);
    }

    readGamesFromCSV();
    listen(socketServidor , 5);

    while(1)
    {   
        tamanhoCliente = sizeof(enderecoCliente);
        socketCliente = accept(socketServidor , (struct sockaddr *)&enderecoCliente , &tamanhoCliente);

        if(socketCliente < 0)
        {
            perror("Erro ao aceitar conexão com cliente");
            exit(1);
        }

        if((pidFilho = fork()) < 0)
        {
            perror("Erro ao criar processo filho");
            exit(1);
        }
        else if(pidFilho == 0)
        {   
            close(socketServidor);

            while(1)
            {
                char clientRequest[512];
                printf("Está a preparar para ler a linha\n");
                int bytesReceived = lerLinha(socketCliente , clientRequest , sizeof(clientRequest));

                printf("Conseguiu ler o request do cliente\n");
                
                if(bytesReceived < 0 || bytesReceived == 0)
                {
                    perror("Error : Server could not receive request from client");
                    exit(1);
                }

                printf("Request do cliente era válido\n");

                int codeRequest = atoi(clientRequest);

                printf("O codigo enviado do cliente era %d \n" , codeRequest);

                char *restOfClientRequest = strchr(clientRequest , ',');
                restOfClientRequest++;

                switch (codeRequest)
                {
                    case 1 :
                        printf("Entrou no case 1\n");
                        sendGameToClient(socketCliente);
                        break;

                    case 2 :
                    {   
                        printf("Entrou no case 2\n");

                        int gameId , rowSelected , columnSelected , clientAnswer;

                        if(sscanf(restOfClientRequest , "%d,%d,%d,%d" , &gameId , &rowSelected , &columnSelected , &clientAnswer) != 4)
                        {
                            perror("Error : Failed to parse client request");
                            exit(1);
                        }

                        printf("Conseguiu receber os parametros para verificar erro direitinhos\n");

                        verifyClientSudokuAnswer(socketCliente , gameId , rowSelected , columnSelected , clientAnswer);
                        break;
                    }
                    
                    default:
                        break;
                }
            }
        }

        close(socketCliente);
    }

}