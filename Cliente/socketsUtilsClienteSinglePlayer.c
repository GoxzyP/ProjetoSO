//Util.h é onde está definido as funções de leitura e escrita dos sockets
#include "../util.h"

//SocketsUtilsCliente.h é onde está definido as funções que tanto o multiplayer quanto o singleplayer iram partilhar
#include "socketsUtilsCliente.h"

//Stdio.h é onde está definido as funções de input e output
#include <stdio.h>

//String,h é onde está definido as funções de manipulação de string
#include <string.h>

//BUFFER_PRODUCER_CONSUMER_SIZE define o tamanho do buffer que irá ser partilhado pelos produtores e consumidores
#define BUFFER_PRODUCER_CONSUMER_SIZE 5

//NUMBER_OF_PRODUCERS define o número de threads produtoras que o nosso código irá gerar
#define NUMBER_OF_PRODUCERS 1

//NUMBER_OF_CONSUMERS define o número de threads produtoras que o nosso código irá gerar
#define NUMBER_OF_CONSUMERS 1

//ProducerConsumerBuffer define a estrutura do buffer partilhado entre os produtores e consumidores
//RowSelected - Linha utilizada na tentativa produzida pelo produtor
//ColumnSelected - Coluna utilizada na tentativa produzida pelo produtor
//ClientAnswer - Valor que o produtor quer testar para a célula
typedef struct ProducerConsumerBuffer
{
    int rowSelected;
    int columnSelected;
    int clientAnswer;
}producerConsumerBuffer;

//ProducerArguments define a estrutra de argumentos passados para cada thread de produtor
//Sudoku - Recebe a referência do jogo
//EmptyPosition - Recebe a referência ao array onde estão armazenados as posições vazias
//NumberOfEmptyPositions - Número total de posições vazias
typedef struct ProducerArguments
{
    sudokuCell (*sudoku)[9][9];
    int (*emptyPositions)[81][2];
    int numberOfEmptyPositions;
}producerArguments;

//ConsumerArguments define a estrutra de argumentos passados para cada thread de consumidor
//Sudoku - Recebe a referência do jogo
//Socket - Socket onde está a ser feita a comunicação cliente/servidor
//GameId - Número do identificador do jogo
typedef struct ConsumerArguments
{
    int socket;
    int gameId;
    sudokuCell (*sudoku)[9][9];
}consumerArguments;

//Buffer circular para a comunicação entre produtores e consumidores
producerConsumerBuffer buffer[BUFFER_PRODUCER_CONSUMER_SIZE];

//Índices circulares utilizados no consumo e produção e o número total de items no buffer
int bufferProducerIndex = 0 ; int bufferConsumerIndex = 0 ; int numberOfItemsInsideBuffer = 0;

//Mutexes e variáveis condicionais que serão utilizados na sicronização do buffer produtores/consumidores
pthread_mutex_t mutexProducerConsumerBuffer;
pthread_cond_t producerBufferConditionVariable ; pthread_cond_t consumerBufferConditionVariable;

//Contador de células vazias do sudoku utilizado para terminar as threads de consumidor
int allProducersFinished = 0; int numberOfProducersFinished = 0;

//Mutexe utilizado para gerir a utilização do socket
pthread_mutex_t mutexSocket;

// -----------------------------------------------------------------------------------
// Função que inicializa o sudoku onde cada célula tem o seu valor,estado,mutex e cond 
// -----------------------------------------------------------------------------------
int loadSudoku(sudokuCell sudoku[9][9] , char partialSolution[82] , int emptyPositions[81][2])
{   
    printf("Entrou na função responsável por incializar o sudoku\n");
    //Recebe o tamanho do array de caracteres
    int partialSolutionSize = strlen(partialSolution);

    printf("Recebeu o tamanho da solução parcial do sudoku\n");

    //Inicializa as variáveis que iremos usar como indexes para o array bidimensional
    int cellLine = 0;
    int cellColumn = 0;
    int emptyPositionsCount = 0;

    for(int i = 0 ; i < partialSolutionSize ; i++)
    {   
        //Pega o endereço de uma das células do sudoku
        sudokuCell *cell = &sudoku[cellLine][cellColumn];

        //Preenche os valores da células , subtraimos o value por '0' para passar de ASCII para int , inicializamos o mutex e o cond 
        //e definimos o estado , se o valor seja 0 significa que ainda precisa ser preenchido logo vai para idle caso contrário assume que a célula já veio preenchida
        cell->value = partialSolution[i] - '0';
        pthread_mutex_init(&cell->mutex , NULL);
        pthread_cond_init(&cell->conditionVariable , NULL);
        cell->state = cell->value == 0 ? IDLE : FINISHED;
        cell->producer = 0;

        if(cell->value == 0)
        {
            emptyPositions[emptyPositionsCount][0] = cellLine;
            emptyPositions[emptyPositionsCount][1] = cellColumn;
            emptyPositionsCount++;
        }

        //Incrementa a coluna
        cellColumn++;
        
        //Caso a coluna seja igual a 9 após a incrementação significa que já chegamos ao final da linha , ent zeramos a coluna e passamos para a próxima linha
        if(cellColumn == 9)
        {
            cellLine++;
            cellColumn = 0;
        }
    }

    printf("Todas as posições vazias são %d -> { " , emptyPositionsCount);

    for(int i = 0 ; i < emptyPositionsCount ; i++)
        printf("[%d , %d] " , emptyPositions[i][0] , emptyPositions[i][1]);

    printf("}\n");fflush(stdout);

    printf("Finalizou com sucesso a função responsável por inicializar o sudoku\n");

    return emptyPositionsCount;
}

// --------------------------------------
// Função que contém a lógica do produtor
// --------------------------------------
void *producer(void *arguments)
{   
    printf("A thread produtora %d entrou na função do producer\n" , pthread_self());
    //Converte os arguments do tipo void* para producerArguments através de casting
    producerArguments *dataInArguments = (producerArguments *)arguments;

    //Obtém a referência do sudoku , o array onde estão contidas as posições vazias(coordenadas) e o número de posições por preencher
    sudokuCell (*sudoku)[9][9] = dataInArguments->sudoku;
    int (*emptyPositions)[81][2] = dataInArguments->emptyPositions;
    int numberOfEmptyPositions = dataInArguments->numberOfEmptyPositions;
    printf("A thread produtora %d carregou as variáveis necessárias para a função\n" , pthread_self());

    //Itera sobre as posições vazias
    for(int i = 0 ; i < numberOfEmptyPositions ; i++)
    {   
        //Pega as coordenadas da célula que é preciso ser preenchida e já inicializa o tamanho do array que irá conter o número de respostas possíveis
        int size = 0;
        int rowSelected = (*emptyPositions)[i][0];
        int columnSelected = (*emptyPositions)[i][1];

        printf("A thread produtora %d irá tentar entrar na cell %d %d \n" , pthread_self() , rowSelected , columnSelected);
        //Obtém a referência da célula que precisa ser preenchida
        sudokuCell *cell = &(*sudoku)[rowSelected][columnSelected];

        //Tenta bloquear o mutex , caso algum produtor já esteja a trabalhar nesta célula passa a frente e tenta outras
        if(pthread_mutex_trylock(&cell->mutex) != 0)
            continue;

        printf("A thread produtora %d conseguiu dar lock com sucesso na cell %d %d \n" , pthread_self() , rowSelected , columnSelected);

        //Caso conseguir verificar o mutex verifica se foi o primeiro produtor a chegar à célula caso não for liberta o mutex e tenta outras células
        if(cell->producer != 0)
        {   
            printf("A thread produtora %d verificou que cell %d %d já tinha dono \n" , pthread_self() , rowSelected , columnSelected);
            pthread_mutex_unlock(&cell->mutex);
            continue;
        }

        //Obtém os valores possíveis para a célula atual
        int* possibleAnswers = getPossibleAnswers(sudoku , rowSelected , columnSelected , &size);//Adaptar função
        printf("A thread produtora %d pegou as respostas possíveis com sucesso da cell %d %d \nRespostas possíveis são -> [" , pthread_self() , rowSelected , columnSelected);
        
        for(int i = 0 ; i < size ; i++)
        {
            printf("%d" , possibleAnswers[i]);
            if(i < size - 1)
                printf(",");
        }

        printf("]\n");

        //Itera sobre os valores possíveis até encontrar o certo
        for(int j = 0 ; j < size ; j++)
        {   
            //A variável operationSucess será utilizado para reenviar a mesma tentativa para o buffer caso tiver acontecido algum erro na comunicação com o server
            int operationSucess = 0;

            //Marca que esta célula só poderá ser trabalhada pela thread produtora atual
            cell->producer = pthread_self();

            //Prepara a entrada a colocar no buffer de produtor/consumidor que contém os dados explicado na struct acima
            producerConsumerBuffer bufferEntry;
            bufferEntry.rowSelected = rowSelected;
            bufferEntry.columnSelected = columnSelected;
            bufferEntry.clientAnswer = possibleAnswers[j];
            printf("A thread produtora %d criou a entrada do buffer com sucesso\n" , pthread_self());

            //Começa um loop até a tentativa ser processada corretamente pelo server
            while(operationSucess == 0)
            {   
                printf("A thread produtora %d fechou o mutex do buffer produtor/consumidor\n" , pthread_self());
                //Fecha o mutex que controla o buffer produtor/consumidor antes de inserir o valor
                pthread_mutex_lock(&mutexProducerConsumerBuffer);

                //Verifica se o buffer está cheio , caso tiver liberta o mutex e bloqueia a tarefa atual até receber um sinal do consumidor
                while(numberOfItemsInsideBuffer == BUFFER_PRODUCER_CONSUMER_SIZE)
                {   
                    printf("A thread produtora %d vai bloquear porque o buffer produtor/consumidor está cheio\n" , pthread_self());
                    pthread_cond_wait(&producerBufferConditionVariable , &mutexProducerConsumerBuffer);
                }
                
                //Atualiza o estado da célula atual indicando que a thread está à espera de uma validação
                cell->state = WAITING;

                //Insere a entrada no buffer produtor/consumidor atualizando o index dos produtores e o número de items no buffer
                buffer[bufferProducerIndex] = bufferEntry;
                bufferProducerIndex = (bufferProducerIndex + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
                numberOfItemsInsideBuffer++;
                printf("A thread produtora %d colocou a entrada no buffer e colocou o estado à espera\n" , pthread_self());

                //Sinaliza um consumidor que um item foi adiciona e liberta o mutex do buffer produtor/consumidor
                pthread_cond_signal(&consumerBufferConditionVariable);
                pthread_mutex_unlock(&mutexProducerConsumerBuffer);
                printf("A thread produtora %d abriu o mutex do buffer produtor/consumidor e sinalizou o consumidor\n" , pthread_self());

                //Verifica se o consumidor ainda está a verificar esta célula , caso tiver liberta o mutex e bloqueia a tarefa atual até receber um sinal do consumidor
                while(cell->state == WAITING)
                {
                    printf("A thread produtora %d vai bloquear à espera do consumidor\n" , pthread_self());
                    pthread_cond_wait(&cell->conditionVariable , &cell->mutex);
                }

                printf("A thread produtora %d foi desbloqueada pelo consumidor\n" , pthread_self());

                //Verifica se a tentativa foi válida e não aconteceu nenhum problema na comunicação com o servidor
                if(cell->state == FINISHED || cell->state == IDLE)
                    operationSucess = 1;
            }

            //Verifica se a tentaiva enviada estava correta , caso a tentativa estiver correta liberta o mutex e sinal do ciclo de respostas possíveis para passar para outra célula
            if(cell->state == FINISHED)
            {   
                printf("A thread produtora %d preencheu a célula %d %d com sucesso\n" , pthread_self() , rowSelected , columnSelected);
                pthread_mutex_unlock(&cell->mutex);
                break;
            }
        }

        //Liberta a memória do array que contém as possíveis respostas
        free(possibleAnswers);
    } 
    
    pthread_mutex_lock(&mutexProducerConsumerBuffer);
    numberOfProducersFinished++;

    if(numberOfProducersFinished == NUMBER_OF_PRODUCERS)
    {   
        printf("Todas as threads produtoras finalizaram a sua respetiva função\n");
        allProducersFinished = 1;
        pthread_cond_broadcast(&consumerBufferConditionVariable);
    }

    pthread_mutex_unlock(&mutexProducerConsumerBuffer);

    //Termina a thread produtor
    return NULL;
}

// --------------------------------------
// Função que contém a lógica do consumidor
// --------------------------------------
void *consumer(void *arguments)
{   
    printf("A thread consumidora %d entrou na função do consumer\n" , pthread_self());
    //Converte os arguments do tipo void* para consumerArguments através de casting
    consumerArguments *dataInArguments = (consumerArguments*)arguments;

    //Obtém a referência do sudoku , o id do jogo atual e o socket que está ser utilizado na comunicação servidor/cliente
    int socket = dataInArguments->socket;
    int gameId = dataInArguments->gameId;
    sudokuCell (*sudoku)[9][9] = dataInArguments->sudoku;
    printf("A thread consumidora %d carregou as variáveis necessárias para a função\n" , pthread_self());

    //Define o código que será usado pelo servidor para identificar a mensagem do cliente
    int codeToVerifyAnswer = 2;

    //Loop até que o sudoku seja completamente preenchido
    while(1)
    {   
        //Bloqueia o mutex responsável pelo buffer produtor/consumidor
        pthread_mutex_lock(&mutexProducerConsumerBuffer);
        printf("A thread consumidora %d bloqueou o mutex do buffer produtor/consumidor\n" , pthread_self());

        //Verifica se o buffer está vazio , se estiver desbloqueia o mutex e bloqueia a thread atual até que receba um sinal de um produtor
        while(numberOfItemsInsideBuffer == 0 && !allProducersFinished)
        {   
            pthread_cond_wait(&consumerBufferConditionVariable , &mutexProducerConsumerBuffer);
            printf("A thread consumidora %d bloqueou porque o buffer produtor/consumidor está vazio\n" , pthread_self());
        }

        if(numberOfItemsInsideBuffer == 0 && allProducersFinished)
        {   
            pthread_mutex_unlock(&mutexProducerConsumerBuffer);
            break;
        }

        if(numberOfItemsInsideBuffer > 0)
        {
            //Retira o item do buffer atualizando o índice dos consumidores e o número de items dentro do buffer
            producerConsumerBuffer bufferEntry = buffer[bufferConsumerIndex];
            bufferConsumerIndex = (bufferConsumerIndex + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
            numberOfItemsInsideBuffer--;
            printf("A thread consumidora %d retirou uma entrada do buffer\n" , pthread_self());

            //Sinaliza um produtor que já tem espaço disponível no buffer para outro item e liberta o mutex responsável pelo buffer produtor/consumidor
            pthread_cond_signal(&producerBufferConditionVariable);
            pthread_mutex_unlock(&mutexProducerConsumerBuffer);
            printf("A thread consumidora %d desbloqueou o buffer produtor/consumidor e sinalizou um produtor\n" , pthread_self());

            //Prepara a mensagem para enviar ao servidor com o formato "codigo,id do jogo,linha,coluna,resposta do cliente\n"
            char messageToServer[BUFFER_SOCKET_MESSAGE_SIZE];
            sprintf(messageToServer , "%d,%d,%d,%d,%d\n" , codeToVerifyAnswer , gameId , bufferEntry.rowSelected , bufferEntry.columnSelected , bufferEntry.clientAnswer);

            //Bloqueia o mutex responsável pela comunicação servidor/cliente utilizando o socket
            pthread_mutex_lock(&mutexSocket);

            printf("A thread consumidora %d está a prepar-se para enviar uma tentativa para o server\n" , pthread_self());
            //Envia a tentativa ao servidor e termina o processo caso esta comunicação falhar
            if(writeSocket(socket , messageToServer , strlen(messageToServer)) != strlen(messageToServer))
            {
                perror("Error : Client could not write the answer to the server or the server disconnected from the socket");
                exit(1);
            }

            printf("A thread consumidora %d enviou a tentativa para o server com sucesso\n" , pthread_self());

            //Recebe a resposta do servidor
            char responseFromServer[BUFFER_SOCKET_MESSAGE_SIZE];
            int bytesReceived = readSocket(socket , responseFromServer , sizeof(responseFromServer));

            //Verifica se ocorreu algum erro na leitura da resposta do servidor terminando o programa caso tiver acontecido
            if(bytesReceived <= 0)
            {
                perror("Error : Client could not read the answer from the server or the server disconnected from the socket");
                exit(1);
            }

            printf("A thread consumidora %d recebeu a resposta da tentativa corretamente\n" , pthread_self());

            //Desbloqueia o mutex responsável pela comunicação servidor/cliente utilizando o socket
            pthread_mutex_unlock(&mutexSocket);

            //Converte a resposta do servidor para o uma string terminada em \0 para utilizar funções da biblioteca string.h
            responseFromServer[bytesReceived - 1] = '\0';

            //Obtém a referência e bloqueia o mutex da célula da tentativa atual
            sudokuCell *cell = &(*sudoku)[bufferEntry.rowSelected][bufferEntry.columnSelected];
            pthread_mutex_lock(&cell->mutex);

            //Verifica se ocorreu algum erro na formatação da mensagem enviada para o servidor , caso tiver muda o estado da célula para FAILED
            if(strncmp(responseFromServer , "Error" , 5) == 0)
            {   
                printf("A thread consumidora %d detetou um erro na tentativa\n" , pthread_self());
                cell->state = FAILED;
            }
            else
            {   
                //Verifica se a resposta estava correta atualizando o estado da célula para Finished e o colocando o respetivo valor correto
                if(strcmp(responseFromServer , "Correct") == 0 )
                {   
                    printf("A thread consumidora %d detetou que a tentativa estava correta\n" , pthread_self());

                    cell->state = FINISHED;
                    cell->value = bufferEntry.clientAnswer;
                    displaySudokuWithCoords(sudoku);
                }
                else
                {   
                    printf("A thread consumidora %d detetou que a tentativa estava errada\n" , pthread_self());

                    //Caso a resposta esteja errada modifica o estado da célula para idle indicando que é preciso tentar outra vez
                    cell->state = IDLE;
                }
            }

            //Sinaliza o produtor da célula que terminou de processar a tentativa e liberta o mutex da célula
            pthread_cond_signal(&cell->conditionVariable);
            pthread_mutex_unlock(&cell->mutex);

            printf("A thread consumidora %d acabou de processar o item e liberou o produtor da cell\n" , pthread_self());
        }
        else
            pthread_mutex_unlock(&mutexProducerConsumerBuffer);
    }

    //Termina a thread consumidor
    return NULL;
}

// -----------------------------------------------------------------
// Função responsável por carregar o sudoku e inicializar as threads
// -----------------------------------------------------------------
void inicializeGame(int socket , int gameId , char partialSolution[82])
{    
    printf("Começou a função responsável por inicializar o jogo\n");
    //Inicializa o sudoku e o array que irá conter as coordenadas das posições vazias
    sudokuCell sudoku[9][9];
    int emptyPositions[81][2];

    //Carrega um sudoku e calcula o numero de posições por preencher
    int numberOfEmptyPositions = loadSudoku(sudoku , partialSolution , emptyPositions);
    printf("Conseguiu inicializar o sudoku e precisa preencher %d posições\n" , numberOfEmptyPositions);
    
    //Mistura o array de posições vazias para adicionar para o sudoku ser resolvido de uma maneira randómica
    shufflePositionsInArray(emptyPositions , numberOfEmptyPositions);
    printf("Conseguiu misturar as posições vazias para preencher o sudoku de maneira random\n");

    //Inicializa os mutexes e as variáveis condicionais
    pthread_mutex_init(&mutexProducerConsumerBuffer , NULL);
    pthread_mutex_init(&mutexSocket , NULL);
    pthread_cond_init(&producerBufferConditionVariable , NULL);
    pthread_cond_init(&consumerBufferConditionVariable , NULL);
    printf("Inicializou os mutexes e variáveis condicionais\n");

    //Inicializa o array que irá conter todos as threads dos produtores e consumidores
    pthread_t threads[NUMBER_OF_PRODUCERS + NUMBER_OF_CONSUMERS];

    //Prepara os argumentos necessários para o funcionamente das threads produtoras
    producerArguments *producerNeededArguments = malloc(sizeof(producerArguments));
    producerNeededArguments->sudoku = &sudoku;
    producerNeededArguments->emptyPositions = &emptyPositions;
    producerNeededArguments->numberOfEmptyPositions = numberOfEmptyPositions;
    printf("Alocou as informações necessários dos produtores na struct\n");

    //Cria todas as threads produtoras
    for(int i = 0 ; i < NUMBER_OF_PRODUCERS ; i++)
    {
        if(pthread_create(&threads[i] , NULL , producer , producerNeededArguments))
        {
            perror("Error : Client could not create a producer thread");
            exit(1);
        }
    }

    printf("Criou os %d threads produtoras\n" , NUMBER_OF_PRODUCERS);

    //Prepara os argumentos necessários para o funcionamente das threads consumidoras
    consumerArguments *consumerNeededArguments = malloc(sizeof(consumerArguments));
    consumerNeededArguments->gameId = gameId;
    consumerNeededArguments->socket = socket;
    consumerNeededArguments->sudoku = &sudoku;
    printf("Alocou as informações necessários dos consumidores na struct\n");

    //Cria todas as threads consumidoras
    for(int i = 0 ; i < NUMBER_OF_CONSUMERS ; i++)
    {
        if(pthread_create(&threads[i + NUMBER_OF_PRODUCERS] , NULL , consumer , consumerNeededArguments))
        {
            perror("Error : Client could not create a consumer thread");
            exit(1);
        }
    }

    printf("Criou os %d threads consumidores\n" , NUMBER_OF_CONSUMERS);

    //Espera todas as threads termirarem as suas respetivas funções
    for(int i = 0 ; i < NUMBER_OF_PRODUCERS + NUMBER_OF_CONSUMERS ; i++)
        pthread_join(threads[i] , NULL);

    printf("Todas as threads foram eliminadas corretamente\n");

    pthread_mutex_destroy(&mutexProducerConsumerBuffer);
    pthread_mutex_destroy(&mutexSocket);
    pthread_cond_destroy(&producerBufferConditionVariable);
    pthread_cond_destroy(&consumerBufferConditionVariable);

    //Liberta a memórias que foi usada para os argumentos dos produtores e consumidores
    free(producerNeededArguments);
    free(consumerNeededArguments);
}