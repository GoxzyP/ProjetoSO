#include "string.h"
#include "../unix.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"
#include <sys/mman.h>
#include <signal.h>

// Variável global para poder limpar no signal handler
int socketServidorGlobal = -1;

void signalHandler(int sig)
{
    static int cleanup_in_progress = 0;
    
    // Evitar chamadas múltiplas
    if (cleanup_in_progress) {
        return;
    }
    cleanup_in_progress = 1;
    
    printf("\n[INFO] Servidor a terminar... Limpando recursos\n");
    
    // Fechar socket
    if (socketServidorGlobal >= 0) {
        close(socketServidorGlobal);
    }
    
    // Limpar socket Unix
    unlink(UNIXSTR_PATH);
    
    // Limpar memória partilhada
    cleanupSharedMemory();
    
    // Matar todos os processos filhos
    signal(SIGCHLD, SIG_IGN);
    kill(0, SIGTERM);
    
    printf("[INFO] Recursos limpos com sucesso\n");
    exit(0);
}

int main(void)
{
    int socketServidor, socketCliente;
    int pidFilho;
    int tamanhoServidor, tamanhoCliente;

    srand(time(NULL));
    
    // Registrar signal handlers para limpeza
    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // kill
    signal(SIGQUIT, signalHandler);  // Ctrl+\

    struct sockaddr_un enderecoServidor, enderecoCliente;
    
    // Limpar recursos anteriores (caso servidor anterior não tenha terminado corretamente)
    unlink(UNIXSTR_PATH);
    shm_unlink("/sudoku_server_shm");

    if ((socketServidor = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Erro não foi possível abrir o socket stream do servidor");
        writeLogf("../Servidor/log_servidor.csv","Erro não foi possível abrir o socket stream do servidor");
        exit(1);
    }
    
    // Armazenar globalmente para cleanup
    socketServidorGlobal = socketServidor;

    bzero((char *)&enderecoServidor, sizeof(enderecoServidor));
    enderecoServidor.sun_family = AF_UNIX;
    strcpy(enderecoServidor.sun_path, UNIXSTR_PATH);

    tamanhoServidor = strlen(enderecoServidor.sun_path) + sizeof(enderecoServidor.sun_family);

    if (bind(socketServidor, (struct sockaddr *)&enderecoServidor, tamanhoServidor) < 0)
    {
        perror("Servidor não consegue fazer bind do endereço local");
        writeLogf("../Servidor/log_servidor.csv","Servidor não consegue fazer bind do endereço local");
        exit(1);
    }

    readGamesFromCSV();
    
    // Inicializar memória compartilhada
    initSharedMemory();
    
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
            
            // Inicializar estatísticas locais deste cliente
            ServerGlobalStats globalStats = {0};
            ClientStatsServer *clientsHash = NULL;

            while (1)
            {
                char clientRequest[512];

                writeLogf("../Servidor/log_servidor.csv","Está a preparar para ler o pedido do cliente");

                int bytesReceived = lerLinha(socketCliente, clientRequest, sizeof(clientRequest));

                if (bytesReceived == 0)
                {
                    writeLogf("../Servidor/log_servidor.csv","Cliente %d desconectou", idCliente);
                    break; // Sai do loop para escrever estatísticas
                }
                
                if (bytesReceived < 0)
                {
                    perror("Error : Servidor não conseguiu receber o pedido do cliente");
                    writeLogf("../Servidor/log_servidor.csv","Erro ao ler pedido do cliente %d", idCliente);
                    break;
                }

                writeLogf("../Servidor/log_servidor.csv","Pedido do cliente foi lido com sucesso");
                writeLogf("../Servidor/log_servidor.csv","Pedido do cliente era válido");

                if (idCliente == -1)
                {
                    idCliente = atoi(clientRequest);
                    writeLogf("../Servidor/log_servidor.csv","Cliente ligado com ID: %d", idCliente);
                    
                    // Registrar cliente único
                    registerUniqueClient(idCliente);
                    
                    // Incrementar contador de clientes ativos
                    incrementActiveClients();
                    
                    // Atualizar estatísticas em tempo real
                    updateRealtimeStats();
                    
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
                        sendGameToClient(socketCliente , idCliente, &clientsHash, &globalStats);
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

                        verifyClientSudokuAnswer(socketCliente, gameId, rowSelected, columnSelected, clientAnswer , idCliente, &clientsHash, &globalStats);
                        break;
                    }

                    default:
                        break;
                }
            }
            
            // Cliente desconectou - decrementar contador
            if (idCliente != -1) {
                unregisterClientOnline(idCliente);
                decrementActiveClients();
                updateRealtimeStats();
                writeLogf("../Servidor/log_servidor.csv","Cliente %d desconectou. Clientes ativos: %d", idCliente, getActiveClientsCount());
            }
            
            // NÃO escrever writeServerStats aqui - mantém estatísticas da memória partilhada
            // As estatísticas já foram atualizadas por updateRealtimeStats()
            
            // Liberar memória da hash table
            ClientStatsServer *cliente, *tmp;
            HASH_ITER(hh, clientsHash, cliente, tmp) {
                HASH_DEL(clientsHash, cliente);
                free(cliente);
            }
        }

        close(socketCliente);
    }
}
