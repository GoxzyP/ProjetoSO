#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h> // pthreads (threads)
#include <sys/socket.h>
#include <sys/un.h>

#include "../unix.h"
#include "socketsUtilsServidor.h"
#include "../log.h"

#define MAX_CLIENTES 10

// Número de jogadores necessários por sala/jogo
#define JOGADORES_POR_SALA 3

// Função executada por cada thread que trata um cliente
// Recebe como argumento um ponteiro para o socket do cliente
void* clienteHandler(void* arg)
{
    // Extrai o descritor do socket do cliente
    int socketCliente = *(int*)arg;

    // Liberta a memória alocada no main para o socket
    free(arg);

    // ID do cliente (enviado pelo próprio cliente na primeira mensagem)
    // Inicializado a -1 para indicar que ainda não foi recebido
    int idCliente = -1;

    // Buffer para armazenar pedidos enviados pelo cliente
    char clientRequest[512];

    // Ciclo principal de comunicação com o cliente
    while (1)
    {
        // Apenas escreve no log se o cliente já tiver um ID atribuído
        if (idCliente != -1)
        {
            writeLogf("../Servidor/log_servidor.csv",
                      "Está a preparar para ler o pedido do cliente -> %d",
                      idCliente);
        }

        // Lê uma linha enviada pelo cliente através do socket
        int bytesReceived = lerLinha(socketCliente,
                                     clientRequest,
                                     sizeof(clientRequest));

        // Se ocorrer erro na ligação
        if (bytesReceived < 0)
        {
            perror("Erro: Servidor não conseguiu receber o pedido do cliente");
            writeLogf("../Servidor/log_servidor.csv",
                      "Servidor não conseguiu receber o pedido do cliente %d",
                      idCliente);
            exit(1);
        }

        // Se o cliente fechar a ligação
        if (bytesReceived == 0)
        {       
            close(socketCliente);
            return NULL;   // termina só esta thread
        }


        // Se ainda não foi recebido o ID do cliente
        // A primeira mensagem enviada pelo cliente corresponde ao seu ID
        if (idCliente == -1)
        {
            idCliente = atoi(clientRequest);
            writeLogf("../Servidor/log_servidor.csv",
                      "Cliente ligado com ID: %d",
                      idCliente);
            continue;
        }

        writeLogf("../Servidor/log_servidor.csv",
                  "Pedido do cliente %d foi lido com sucesso",
                  idCliente);

        int codeRequest = atoi(clientRequest);

        writeLogf("../Servidor/log_servidor.csv",
                  "O codigo do pedido enviado pelo cliente %d é -> %d",
                  idCliente,
                  codeRequest);

        char *restOfClientRequest = strchr(clientRequest, ',');
        if (restOfClientRequest)
            restOfClientRequest++;

        switch (codeRequest)
        {
            case 1:
                writeLogf("../Servidor/log_servidor.csv",
                          "O pedido do cliente %d é sobre o envio de um novo jogo",
                          idCliente);

                // Envia o jogo ao cliente
                // JOGADORES_POR_SALA indica quantos jogadores são necessários
                sendGameToClient(socketCliente,
                                 idCliente,
                                 JOGADORES_POR_SALA);
                break;

            case 2:
            {
                // Loga que o cliente está a fazer um pedido de verificação de uma célula do Sudoku
                writeLogf("../Servidor/log_servidor.csv",
                          "O pedido do cliente %d é sobre a verificação de uma resposta",
                          idCliente);

                // Variáveis que vão armazenar os parâmetros enviados pelo cliente
                int gameId, row, col, answer;

                // Verifica se a string de resto da mensagem existe e tenta extrair 4 inteiros
                // gameId -> ID do jogo
                // row -> linha da célula a validar
                // col -> coluna da célula a validar
                // answer -> valor do cliente para essa célula
                if (restOfClientRequest &&
                    sscanf(restOfClientRequest, "%d,%d,%d,%d",
                           &gameId, &row, &col, &answer) == 4)
                {
                    // Loga que os parâmetros recebidos são válidos
                    writeLogf("../Servidor/log_servidor.csv",
                              "O pedido do cliente %d enviou os parâmetros de verificação válidos",
                              idCliente);

                    // Aloca memória para criar um novo pedido de validação
                    PedidoValidacao *p = malloc(sizeof(PedidoValidacao));
                    if (!p)
                    {
                        // Se falhar a alocação, loga erro e interrompe o case
                        writeLogf("../Servidor/log_servidor.csv",
                                  "Erro de alocação de memória para PedidoValidacao (cliente %d)",
                                  idCliente);
                        break;
                    }

                    // Preenche os campos da estrutura com os dados do cliente e do jogo
                    p->socketCliente = socketCliente; // socket do cliente
                    p->clientId      = idCliente;     // ID do cliente
                    p->gameId        = gameId;        // ID do jogo
                    p->row           = row;           // linha da célula
                    p->col           = col;           // coluna da célula
                    p->answer        = answer;        // valor do cliente
                    p->next          = NULL;          // próximo elemento da fila (nulo por enquanto)

                    // Envia o pedido para a fila FIFO, que será processada pelo workerValidacoes
                    enqueuePedido(p);
                }
                else
                {
                    // Caso os parâmetros recebidos sejam inválidos, loga o erro
                    writeLogf("../Servidor/log_servidor.csv",
                              "Parâmetros inválidos enviados pelo cliente %d",
                              idCliente);
                }

                // Encerra este case
                break;
            }

            default:
                writeLogf("../Servidor/log_servidor.csv",
                          "Pedido inválido enviado pelo cliente %d",
                          idCliente);
                break;
        }
    }

    close(socketCliente);
    return NULL;
}


int main(void)
{
    int socketServidor;
    struct sockaddr_un enderecoServidor;

    pthread_t worker;
    pthread_create(&worker, NULL, workerValidacoes, NULL);


    srand(time(NULL));

    if ((socketServidor = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Erro ao criar socket");
        exit(1);
    }

    memset(&enderecoServidor, 0, sizeof(enderecoServidor));
    enderecoServidor.sun_family = AF_UNIX;
    strcpy(enderecoServidor.sun_path, UNIXSTR_PATH);
    unlink(UNIXSTR_PATH);

    if (bind(socketServidor,
             (struct sockaddr*)&enderecoServidor,
             sizeof(enderecoServidor)) < 0)
    {
        perror("Erro no bind");
        exit(1);
    }

    listen(socketServidor, MAX_CLIENTES);

    readGamesFromCSV();

    printf("Servidor pronto para aceitar clientes...\n");

    while (1)
    {
        int socketCliente = accept(socketServidor, NULL, NULL);
        if (socketCliente < 0)
            continue;

        // Aloca memória para passar o socket à thread
        int *sockPtr = malloc(sizeof(int));
        *sockPtr = socketCliente;

         // Cria uma nova thread para tratar o cliente
        pthread_t tid;
        pthread_create(&tid, NULL, clienteHandler, sockPtr);

         // A thread é destacada (liberta recursos automaticamente)
        pthread_detach(tid);
    }

    close(socketServidor);
    return 0;
}
