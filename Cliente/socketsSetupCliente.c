#include "../unix.h"
#include "socketsUtilsCliente.h"
#include "../util.h"
#include "../log.h"

// Adicionei o id do cliente nos logs

int main(void)
{
    int socketServidor;
    int tamanhoEnderecoServidor;
    int idCliente = getpid();
    struct sockaddr_un enderecoServidor;

    srand(time(NULL));

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

    int continuar = 1;

    char logPath[512];
    snprintf(logPath, sizeof(logPath), "../Cliente/log_cliente%d.csv", idCliente);

    char messageToServer[512];
    int positionInMessageToServer= sprintf(messageToServer, "%d,", idCliente);
    messageToServer[positionInMessageToServer++] = '\n';

    if((escreveSocket(socketServidor , messageToServer , strlen(messageToServer))) != strlen(messageToServer))
    {
        perror("Erro : Não foi possível enviar o id do cliente para o servidor");
        writeLogf(logPath,"Não foi possível enviar o id do cliente para o servidor");
        exit(1);
    }

    writeLogf(logPath,"O id do cliente %d foi enviado para o servidor",idCliente);

    while(continuar) 
    {
        int escolha;
        do 
        {
            printf("\nEscolha uma opcao:\n");
            printf("0 = Jogar\n");
            printf("2 = Sair\n");
            printf("Opcao: ");
            scanf("%d", &escolha);
        } 
        while(escolha != 0 && escolha != 1 && escolha != 2);

        if(escolha == 0) 
        {
            int gameId;
            char partialSolution[81];

            writeLogf(logPath,"O cliente %d comecou a jogar",idCliente);
            requestGame(socketServidor , &gameId , partialSolution , logPath);
            displaySudokuWithCoords(partialSolution);
            fillSudoku(socketServidor , partialSolution , gameId , logPath);
        }
        else if(escolha == 2) {
            continuar = 0;
            printf("Programa terminado.\n");
            writeLogf(logPath,"O cliente %d se desconectou",idCliente);
        }
    }
    
    close(socketServidor);
    exit(0);
}
