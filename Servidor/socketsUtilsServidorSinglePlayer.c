#include "socketsUtilsServidorSinglePlayer.h"
#include "socketsUtilsServidor.h"
#include "servidorGameManager.h"
#include "../util.h"
#include "../log.h"  // Para sistema de logs
#include <unistd.h> // Para close()

#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h> // Para epoll

// Número máximo de eventos para epoll_wait
#define MAX_NUMBER_OF_EVENTS 64

// --------------------------------------------------
// Variáveis globais para estatísticas
// --------------------------------------------------
ClientStatsServer *clientsStatsHash = NULL;  // Hash table de stats de clientes
ServerGlobalStats globalStats = {0, 0, 0, 0, 0, 0};  // Stats globais inicializadas

// Padrão Leitor-Escritor: múltiplos leitores OU um escritor exclusivo
pthread_mutex_t mutexLeitores;   // Protege numLeitores
pthread_mutex_t mutexEscritores; // Escritor exclusivo
int numLeitores = 0;             // Contador de leitores ativos


// Filas FIFO para pedidos normais e prioritários
queueFifo normalQueue;
queueFifo priorityQueue;

// Mutex e variáveis de condição para sincronização produtor/consumidor
pthread_mutex_t mutexProducerConsumerQueueFifo;
pthread_cond_t producerQueueFifoConditionVariable;
pthread_cond_t consumerQueueFifoConditionVariable;

// Arrays de threads
pthread_t consumers[NUMBER_OF_CONSUMERS];
pthread_t ioThread;

// Variável global do epoll, declarada extern
extern int epollFd;

// ------------------------------------------------------------------------------------
// Thread responsável pela leitura do socket (recebe pedidos e coloca nas filas)
// ------------------------------------------------------------------------------------
void *ioHandler(void *arguments)
{   
    // Array que irá armazenar os sockets com dados prontos para leitura
    struct epoll_event events[MAX_NUMBER_OF_EVENTS];

    while(1)
    {   
        // Bloqueia até que pelo menos um socket tenha dados prontos
        int numberOfEvents = epoll_wait(epollFd , events , MAX_NUMBER_OF_EVENTS , -1);

        for(int i = 0 ; i < numberOfEvents ; i++)
        {
            int clientSocket = events[i].data.fd;

            char messageFromClient[BUFFER_SOCKET_MESSAGE_SIZE];
            int bytesReceived = readSocket(clientSocket , messageFromClient , sizeof(messageFromClient));

            if(bytesReceived <= 0)
            {   
                // Remove socket do epoll e fecha
                epoll_ctl(epollFd , EPOLL_CTL_DEL , clientSocket , NULL);
                close(clientSocket);
                
                // Decrementa contador de clientes ativos
                decrementActiveClients();
                int clientesAtivos = getActiveClientsCount();
                printf("Cliente desconectado. Clientes ativos: %d\n", clientesAtivos);
                writeLogf("../Servidor/log_servidor.txt", "Cliente desconectado (socket %d). Total de clientes ativos: %d", clientSocket, clientesAtivos);
                
                continue;
            }

            // Lê o código da mensagem do cliente
            int codeOfClientMessage = atoi(messageFromClient); 

            // Caso o código seja 1, envia um jogo ao cliente
            if(codeOfClientMessage == 1)
            {   
                // Extrair clientId da mensagem (formato: "1,clientId")
                int clientId = 0;
                char *comma = strchr(messageFromClient, ',');
                if (comma) {
                    clientId = atoi(comma + 1);
                }
                
                printf("Pedido de jogo recebido do cliente ID %d (socket %d)\n", clientId, clientSocket);
                writeLogf("../Servidor/log_servidor.txt", "Pedido de jogo recebido do cliente ID %d (socket %d)", clientId, clientSocket);
                
                sendGameToClient(clientSocket, clientId);
                printf("A thread produtora %lu acabou de processar o pedido de código 1 do cliente %d\n" , pthread_self(), clientId);
                writeLogf("../Servidor/log_servidor.txt", "Thread produtora %lu acabou de processar o pedido de código 1 do cliente %d", pthread_self(), clientId);
                continue;
            }
            // Caso o código seja 2, o cliente está a enviar uma tentativa de solução
            else if(codeOfClientMessage == 2)
            {
                clientRequest *request = malloc(sizeof(clientRequest));
                request->socket = clientSocket;

                // Obtem o restante da mensagem após a primeira vírgula
                char *restOfClientMessage = strchr(messageFromClient, ',');
                if(!restOfClientMessage){ free(restOfClientMessage) ; continue;}
                restOfClientMessage++; 

                // Determina se a mensagem é parcial (1) ou completa (2)
                request->requestType = atoi(restOfClientMessage);

                // Bloqueia a fila produtor/consumidor
                pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

                if(request->requestType == 1)
                {
                    // Preenche a estrutura para pedido parcial (formato: "1,gameId,row,col,answer,clientId")
                    sscanf(
                        restOfClientMessage,
                        "1,%d,%d,%d,%d,%d",
                        &request->data.partialSolution.gameId,
                        &request->data.partialSolution.rowSelected,
                        &request->data.partialSolution.columnSelected,
                        &request->data.partialSolution.clientAnswer,
                        &request->data.partialSolution.clientId
                    );
                    
                    // Espera se a fila normal estiver cheia
                    while(isQueueFull(&normalQueue))
                        pthread_cond_wait(&producerQueueFifoConditionVariable, &mutexProducerConsumerQueueFifo);

                    // Adiciona pedido à fila normal
                    addItemToQueue(&normalQueue , request);
                }
                else
                {
                    // Preenche a estrutura para pedido completo (formato: "2,gameId,solution,clientId")
                    char tempSolution[100];  // Buffer maior para evitar overflow
                    int tempClientId;
                    sscanf(restOfClientMessage , "2,%d,%82[^,],%d" , &request->data.completeSolution.gameId , tempSolution, &tempClientId);
                    
                    strncpy(request->data.completeSolution.clientCompleteSolution, tempSolution, 81);
                    request->data.completeSolution.clientCompleteSolution[81] = '\0';
                    request->data.completeSolution.clientId = tempClientId;

                    // Espera se a fila prioritária estiver cheia
                    while(isQueueFull(&priorityQueue))
                        pthread_cond_wait(&producerQueueFifoConditionVariable, &mutexProducerConsumerQueueFifo);

                    // Adiciona pedido à fila prioritária
                    addItemToQueue(&priorityQueue , request);
                }

                // Sinaliza consumidores e desbloqueia mutex
                pthread_cond_signal(&consumerQueueFifoConditionVariable);
                pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);
            }
        }
    }    

    return NULL;
}

// -------------------------------------------------------------------------
// Thread consumidora que processa pedidos dos clientes (valida as respostas)
// -------------------------------------------------------------------------
void *consumer(void *arguments)
{   
    while(1)
    {   
        pthread_mutex_lock(&mutexProducerConsumerQueueFifo);
        
        // Bloqueia até que pelo menos uma fila tenha itens
        while(isQueueEmpty(&priorityQueue) && isQueueEmpty(&normalQueue))
            pthread_cond_wait(&consumerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);

        clientRequest *request;

        // Prioriza pedidos da fila prioritária
        if(!isQueueEmpty(&priorityQueue))
            request = removeItemFromQueue(&priorityQueue);
        else
            request = removeItemFromQueue(&normalQueue);  
        
        // Sinaliza produtores e desbloqueia mutex
        pthread_cond_signal(&producerQueueFifoConditionVariable);
        pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

        // Processa pedido parcial
        if(request->requestType == 1)
        {
            verifyClientPartialSolution(
                request->socket, 
                request->data.partialSolution.gameId,
                request->data.partialSolution.rowSelected,
                request->data.partialSolution.columnSelected,
                request->data.partialSolution.clientAnswer,
                request->data.partialSolution.clientId
            );
        }
        // Processa pedido completo
        else
            verifyClientCompleteSolution(request->socket , request->data.completeSolution.gameId , request->data.completeSolution.clientCompleteSolution, request->data.completeSolution.clientId);

        // Liberta memória do pedido
        free(request);
    }

    return NULL;
}

// --------------------------------------------------------------
// Inicializa o sistema de estatísticas
// --------------------------------------------------------------
void initStatsSystem()
{
    // Inicializar mutexes para padrão Leitor-Escritor
    pthread_mutex_init(&mutexLeitores, NULL);
    pthread_mutex_init(&mutexEscritores, NULL);
    numLeitores = 0;
    
    // Inicializar estrutura de stats globais
    globalStats.totalAcertos = 0;
    globalStats.totalErros = 0;
    globalStats.totalJogadas = 0;
    globalStats.totalJogos = 0;
    globalStats.totalClientes = 0;
    globalStats.clientesAtivos = 0;
    
    // Hash table já está NULL por ser global
}

// --------------------------------------------------------------
// Lock para LEITURA (múltiplos leitores permitidos simultaneamente)
// --------------------------------------------------------------
void readerLock()
{
    pthread_mutex_lock(&mutexLeitores);
    numLeitores++;
    if (numLeitores == 1) {
        pthread_mutex_lock(&mutexEscritores);  // Primeiro leitor bloqueia escritores
    }
    pthread_mutex_unlock(&mutexLeitores);
}

// --------------------------------------------------------------
// Unlock para LEITURA
// --------------------------------------------------------------
void readerUnlock()
{
    pthread_mutex_lock(&mutexLeitores);
    numLeitores--;
    if (numLeitores == 0) {
        pthread_mutex_unlock(&mutexEscritores);  // Último leitor libera escritores
    }
    pthread_mutex_unlock(&mutexLeitores);
}

// --------------------------------------------------------------
// Lock para ESCRITA (escritor exclusivo - bloqueia tudo)
// --------------------------------------------------------------
void writerLock()
{
    pthread_mutex_lock(&mutexEscritores);
}

// --------------------------------------------------------------
// Unlock para ESCRITA
// --------------------------------------------------------------
void writerUnlock()
{
    pthread_mutex_unlock(&mutexEscritores);
}

// --------------------------------------------------------------
// Escreve estatísticas do servidor no log E mostra no terminal
// --------------------------------------------------------------
void writeServerStats()
{
    readerLock();  // Lock de LEITURA - múltiplos podem ler simultaneamente
    
    // Calcular taxa de acerto global
    float taxaAcertoGlobal = 0.0;
    int totalJogadas = globalStats.totalAcertos + globalStats.totalErros;
    if (totalJogadas > 0) {
        taxaAcertoGlobal = ((float)globalStats.totalAcertos / (float)totalJogadas) * 100.0;
    }

    // Calcular média de jogadas por cliente
    float mediaJogadasPorCliente = 0.0;
    if (globalStats.totalClientes > 0) {
        mediaJogadasPorCliente = (float)totalJogadas / (float)globalStats.totalClientes;
    }
    
    int clientesAtivos = globalStats.clientesAtivos;
    int totalAcertos = globalStats.totalAcertos;
    int totalErros = globalStats.totalErros;
    int totalJogos = globalStats.totalJogos;

    readerUnlock();  // Unlock de LEITURA

    // =========================================================================
    // MOSTRAR ESTATÍSTICAS NO TERMINAL (CMD)
    // =========================================================================
    printf("\n");
    printf("========================================\n");
    printf("ESTATISTICAS GLOBAIS DO SERVIDOR\n");
    printf("========================================\n");
    printf("1. Clientes Ativos (Conectados): %d\n", clientesAtivos);
    printf("2. Taxa de Acerto Global: %.2f%%\n", taxaAcertoGlobal);
    printf("3. Total de Acertos: %d\n", totalAcertos);
    printf("4. Total de Jogadas: %d\n", totalJogadas);
    printf("5. Total de Jogos Distribuidos: %d\n", totalJogos);
    printf("6. Total de Erros: %d\n", totalErros);
    printf("7. Media de Jogadas por Cliente: %.2f\n", mediaJogadasPorCliente);
    printf("========================================\n\n");
    fflush(stdout);  // Força a exibição imediata no terminal

    // Guardar nas logs do servidor também
    writeLogf("../Servidor/log_servidor.txt", "========================================");
    writeLogf("../Servidor/log_servidor.txt", "ESTATISTICAS GLOBAIS DO SERVIDOR");
    writeLogf("../Servidor/log_servidor.txt", "========================================");
    writeLogf("../Servidor/log_servidor.txt", "1. Clientes Ativos (Conectados): %d", clientesAtivos);
    writeLogf("../Servidor/log_servidor.txt", "2. Taxa de Acerto Global: %.2f%%", taxaAcertoGlobal);
    writeLogf("../Servidor/log_servidor.txt", "3. Total de Acertos: %d", totalAcertos);
    writeLogf("../Servidor/log_servidor.txt", "4. Total de Jogadas: %d", totalJogadas);
    writeLogf("../Servidor/log_servidor.txt", "5. Total de Jogos Distribuidos: %d", totalJogos);
    writeLogf("../Servidor/log_servidor.txt", "6. Total de Erros: %d", totalErros);
    writeLogf("../Servidor/log_servidor.txt", "7. Media de Jogadas por Cliente: %.2f", mediaJogadasPorCliente);
    writeLogf("../Servidor/log_servidor.txt", "========================================\n");

    // Criar caminho para ficheiro de estatísticas separado
    char statsPath[512] = "../Servidor/estatisticas_servidor.csv";

    // SOBRESCREVER ficheiro de estatísticas
    FILE *file = fopen(statsPath, "w");  // Modo "w" sobrescreve
    if (!file) {
        perror("Erro ao abrir ficheiro de estatísticas do servidor");
        return;
    }

    fprintf(file, "========================================\n");
    fprintf(file, "ESTATISTICAS GLOBAIS DO SERVIDOR\n");
    fprintf(file, "========================================\n");
    fprintf(file, "1. Clientes Ativos (Conectados): %d\n", clientesAtivos);
    fprintf(file, "2. Taxa de Acerto Global: %.2f%%\n", taxaAcertoGlobal);
    fprintf(file, "3. Total de Acertos: %d\n", totalAcertos);
    fprintf(file, "4. Total de Jogadas: %d\n", totalJogadas);
    fprintf(file, "5. Total de Jogos Distribuidos: %d\n", totalJogos);
    fprintf(file, "6. Total de Erros: %d\n", totalErros);
    fprintf(file, "7. Media de Jogadas por Cliente: %.2f\n", mediaJogadasPorCliente);
    fprintf(file, "========================================\n");
    
    fclose(file);
}

// --------------------------------------------------------------
// Incrementa contador de clientes ativos
// --------------------------------------------------------------
void incrementActiveClients()
{
    writerLock();  // Lock de ESCRITA - acesso exclusivo
    globalStats.clientesAtivos++;
    writerUnlock();
    
    writeServerStats();  // Atualiza ficheiro em tempo real
}

// --------------------------------------------------------------
// Decrementa contador de clientes ativos
// --------------------------------------------------------------
void decrementActiveClients()
{
    writerLock();  // Lock de ESCRITA - acesso exclusivo
    if (globalStats.clientesAtivos > 0) {
        globalStats.clientesAtivos--;
    }
    writerUnlock();
    
    writeServerStats();  // Atualiza ficheiro em tempo real
}

// --------------------------------------------------------------
// Retorna número de clientes atualmente conectados
// --------------------------------------------------------------
int getActiveClientsCount()
{
    readerLock();  // Lock de LEITURA - múltiplos leitores permitidos
    int count = globalStats.clientesAtivos;
    readerUnlock();
    
    return count;
}

// --------------------------------------------------------------
// Lida com desconexão de cliente (remove do epoll e decrementa contador)
// --------------------------------------------------------------
void handleClientDisconnection(int socket)
{
    extern int epollFd;
    epoll_ctl(epollFd, EPOLL_CTL_DEL, socket, NULL);
    close(socket);
    decrementActiveClients();
    int clientesAtivos = getActiveClientsCount();
    printf("Cliente desconectado por erro. Clientes ativos: %d\n", clientesAtivos);
    writeLogf("../Servidor/log_servidor.txt", "Cliente desconectado por erro (socket %d). Total de clientes ativos: %d", socket, clientesAtivos);
}
