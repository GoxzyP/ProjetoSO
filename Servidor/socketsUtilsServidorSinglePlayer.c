// -------------------- Includes --------------------

#include "../unix.h"
#include "../util.h"
#include <pthread.h> // Define as funções de manipulação de threads
#include <sys/epoll.h>

#include "../include/uthash.h" // Define as funções da hash table em C
#include <stdio.h>
#include <string.h>

// -------------------- Defines --------------------

#define BUFFER_PRODUCER_CONSUMER_SIZE 10
#define BUFFER_SOCKET_MESSAGE_SIZE 512
#define MAX_LINE_SIZE_IN_CSV 166 // Tamanho máximo de cada linha que contém as informações do jogo em csv
#define MAX_NUMBER_OF_EVENTS 64 // Número máximo de eventos que epoll_wait pode retornar de uma só vez
#define NUMBER_OF_CONSUMERS 4

// --------------------- Estruturas ---------------------

// Estrutura que representa um jogo de Sudoku
typedef struct GameData{
    int id;                          
    char partialSolution[82];       
    char totalSolution[82];         
    UT_hash_handle hh; 
} gameData;

// Estrutura para armazenar solução completa enviada pelo cliente
typedef struct PriorityQueueEntry
{
    int gameId;
    char clientCompleteSolution[82];
}priorityQueueEntry;

// Estrutura para armazenar tentativa parcial do cliente
typedef struct QueueEntry
{
    int gameId;
    int rowSelected;
    int columnSelected;
    int clientAnswer;
}queueEntry;

// Fila circular genérica para produtores/consumidores
typedef struct QueueFifo
{   
    void *items[BUFFER_PRODUCER_CONSUMER_SIZE];
    int front;
    int rear;
    int numberOfItemsInQueue;
}queueFifo;

// Estrutura que representa uma mensagem do cliente
typedef struct ClientRequest
{
    int socket;
    int requestType;
    union {
        queueEntry partialSolution;
        priorityQueueEntry completeSolution;
    } data;
}clientRequest;

// Estrutura que representa uma sala do modo multijogador
typedef struct MultiplayerRoom
{
    int gameId;
    char partialSolution[82];
    int numberOfClients;
    pthread_barrier_t barrier;
}multiplayerRoom;

// --------------------- Variáveis globais ---------------------

gameData *gamesHashTable = NULL; // Tabela hash com todos os jogos
queueFifo normalQueue ; // Fila para respostas parciais
queueFifo priorityQueue; // Fila para respostas completas
multiplayerRoom room = {0};

pthread_mutex_t mutexProducerConsumerQueueFifo; // Mutex para acesso às filas
pthread_mutex_t mutexGameData; // Mutex para acesso à hash table dos jogos
pthread_mutex_t mutexMultiplayerRoom; // Mutex para acesso à sala do modo multijogador
pthread_cond_t producerQueueFifoConditionVariable; // Condicional para produtor
pthread_cond_t consumerQueueFifoConditionVariable; // Condicional para consumidor

pthread_t consumers[NUMBER_OF_CONSUMERS]; // Threads consumidoras 
pthread_t ioThread; // Thread responsável por receber os pedidos dos clientes

int epollFd; // File descriptor do epoll usado para monitorizar múltiplos sockets simultaneamente

// --------------------- Funções auxiliares ---------------------

// -----------------------------
// Verifica se a fila está vazia
// -----------------------------
int isQueueEmpty(queueFifo *queue) {return queue->numberOfItemsInQueue == 0;}

// -----------------------------------------------------
// Verifica se a fila está cheia
// -----------------------------------------------------
int isQueueFull(queueFifo *queue) {return queue->numberOfItemsInQueue == BUFFER_PRODUCER_CONSUMER_SIZE;}

// -----------------------------------------------------
// Inicializa uma fila circular
// -----------------------------------------------------
void inicializeQueue(queueFifo *queue)
{   
    queue->front = 0;
    queue->rear = 0;
    queue->numberOfItemsInQueue = 0;
}

// ---------------------------------------
// Adiciona item ao final da fila circular
// ---------------------------------------
void addItemToQueue(queueFifo *queue , void *item)
{   
    queue->items[queue->rear] = item;

    queue->rear = (queue->rear + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;

    queue->numberOfItemsInQueue++;
}

// --------------------------------------
// Remove item do início da fila circular
// --------------------------------------
void *removeItemFromQueue(queueFifo *queue)
{   
    void *itemRemoved = queue->items[queue->front];

    queue->front = (queue->front + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;

    queue->numberOfItemsInQueue--;

    return itemRemoved;
}

// --------------------- Gestão de jogos ---------------------

// -----------------------------------------
// Lê os jogos do CSV e insere na hash table
// -----------------------------------------
void readGamesFromCSV()
{   
    FILE *file = fopen("Servidor/jogos.csv", "r");

    if (file == NULL)
    {
        printf("Erro : Não foi possível abrir o ficheiro onde estão armazenados os jogos\n");
        return;
    }

    char line[MAX_LINE_SIZE_IN_CSV];

    while (fgets(line, MAX_LINE_SIZE_IN_CSV, file)) //Lê o ficheiro linha a linha
    {   
        char *dividedLine = strtok(line, ","); // Separa utilizando vírgula
        if (dividedLine == NULL) continue;

        //Cria uma estrutura gameData que vai armazenar a informação do jogo que está a ser lido e copia os dados para a estrutura
        gameData *currentGame = malloc(sizeof(gameData)); 
        currentGame->id = atoi(dividedLine);              

        dividedLine = strtok(NULL, ",");  
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->partialSolution, dividedLine);

        dividedLine = strtok(NULL, ","); 
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->totalSolution, dividedLine);;

        HASH_ADD_INT(gamesHashTable, id, currentGame); //Adiciona o jogo à hashTable
    }

    fclose(file);
}

// --------------------------------------
// Envia um jogo aleatório para o cliente
// --------------------------------------
void sendGameToClient(int socket)
{   
    printf("Thread produtora %d entrou na função de enviar o jogo para o cliente\n" , pthread_self());

    int numberOfGamesInServer = HASH_COUNT(gamesHashTable); // Conta quantos jogos existem armazenados no servidor

    if(numberOfGamesInServer == 0)
    {
        perror("Error : Server could not find any games in the data base");
        exit(1);
    }

    int index = rand() % numberOfGamesInServer; // Escolhe um jogo aleatório
    int i = 0;

    gameData *game;

    for(game = gamesHashTable ; game != NULL ; game = game->hh.next)
    {   
        if(i == index) // Percorre a hashTable até chegar ao jogo escolhido
        {   
            //Prepara a mensagem para enviar ao cliente com o formato "id,solução parcial\n"
            char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

            snprintf(messageToClient , sizeof(messageToClient) , "%d,%s\n" , game->id , game->partialSolution);
            printf("Mensagem que será enviada para o cliente pela thread produtora %d -> %s\n" , pthread_self() , messageToClient);

            if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
            {   
                //Caso falhar , remove o socket da pool e fecha o socket
                perror("Error : Server could not sent game to client or client disconnected from the socket");
                epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
                close(socket);
                return;           
            }

            printf("Thread produtora %d enviou o jogo com sucesso\n" , pthread_self());

            break;
        }

        i++;
    }

    printf("Thread produtora %d finalizou a função responsável por enviar o jogo ao cliente\n" , pthread_self());
}

void handleSendGameLogic(socket)
{
    if(MULTIPLAYER_MODE == 0)
    {
        sendGameToClient(socket);
        return;
    }

    pthread_mutex_lock(&mutexMultiplayerRoom);

    if(room.numberOfClients == 0)
    {
        int count = HASH_COUNT(gamesHashTable);
        int index = rand() % count;
        int i = 0;
        gameData *game;

        for (game = gamesHashTable; game; game = game->hh.next)
        {
            if (i++ == index)
            {
                room.gameId = game->id;
                memcpy(room.partialSolution, game->partialSolution, 81);
                room.partialSolution[81] = '\0';
                break;
            }
        }

        pthread_barrier_init(&room.barrier, NULL, NUMBER_OF_PLAYERS_IN_A_ROOM);
    }

    room.numberOfClients++;
    pthread_mutex_unlock(&mutexMultiplayerRoom);
    pthread_barrier_wait(&room.barrier);

    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];
    snprintf(messageToClient , sizeof(messageToClient) , "%d,%s\n" , room.gameId , room.partialSolution);

    if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
    {   
        room.numberOfClients--;
        perror("Error : Server could not sent game to client or client disconnected from the socket");
        epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
        close(socket);
        return;   
    }

    pthread_mutex_lock(&mutexMultiplayerRoom);
    room.numberOfClients--;

    if(room.numberOfClients == 0)
        pthread_barrier_destroy(&room.barrier);

    pthread_mutex_unlock(&mutexMultiplayerRoom);
}

// -------------------------------------
// Verifica soluções parciais do cliente
// -------------------------------------
void verifyClientPartialSolution(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer)
{   
    gameData *gameSentToClient;
    pthread_mutex_lock(&mutexGameData); // Protege acesso à hash table
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient); // Procura o jogo na hashTable utilizando o seu id
    pthread_mutex_unlock(&mutexGameData);
    
    // Prepara a mensagem para enviar ao cliente com o formato "Correct\n ou Wrong\n"
    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    if (!gameSentToClient || (rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8))
    {
        perror("Error : Server could not find a game with the id sent from the client or the coordinates are wrong");
        epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
        close(socket);
        return;
    }
    
    // Verifica se a resposta está correta
    gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0' ? strcpy(messageToClient, "Correct\n") : strcpy(messageToClient, "Wrong\n");
    printf("A thread consumidora %d avaliou a tentativa do cliente e esta estava %s\n" , pthread_self() , messageToClient);
    printf("A thread consumidora %d está a preparar-se para enviar a resposta da tentativa ao cliente\n" , pthread_self());

    if (writeSocket(socket, messageToClient, strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client partial solution or the client disconnected from the socket");
        epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
        close(socket);
        return;   
    }

    printf("A thread consumidora %d enviou a resposta ao cliente com sucesso\n" , pthread_self());
}

// --------------------------------------
// Verifica soluções completas do cliente
// --------------------------------------
void verifyClientCompleteSolution(int socket , int gameId , char completeSolution[82])
{
    gameData *gameSentToClient;
    pthread_mutex_lock(&mutexGameData); // Protege acesso à hash table
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient); // Procura o jogo na hashTable utilizando o seu id
    pthread_mutex_unlock(&mutexGameData);

    if (!gameSentToClient)
    {   
        perror("Error : Server could not find a game with the id sent from the client");
        epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
        close(socket);
        return;
    }

    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    // Passa por todas as células do Sudoku e envia ao cliente a primeira célula incorreta
    for(int row = 0 ; row < 9 ; row++)
    {
        for(int column = 0 ; column < 9 ; column++)
        {
            if(gameSentToClient->totalSolution[row * 9 + column] != completeSolution[row * 9 + column])
            {
                sprintf(messageToClient , "%d,%d\n" , row , column);

                printf("A thread consumidora %d encontrou um erro na celula %d %d da solucao completa enviada pelo cliente\n" , row , column);

                if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
                {
                    perror("Error : Server could not write the answer of the client complete solution or the client disconnected from the socket");
                    epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
                    close(socket);
                    return;
                }

                return;
            }
        }
    }

    printf("A thread consumidora %d nao nenhum erro na solucao completa enviada pelo cliente");
    strcpy(messageToClient, "Correct\n");

    if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client complete solution or the client disconnected from the socket");
        epoll_ctl(epollFd , EPOLL_CTL_DEL , socket , NULL);
        close(socket);
        return;
    }
}

// --------------------- Threads ---------------------

// ------------------------------------------------------------------------------------
// Thread responsável pela leitura do socket(Receber os pedidos e coloca-los nas filas)
// ------------------------------------------------------------------------------------
void *ioHandler(void *arguments)
{   
    // Array que irá armazenar os sockets com dados prontos para leitura
    struct epoll_event events[MAX_NUMBER_OF_EVENTS];

    while(1)
    {   
        // Bloqueia até que pelo menos um socket tenha dados prontos e retorna o número de sockets disponíveis
        int numberOfEvents = epoll_wait(epollFd , events , MAX_NUMBER_OF_EVENTS , -1);

        for(int i = 0 ; i < numberOfEvents ; i++)
        {
            int clientSocket = events[i].data.fd;

            char messageFromClient[BUFFER_SOCKET_MESSAGE_SIZE];
            int bytesReceived = readSocket(clientSocket , messageFromClient , sizeof(messageFromClient));

            if(bytesReceived <= 0)
            {   
                epoll_ctl(epollFd , EPOLL_CTL_DEL , clientSocket , NULL);
                close(clientSocket);
                continue;
            }

            int codeOfClientMessage = atoi(messageFromClient); //Lê o código da mensagem do cliente

            clientRequest *request = malloc(sizeof(clientRequest));
            request->socket = clientSocket;

            // Caso o código seja 1, envia um jogo ao cliente
            if(codeOfClientMessage == 1)
            {   
                request->requestType = 0;

                pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

                while(isQueueFull(&priorityQueue))
                        pthread_cond_wait(&producerQueueFifoConditionVariable, &mutexProducerConsumerQueueFifo);

                addItemToQueue(&priorityQueue , request);

                pthread_cond_signal(&consumerQueueFifoConditionVariable);
                pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

                printf("A thread produtora %d acabou de processar o pedido de código 1 do cliente\n" , pthread_self());
            }
            // Caso o código seja 2, o cliente está a enviar uma tentativa de solução
            else if(codeOfClientMessage == 2)
            {
                char *restOfClientMessage = strchr(messageFromClient, ',');
                if(!restOfClientMessage){continue;}
                restOfClientMessage++; 

                request->requestType = atoi(restOfClientMessage); // Determina se a mensagem é parcial (1) ou completa (2).

                pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

                if(request->requestType == 1)
                {
                    sscanf(
                        restOfClientMessage,
                        "1,%d,%d,%d,%d",
                        &request->data.partialSolution.gameId,
                        &request->data.partialSolution.rowSelected,
                        &request->data.partialSolution.columnSelected,
                        &request->data.partialSolution.clientAnswer
                    );
                    
                    // Espera se a fila normal estiver cheia
                    while(isQueueFull(&normalQueue))
                        pthread_cond_wait(&producerQueueFifoConditionVariable, &mutexProducerConsumerQueueFifo);

                    addItemToQueue(&normalQueue , request);
                }
                else
                {
                    sscanf(restOfClientMessage , "2,%d,%s" , &request->data.completeSolution.gameId , request->data.completeSolution.clientCompleteSolution);

                    // Espera se a fila prioritária  estiver cheia
                    while(isQueueFull(&priorityQueue))
                        pthread_cond_wait(&producerQueueFifoConditionVariable, &mutexProducerConsumerQueueFifo);

                    addItemToQueue(&priorityQueue , request);
                }

                pthread_cond_signal(&consumerQueueFifoConditionVariable);
                pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);
            }
            else
                free(request);
        }
    }    

    return NULL;
}

// -------------------------------------------------------------------------
// Thread consumidora que processa pedidos dos clientes(valida as respostas)
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

        // Prioriza pedidos da fila prioritaria
        if(!isQueueEmpty(&priorityQueue))
            request = removeItemFromQueue(&priorityQueue);
        else
            request = removeItemFromQueue(&normalQueue);  
        
        pthread_cond_signal(&producerQueueFifoConditionVariable);
        pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

        if(request->requestType == 0)
        {   
            printf("A\n");
            handleSendGameLogic(request->socket);
        }

        else if(request->requestType == 1)
        {
            verifyClientPartialSolution(
                request->socket, 
                request->data.partialSolution.gameId,
                request->data.partialSolution.rowSelected,
                request->data.partialSolution.columnSelected,
                request->data.partialSolution.clientAnswer
            );
        }
        else
            verifyClientCompleteSolution(request->socket , request->data.completeSolution.gameId , request->data.completeSolution.clientCompleteSolution);

        free(request);
    }

    return NULL;
}

// --------------------- Main ---------------------

// --------------------------------------------------
// Função principal que inicializa servidor e threads
// --------------------------------------------------
int main(void)
{   
    int serverSocket;
    struct sockaddr_un serverAddr;

    srand(time(NULL));
    readGamesFromCSV();

    inicializeQueue(&normalQueue);
    inicializeQueue(&priorityQueue);

    pthread_mutex_init(&mutexProducerConsumerQueueFifo, NULL);
    pthread_mutex_init(&mutexGameData, NULL);
    pthread_mutex_init(&mutexMultiplayerRoom, NULL);
    pthread_cond_init(&producerQueueFifoConditionVariable, NULL);
    pthread_cond_init(&consumerQueueFifoConditionVariable, NULL);

    // Cria o socket do servidor
    if((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("Error: Could not open server stream socket");
        exit(1);
    }
    
    // Limpa estrutura de endereço
    bzero((char *)&serverAddr, sizeof(serverAddr));

    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, UNIXSTR_PATH);
    unlink(UNIXSTR_PATH);

    // Associa o socket a um endereço local
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, strlen(serverAddr.sun_path) + sizeof(serverAddr.sun_family)) < 0)
    {
        perror("Error : Server cannot bind to local address");
        exit(1);
    }

    listen(serverSocket, 10); // Escuta conexões com backlog de 10
    epollFd = epoll_create1(0); // Cria epoll que irá monitorizar múltiplos sockets

    for (int i = 0; i < NUMBER_OF_CONSUMERS; i++)
        pthread_create(&consumers[i], NULL, consumer, NULL);

    pthread_create(&ioThread, NULL, ioHandler, NULL);

    while(1)
    {
        int clientSocket = accept(serverSocket, NULL, NULL);

        struct epoll_event ev;
        ev.events = EPOLLIN; // Monitoriza apenas eventos de leitura
        ev.data.fd = clientSocket;

        epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &ev); // Adiciona o socket do cliente ao epoll
    }
}