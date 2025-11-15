#include "../unix.h"
#include "socketsUtilsCliente.h"
#include "../log.h"

int main(void)
{
    int socketServidor;
    int tamanhoEnderecoServidor; 
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

            printf("O cliente escolheu o numero 0\n");
            writeLogf("../Cliente/log_cliente.csv","O cliente escolheu o numero 0");
            requestGame(socketServidor , &gameId , partialSolution);
            displaySudokuWithCoords(partialSolution);
            fillSudoku(socketServidor , partialSolution , gameId);
        }
        else if(escolha == 2) {
            continuar = 0;
            printf("Programa terminado.\n");
            writeLogf("../Cliente/log_cliente.csv","Programa terminado");
        }
    }
    
    close(socketServidor);
    exit(0);
}
