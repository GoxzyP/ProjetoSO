#include "../unix.h"
#include "utilsServidor.h"
#

int main(void)
{
    int socketServidor , socketCliente;
    int pidFilho;
    int tamanhoServidor , tamanhoCliente;

    struct sockaddr_un enderecoServidor , enderecoCliente;

    if(socketServidor = socket(AF_UNIX , SOCK_STREAM , 0) < 0)
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
        }

        close(socketCliente);
    }

}