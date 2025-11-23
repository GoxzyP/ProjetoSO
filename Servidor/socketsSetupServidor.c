#include "string.h"
#include "../unix.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"

int main(void)
{
    int socketServidor, socketCliente;
    int pidFilho;
    int tamanhoServidor, tamanhoCliente;

    srand(time(NULL));

    struct sockaddr_un enderecoServidor, enderecoCliente;

    if ((socketServidor = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Erro não foi possível abrir o socket stream do servidor");
        writeLogf("../Servidor/log_servidor.csv","Erro não foi possível abrir o socket stream do servidor");
        exit(1);
    }

    bzero((char *)&enderecoServidor, sizeof(enderecoServidor));
    enderecoServidor.sun_family = AF_UNIX;
    strcpy(enderecoServidor.sun_path, UNIXSTR_PATH);

    tamanhoServidor = strlen(enderecoServidor.sun_path) + sizeof(enderecoServidor.sun_family);
    unlink(UNIXSTR_PATH);

    if (bind(socketServidor, (struct sockaddr *)&enderecoServidor, tamanhoServidor) < 0)
    {
        perror("Servidor não consegue fazer bind do endereço local");
        writeLogf("../Servidor/log_servidor.csv","Servidor não consegue fazer bind do endereço local");
        exit(1);
    }

    readGamesFromCSV();
    listen(socketServidor, 5);

    while (1)
    {
        tamanhoCliente = sizeof(enderecoCliente);
        socketCliente = accept(socketServidor, (struct sockaddr *)&enderecoCliente, &tamanhoCliente);

        if (socketCliente < 0)
        {
            perror("Erro ao aceitar conexão com cliente");
            writeLogf("../Servidor/log_servidor.csv","Erro ao aceitar conexão com cliente");
            exit(1);
        }

        if ((pidFilho = fork()) < 0)
        {
            perror("Erro ao criar processo filho");
            writeLogf("../Servidor/log_servidor.csv","Erro ao criar processo filho");
            exit(1);
        }

        else if (pidFilho == 0)
        {
            close(socketServidor);

            int idCliente = -1;

            while (1)
            {
                char clientRequest[512];

                writeLogf("../Servidor/log_servidor.csv","Está a preparar para ler o pedido do cliente");

                int bytesReceived = lerLinha(socketCliente, clientRequest, sizeof(clientRequest));

                writeLogf("../Servidor/log_servidor.csv","Pedido do cliente foi lido com sucesso");

                if (bytesReceived < 0 || bytesReceived == 0)
                {
                    perror("Error : Servidor não conseguiu receber o pedido do cliente");
                    writeLogf("../Servidor/log_servidor.csv","Servidor não conseguiu receber o pedido do cliente");
                    exit(1);
                }
                
                writeLogf("../Servidor/log_servidor.csv","Pedido do cliente era válido");

                if (idCliente == -1)
                {
                    idCliente = atoi(clientRequest);
                    writeLogf("../Servidor/log_servidor.csv","Cliente ligado com ID: %d", idCliente);
                    continue;
                }

                int codeRequest = atoi(clientRequest);

                writeLogf("../Servidor/log_servidor.csv","O codigo do pedido enviado pelo cliente %d é -> %d", idCliente , codeRequest);

                char *restOfClientRequest = strchr(clientRequest, ',');
                restOfClientRequest++;

                switch (codeRequest)
                {
                    case 1:
                        writeLogf("../Servidor/log_servidor.csv","O pedido do cliente %d é sobre o envio de um novo jogo" , idCliente);
                        sendGameToClient(socketCliente , idCliente);
                        break;

                    case 2:
                    {
                        writeLogf( "../Servidor/log_servidor.csv","O pedido do cliente %d é sobre a verificação de uma resposta" , idCliente);

                        int gameId, rowSelected, columnSelected, clientAnswer;

                        if (sscanf(restOfClientRequest, "%d,%d,%d,%d",
                                   &gameId, &rowSelected, &columnSelected, &clientAnswer) != 4)
                        {
                            perror("Error : O servidor não conseguiu receber os parâmetros necessários para realizar a verificação");
                            writeLogf("../Servidor/log_servidor.csv","O servidor não conseguiu receber os parâmetros necessários para realizar a verificação");
                            exit(1);
                        }

                        writeLogf("../Servidor/log_servidor.csv","O pedido do cliente %d enviou os pârametros de verificação válidos" , idCliente);

                        verifyClientSudokuAnswer(socketCliente, gameId, rowSelected, columnSelected, clientAnswer , idCliente);
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
