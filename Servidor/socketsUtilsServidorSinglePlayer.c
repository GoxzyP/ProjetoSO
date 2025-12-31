//Util.h é onde está definido as funções de leitura e escrita dos sockets
#include "../util.h"

//Pthread.h é onde está definido as funções de manipulação de threads
#include <pthread.h>

//Uthash.h é onde está definido as funções de hashTable em c
#include "../include/uthash.h"

//Stdio.h é onde está definido as funções de input e output
#include <stdio.h>

//Stdbool.h permite usar o tipo boolean
#include <stdbool.h>

//String,h é onde está definido as funções de manipulação de string
#include <string.h>

#include <sys/select.h>

//BUFFER_PRODUCER_CONSUMER_SIZE define o tamanho do buffer que irá ser partilhado pelos produtores e consumidores
#define BUFFER_PRODUCER_CONSUMER_SIZE 5

//BUFFER_SOCKET_MESSAGE_SIZE define o tamanho máximo de mensagems que podem passar pelos sockets
#define BUFFER_SOCKET_MESSAGE_SIZE 512

//Tamanho máximo de cada linha que contém as informações do jogo em csv
#define MAX_LINE_SIZE_IN_CSV 166

//NUMBER_OF_PRODUCERS define o número de threads produtoras que o nosso código irá gerar
#define NUMBER_OF_PRODUCERS 1

//NUMBER_OF_CONSUMERS define o número de threads produtoras que o nosso código irá gerar
#define NUMBER_OF_CONSUMERS 1

//GameData define a estrutura que irá conter a informação dos jogos sudoku
//Id - Identificador único de jogo
//PartialSolution - Solução incompleta
//TotalSolution - Solução completa
//Hh - Campo necessário para utilizar esta struct como uma hashtable
typedef struct GameData{
    int id;                          
    char partialSolution[82];       
    char totalSolution[82];         
    UT_hash_handle hh; 
} gameData;

typedef struct PriorityQueueEntry
{
    int gameId;
    char clientCompleteSolution[82];
}priorityQueueEntry;

//QueueEntry define a estrutura de como é armazenado cada tentativa do cliente na fila
//GameId - Identificador único de jogo
//RowSelected - Linha escolhida na solução
//ColumnSelected - Coluna escolhida na solução
//ClientAnswer - Solução enviada pelo cliente
typedef struct QueueEntry
{
    int gameId;
    int rowSelected;
    int columnSelected;
    int clientAnswer;
}queueEntry;

//QueueFifo define a estrutura que irá funcionar como uma fila circula(FIFO)
//Items - Array que irá conter todas as entradas da fila
//Front - Índice do primeiro elemento da fila
//Rear - Índice do ultimo elemento da fila
//NumberOfItemsInQueue - Quantidade atual de itens na fila
typedef struct QueueFifo
{   
    void *items[BUFFER_PRODUCER_CONSUMER_SIZE];
    int front;
    int rear;
    int numberOfItemsInQueue;
}queueFifo;

//ProducerAndConsumerArguments define a estrutura de argumentos passados para cada thread de produtor e consumidor
//Socket - Socket onde está a acontecer a comunicação cliente/servidor
//Queue - Ponteiro que aponta para a fila FIFO
typedef struct ProducerAndConsumerArguments
{
    int socket;
    queueFifo *queue;
    queueFifo *priorityQueue;
}producerAndConsumerArguments;

//Tabela hash com todos os jogos carregados por csv
gameData *gamesHashTable = NULL;

//Mutexes e variáveis condicionais que serão utilizados na sicronização do buffer produtores/consumidores
pthread_mutex_t mutexProducerConsumerQueueFifo;
pthread_cond_t producerQueueFifoConditionVariable ; pthread_cond_t consumerQueueFifoConditionVariable;

//Mutexe utilizado para gerir a utilização da hash table que contém os jogos
pthread_mutex_t mutexGameData;

//Mutexe utilizado para gerir a utilização do socket
pthread_mutex_t mutexSocket;

// -----------------------------------------------------
// Função que retorna verdadeiro se a fila estiver vazia
// -----------------------------------------------------
bool isQueueEmpty(queueFifo *queue) {return queue->numberOfItemsInQueue == 0;}

// -----------------------------------------------------
// Função que retorna verdadeiro se a fila estiver cheia
// -----------------------------------------------------
bool isQueueFull(queueFifo *queue) {return queue->numberOfItemsInQueue == BUFFER_PRODUCER_CONSUMER_SIZE;}

// -----------------------------------------------------
// Função que inicializa as variáveis da estrutura queue
// -----------------------------------------------------
void inicializeQueue(queueFifo *queue)
{   
    //Inicializa os índice da fila e o número de items a 0
    queue->front = 0;
    queue->rear = 0;
    queue->numberOfItemsInQueue = 0;
}

// --------------------------------------------
// Função que adiciona um item ao final da fila
// --------------------------------------------
void addItemToQueue(queueFifo *queue , void *item)
{   
    //Insere um item na última posição da fila
    queue->items[queue->rear] = item;

    //Atualiza o índice do último elemento da fila tendo em conta que é uma fila circula por isso utilizamos %
    queue->rear = (queue->rear + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;

    //Incrementa o número de items na fila
    queue->numberOfItemsInQueue++;
}

// -----------------------------------------
// Função que remove o primeiro item da fila
// -----------------------------------------
void *removeItemFromQueue(queueFifo *queue)
{   
    //Remove o primeiro item da fila
    void *itemRemoved = queue->items[queue->front];

    //Atualiza o índice do primeiro elemento da fila tendo em conta que é uma fila circula por isso utilizamos %
    queue->front = (queue->front + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;

    //Decrementa o número de items na fila
    queue->numberOfItemsInQueue--;

    //Retorna o item que foi retirado
    return itemRemoved;
}

// ----------------------------------------------------
// Função que insere os jogos lidos do csv na hashTable
// ----------------------------------------------------
void readGamesFromCSV()
{   
    // Abre o ficheiro jogos.csv para leitura
    FILE *file = fopen("Servidor/jogos.csv", "r");

    //Verifica se o ficheiro falhou ao abrir , caso tiver falhado termina a função
    if (file == NULL)
    {
        printf("Erro : Não foi possível abrir o ficheiro onde estão armazenados os jogos\n");
        return;
    }

    //Buffer que será utilizado para ler a linha do csv
    char line[MAX_LINE_SIZE_IN_CSV];

    //Lê o ficheiro linha a linha
    while (fgets(line, MAX_LINE_SIZE_IN_CSV, file))
    {   
        //Divide cada linha por vírgulas
        //1. Id do jogo
        //2. Solução parcial
        //3. Solução completa
        char *dividedLine = strtok(line, ",");  // Separa usando vírgula
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

        //Adiciona o jogo à hashTable
        HASH_ADD_INT(gamesHashTable, id, currentGame);
    }

    //Fecha o ficheiro
    fclose(file);
}

// --------------------------------------
// Função que envia o jogo para o cliente
// --------------------------------------
void sendGameToClient(int socket)
{   
    printf("Thread produtora %d entrou na função de enviar o jogo para o cliente\n" , pthread_self());

    //Conta quantos jogos existem armazenados no servidor
    int numberOfGamesInServer = HASH_COUNT(gamesHashTable);

    //Verifica se não existe nenhum jogo , caso não existir termina o processo
    if(numberOfGamesInServer == 0)
    {
        perror("Error : Server could not find any games in the data base");
        exit(1);
    }

    //Escolhe um jogo aleatório
    int index = rand() % numberOfGamesInServer;
    int i = 0;

    gameData *game;

    for(game = gamesHashTable ; game != NULL ; game = game->hh.next)
    {   
        //Percorre a hashTable até chegar ao índice escolhido
        if(i == index)
        {   
            //Prepara a mensagem para enviar ao cliente com o formato "id,solução parcial\n"
            char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

            snprintf(messageToClient , sizeof(messageToClient) , "%d,%s\n" , game->id , game->partialSolution);
            printf("Mensagem que será enviada para o cliente pela thread produtora %d -> %s\n" , pthread_self() , messageToClient);

            //Envia a mensagem para o cliente , caso esta falhar termina o processo
            if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
            {
                perror("Error : Server could not sent game to client or client disconnected from the socket");
                exit(1);            
            }

            printf("Thread produtora %d enviou o jogo com sucesso\n" , pthread_self());

            break;
        }

        i++;
    }

    printf("Thread produtora %d finalizou a função responsável por enviar o jogo ao cliente\n" , pthread_self());
}

// -----------------------------------------
// Função que verifica a resposta do cliente
// -----------------------------------------
void verifyClientPartialSolution(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer)
{   
    gameData *gameSentToClient;

    //Utiliza o mutex para proteger o acesso à hashTable
    pthread_mutex_lock(&mutexGameData);

    //Procura o jogo na hashTable utilizando o seu id
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient);
    pthread_mutex_unlock(&mutexGameData);
    
    //Prepara a mensagem para enviar ao cliente com o formato "Correct\n"
    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    //Verifica se foi encontrado o jogo , caso não tiver sido envia uma mensagem de erro ao cliente
    if (!gameSentToClient)
    {
        strcpy(messageToClient , "Error : Server could not find a game with the id sent from the client\n");
        printf("A thread consumidora %d não encontrou um jogo com esse id\n" , pthread_self());
    }
    
    //Verifica se as coordenas de resposta foram válidas , caso não tiverem sido envia uma mensagem de erro ao cliente
    else if (rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8)
    {
        strcpy(messageToClient , "Error : Server could not process the sudoku coordinates sent from the client\n");
        printf("A thread consumidora %d recebeu coordenadas de tentativa inválidas\n" , pthread_self());
    }

    //Verifica se a resposta estava correta ou errada
    else
    {   
        gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0' ? strcpy(messageToClient, "Correct\n") : strcpy(messageToClient, "Wrong\n");
        printf("A thread consumidora %d avaliou a tentativa do cliente e esta estava %s\n" , pthread_self() , messageToClient);
    }

    //Utiliza o mutex para proteger o acesso ao socket
    pthread_mutex_lock(&mutexSocket);

    printf("A thread consumidora %d está a preparar-se para enviar a resposta da tentativa ao cliente\n" , pthread_self());

    //Envia a mensagem para o cliente , caso der erro ao enviar a mensagem termina o processo
    if (writeSocket(socket, messageToClient, strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client partial solution or the client disconnected from the socket");
        exit(1);
    }

    pthread_mutex_unlock(&mutexSocket);

    printf("A thread consumidora %d enviou a resposta ao cliente com sucesso\n" , pthread_self());
}

void verifyClientCompleteSolution(int socket , int gameId , char completeSolution[82])
{
    gameData *gameSentToClient;

    //Utiliza o mutex para proteger o acesso à hashTable
    pthread_mutex_lock(&mutexGameData);

    //Procura o jogo na hashTable utilizando o seu id
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient);
    pthread_mutex_unlock(&mutexGameData);

    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    //Verifica se foi encontrado o jogo , caso não tiver sido envia uma mensagem de erro ao cliente
    if (!gameSentToClient)
    {
        strcpy(messageToClient , "Error : Server could not find a game with the id sent from the client\n");
        printf("A thread consumidora %d não encontrou um jogo com esse id\n" , pthread_self());
    }

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
                    exit(1);
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
        exit(1);
    }
}

// --------------------------------------------------
// Função que possui a lógica do produtor do servidor
// --------------------------------------------------
void *producer(void *arguments)
{   

    //Converte os arguments do tipo void* para producerAndConsumerArguments através de casting
    producerAndConsumerArguments *dataInArguments = (producerAndConsumerArguments*)arguments;

    //Obtém a referência da fila FIFO e o socket a ser utilizado na comunicação 
    int socket = dataInArguments->socket;
    queueFifo *queue = dataInArguments->queue;
    queueFifo *priorityQueue = dataInArguments->priorityQueue;
    

    fd_set socketReadyToRead;
    struct timeval timeToReadAgain;

    timeToReadAgain.tv_sec = 0;
    timeToReadAgain.tv_usec = 100;

    while(1)
    {   
        //Prepara o buffer para ler a mensagem do cliente no formato "codigo,...\n"
        char messageFromClient[BUFFER_SOCKET_MESSAGE_SIZE];

        FD_ZERO(&socketReadyToRead);
        FD_SET(socket , &socketReadyToRead);

        //Utiliza o mutex para proteger o acesso ao socket
        pthread_mutex_lock(&mutexSocket);

        select(socket + 1, &socketReadyToRead , NULL , NULL , &timeToReadAgain);

        if(FD_ISSET(socket , &socketReadyToRead))
        {   
            printf("Thread produtora %d está pronta para ler o pedido do cliente\n" , pthread_self());

            //Recebe o número de bytes que foram lidos da mensagem do cliente
            int bytesReceived = readSocket(socket , messageFromClient , sizeof(messageFromClient));

            //Caso o número de bytes for menor que 0 termina o processo
            if(bytesReceived <= 0)
            {
                perror("Error : Server could not read the request of the client or the client disconnected from the socket");
                exit(1);
            }

            printf("Mensagem lida pela thread produtora %d -> %s\n" , pthread_self() , messageFromClient);
            pthread_mutex_unlock(&mutexSocket);
        }
        else
        {
            pthread_mutex_unlock(&mutexSocket);
            continue;
        }

        //Lê o código da mensagem do cliente
        int codeOfClientMessage = atoi(messageFromClient);

        //Caso o código for 1 envia um jogo ao cliente
        if(codeOfClientMessage == 1)
        {   
            sendGameToClient(socket);
            printf("A thread produtora %d acabou de processar o pedido de código 1 do cliente\n" , pthread_self());
            continue;
        }
        
        if(codeOfClientMessage == 2)
        {   
            //Recebe o que restou da mensagem para além do código
            char *restOfClientMessage = strchr(messageFromClient, ',');

            //Caso não tiver resto envia para o cliente uma mensagem de erro
            if(!restOfClientMessage)
            {   
                perror("Error : Server could not process the format of the client answer regarding the lack of ',' after the code or the client disconnected from the socket");
                exit(1);
            }

            restOfClientMessage++;

            char bufferCopy[BUFFER_SOCKET_MESSAGE_SIZE];
            strncpy(bufferCopy, restOfClientMessage, sizeof(bufferCopy));
            bufferCopy[sizeof(bufferCopy)-1] = '\0';

            int typeOfClientAnswer = atoi(strtok(bufferCopy, ","));
            
            //Utiliza o mutex para proteger o acesso à fila FIFO
            pthread_mutex_lock(&mutexProducerConsumerQueueFifo);

            if(typeOfClientAnswer == 1)
            {
                //Cria a variável que irá conter as informações da tentativa do cliente
                queueEntry *queueFifoEntry = malloc(sizeof(queueEntry));
                if(!queueFifoEntry) { perror("queueEntry malloc failed"); exit(1); }

                //Lê o resto da mensagem do cliente e extrai as informações da tentativa , caso tiver acontecido algum erro envia uma mensagem para o cliente a avisar
                if(sscanf(restOfClientMessage , "1,%d,%d,%d,%d" , &queueFifoEntry->gameId , &queueFifoEntry->rowSelected , &queueFifoEntry->columnSelected , &queueFifoEntry->clientAnswer) != 4)
                {   
                    free(queueFifoEntry);
                    perror("Error : Server could not process the format of the client answer regarding the arguments of partial solution sent to the server or the client disconnected from the socket");
                    exit(1);
                }

                //Verifica se a fila está cheia , caso tiver blouqueia o processo à espera de um espaço vazia
                while(isQueueFull(queue))
                {
                    printf("A thread produtora %d irá bloquear-se pois a fila simples circular está cheia\n" , pthread_self());
                    pthread_cond_wait(&producerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);
                }

                //Adiciona o item ao fim da fila
                addItemToQueue(queue , queueFifoEntry);
                printf("A thread produtora %d acabou de adicionar o item à fila simples circular\n" , pthread_self());
            }
            else if(typeOfClientAnswer == 2)
            {
                priorityQueueEntry *queueFifoEntry = malloc(sizeof(priorityQueueEntry));
                if(!queueFifoEntry) { perror("priorityQueueEntry malloc failed"); exit(1); }

                if(sscanf(restOfClientMessage , "2,%d,%s" , &queueFifoEntry->gameId , &queueFifoEntry->clientCompleteSolution) != 2)
                {      
                    free(queueFifoEntry);
                    perror("Error : Server could not process the format of the client answer regarding the arguments of complete solution sent to the server or the client disconnected from the socket");
                    exit(1);
                }

                //Verifica se a fila está cheia , caso tiver blouqueia o processo à espera de um espaço vazia
                while(isQueueFull(priorityQueue))
                {
                    printf("A thread produtora %d irá bloquear-se pois a fila simples circular está cheia\n" , pthread_self());
                    pthread_cond_wait(&producerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);
                }

                //Adiciona o item ao fim da fila
                addItemToQueue(priorityQueue , queueFifoEntry);
                printf("A thread produtora %d acabou de adicionar o item à fila prioritaria circular\n" , pthread_self());
            }

            //Sinaliza um consumidor a avisar que a fila já não está mais vazia
            pthread_cond_signal(&consumerQueueFifoConditionVariable);
            pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);
            printf("A thread produtora %d acabou de processar o pedido de código 2 do cliente\n" , pthread_self());
        }
    }

    return NULL;
}

// --------------------------------------------------
// Função que possui a lógica do consumidor do servidor
// --------------------------------------------------
void *consumer(void *arguments)
{   
    //Converte os arguments do tipo void* para producerAndConsumerArguments através de casting
    producerAndConsumerArguments *dataInArguments = (producerAndConsumerArguments*)arguments;

    //Obtém a referência da fila FIFO e o socket a ser utilizado na comunicação 
    int socket = dataInArguments->socket;
    queueFifo *queue = dataInArguments->queue;
    queueFifo *priorityQueue = dataInArguments->priorityQueue; 

    while(1)
    {   
        //Utilizar o mutex para proteger o acesso à fila FIFO
        pthread_mutex_lock(&mutexProducerConsumerQueueFifo);
        
        while(isQueueEmpty(priorityQueue) && isQueueEmpty(queue))
            pthread_cond_wait(&consumerQueueFifoConditionVariable , &mutexProducerConsumerQueueFifo);

        if(!isQueueEmpty(priorityQueue))
        {
            priorityQueueEntry *item = (priorityQueueEntry*)removeItemFromQueue(priorityQueue);

            pthread_cond_signal(&producerQueueFifoConditionVariable);
            pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

            printf("A thread consumidora %d ira verificar a solucao completa %s do jogo %d\n" , pthread_self() , item->clientCompleteSolution , item->gameId);
            verifyClientCompleteSolution(socket , item->gameId , item->clientCompleteSolution);

            free(item);
        }
        else
        {
            queueEntry *item = (queueEntry*)removeItemFromQueue(priorityQueue);

            pthread_cond_signal(&producerQueueFifoConditionVariable);
            pthread_mutex_unlock(&mutexProducerConsumerQueueFifo);

            printf("A thread consumidora %d ira verificar a solucao parcial do jogo %d na celula %d %d com o valor %d\n" , pthread_self() , item->gameId , item->rowSelected , item->columnSelected , item->clientAnswer);
            verifyClientPartialSolution(socket , item->gameId , item->rowSelected , item->columnSelected , item->clientAnswer);

            free(item);
        }
    }

    return NULL;
}

// -----------------------------------------------------------------
// Função responsável por carregar a fila e inicializar as threads
// -----------------------------------------------------------------
void inicializeClientResponseThreads(int socket)
{   
    printf("Entrou na função que inicializa os threads do lado do servidor\n");
    //Cria a fila FIFO e inicializa-a
    queueFifo *queue = malloc(sizeof(queueFifo));
    queueFifo *priorityQueue = malloc(sizeof(queueFifo));

    inicializeQueue(queue);
    inicializeQueue(priorityQueue);

    //Inicializa os mutexes e as variáveis condicionais
    pthread_mutex_init(&mutexProducerConsumerQueueFifo , NULL);
    pthread_mutex_init(&mutexGameData , NULL);
    pthread_mutex_init(&mutexSocket , NULL);
    pthread_cond_init(&producerQueueFifoConditionVariable , NULL);
    pthread_cond_init(&consumerQueueFifoConditionVariable , NULL);

    //Inicializa o array que irá conter todos as threads dos produtores e consumidores
    pthread_t threads[NUMBER_OF_PRODUCERS + NUMBER_OF_CONSUMERS];

    //Prepara os argumentos necessários para o funcionamente das threads
    producerAndConsumerArguments *producerAndConsumerNeededArguments = malloc(sizeof(producerAndConsumerArguments));
    producerAndConsumerNeededArguments->socket = socket;
    producerAndConsumerNeededArguments->queue = queue;
    producerAndConsumerNeededArguments->priorityQueue = priorityQueue;

    //Cria todas as threads produtoras
    for(int i = 0 ; i < NUMBER_OF_PRODUCERS ; i++)
    {
        if(pthread_create(&threads[i] , NULL , producer , producerAndConsumerNeededArguments))
        {
            perror("Error : Server could not create a producer thread");
            exit(1);
        }
    }

    printf("Criou os %d threads produtores\n" , NUMBER_OF_PRODUCERS);

    //Cria todas as threads consumidoras
    for(int i = 0 ; i < NUMBER_OF_CONSUMERS ; i++)
    {
        if(pthread_create(&threads[i + NUMBER_OF_PRODUCERS] , NULL , consumer , producerAndConsumerNeededArguments))
        {
            perror("Error : Server could not create a consumer thread");
            exit(1);
        }
    }

    printf("Criou os %d threads consumidores\n" , NUMBER_OF_CONSUMERS);

    //Espera todas as threads termirarem as suas respetivas funções
    for(int i = 0 ; i < NUMBER_OF_PRODUCERS + NUMBER_OF_CONSUMERS ; i++)
        pthread_join(threads[i] , NULL);

    pthread_mutex_destroy(&mutexProducerConsumerQueueFifo);
    pthread_mutex_destroy(&mutexGameData);
    pthread_mutex_destroy(&mutexSocket);
    pthread_cond_destroy(&producerQueueFifoConditionVariable);
    pthread_cond_destroy(&consumerQueueFifoConditionVariable);

    //Liberta a memórias que foi usada para os argumentos dos produtores e consumidores e a fila
    free(producerAndConsumerNeededArguments);
    free(queue);

    printf("O cliente finalizou o jogo\n");
}