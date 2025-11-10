#include "../unix.h"
#include "utilsCliente.h"

int main(void)
{
    int socketServidor;
    int tamanhoEnderecoServidor; 
    struct sockaddr_un enderecoServidor;

    if((socketServidor = socket(AF_UNIX , SOCK_STREAM , 0)) < 0)
    {
        perror("Erro não foi possível abrir o socket stream do cliente");
        exit(1);
    }

    bzero((char *)&enderecoServidor , sizeof(enderecoServidor));
    enderecoServidor.sun_family = AF_UNIX;
    strcpy(enderecoServidor.sun_path , UNIXSTR_PATH);
    tamanhoEnderecoServidor = strlen(enderecoServidor.sun_path) + sizeof(enderecoServidor.sun_family);

    if((connect(socketServidor , (struct sockaddr *)  &enderecoServidor, tamanhoEnderecoServidor)) < 0)
    {
        perror("Erro cliente não consegue conectar-se ao servidor");
        exit(1);
    }

    close(socketServidor);
    exit(0);
}