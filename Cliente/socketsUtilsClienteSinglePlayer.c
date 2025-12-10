#include "unix.h"
#include <pthread.h>

#include <stdio.h>
#include <string.h>

#define BUFFER_PRODUCER_CONSUMER_SIZE 5
#define BUFFER_SOCKET_MESSAGE_SIZE 512

enum cellState 
{
    IDLE ,
    WAITING ,
    FAILED ,
    FINISHED
};

typedef struct SudokuCell 
{
    int value;
    pthread_t producer;
    pthread_mutex_t mutex;
    pthread_cond_t conditionVariable;
    enum cellState state;
}sudokuCell;

typedef struct ProducerConsumerBuffer
{
    int rowSelected;
    int columnSelected;
    int clientAnswer;
}producerConsumerBuffer;


producerConsumerBuffer buffer[BUFFER_PRODUCER_CONSUMER_SIZE];
int bufferProducerIndex = 0 ; int bufferConsumerIndex = 0 ; int numberOfItemsInsideBuffer = 0;

pthread_mutex_t mutexProducerConsumerBuffer;
pthread_cond_t producerBufferConditionVariable ; pthread_cond_t consumerBufferConditionVariable;//Não esquecer de inicializer estas variaveis e o mutex do buffer

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(sudokuCell *sudoku[9][9] , int rowSelected, int columnSelected , int *size)
{   
    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        // Se a linha já contém o número, remove da lista de possíveis
        if(sudoku[rowSelected][i]->value != 0 && allPossibleAnswers[sudoku[rowSelected][i]->value - 1] != 0)
        {
            allPossibleAnswers[sudoku[rowSelected][i]->value - 1] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if(sudoku[i][columnSelected]->value != 0 && allPossibleAnswers[sudoku[i][columnSelected]->value - 1] != 0)
        {
            allPossibleAnswers[sudoku[i][columnSelected]->value - 1] = 0; // lógica igual às linhas
            arraySize--;
        }
    }

    // Determina o bloco 3x3 da célula selecionada
    int startRow = (rowSelected / 3) * 3;
    int startColumn = (columnSelected / 3) * 3;

    // Remove valores já presentes no bloco 3x3
    for(int i = 0 ; i < 3 ; i++)
    {
        for(int j = 0 ; j < 3 ; j++)
        {   
            if(sudoku[startRow + i][startColumn + j]->value != 0 && allPossibleAnswers[sudoku[startRow + i][startColumn + j]->value - 1] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[sudoku[startRow + i][startColumn + j]->value - 1] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
                arraySize--;                     // decrementa o número de respostas válidas
            }
        }
    }

    // Aloca memória para armazenar dinamicamente os números válidos que podem ser colocados na célula
    int *possibleAnswersBasedOnPlacement = malloc(arraySize * sizeof(int)); 
    if (!possibleAnswersBasedOnPlacement) 
    {
        perror("Erro : Não foi possível alocar memória no array dinâmico onde fica armazenado as respostas possíveis");
        exit(1); // termina programa se falhar a alocação
    }

    // Copia valores válidos para o array final
    int index = 0;
    for(int i = 0 ; i < 9 ; i++)
    {
        if(allPossibleAnswers[i] != 0)  //verifica se o valor é valido, se for 0 quer dizer que um certo valor foi excluido
            possibleAnswersBasedOnPlacement[index++] = allPossibleAnswers[i]; //array com as respostas válidas
    }
    *size = arraySize;  // armazena no endereço apontado por 'size' o número de respostas válidas encontradas

    return possibleAnswersBasedOnPlacement; // retorna ponteiro para array dinâmico
}

// -----------------------------------------------------------------------------------
// Função que inicializa o sudoku onde cada célula tem o seu valor,estado,mutex e cond 
// -----------------------------------------------------------------------------------
void inicializeSudoku(sudokuCell *sudoku[9][9] , char partialSolution[81])
{   
    //Recebe o tamanho do array de caracteres
    int partialSolutionSize = strlen(partialSolution);

    //Se o array de caracteres não for igual a 81 significa que o jogo foi mal passado
    if(partialSolutionSize != 81)
        return;

    //Inicializa as variáveis que iremos usar como indexes para o array bidimensional
    int cellLine = 0;
    int cellColumn = 0;

    for(int i = 0 ; i < partialSolutionSize ; i++)
    {   
        //Pega o endereço de uma das células do sudoku
        sudokuCell *cell = sudoku[cellLine][cellColumn];

        //Preenche os valores da células , subtraimos o value por '0' para passar de ASCII para int , inicializamos o mutex e o cond 
        //e definimos o estado , se o valor seja 0 significa que ainda precisa ser preenchido logo vai para idle caso contrário assume que a célula já veio preenchida
        cell->value = partialSolution[i] - '0';
        pthread_mutex_init(&cell->mutex , NULL);
        cell->state = cell->value == 0 ? IDLE : FINISHED;
        cell->producer = 0;

        //Incrementa a coluna
        cellColumn++;
        
        //Caso a coluna seja igual a 9 após a incrementação significa que já chegamos ao final da linha , ent zeramos a coluna e passamos para a próxima linha
        if(cellColumn == 9)
        {
            cellLine++;
            cellColumn = 0;
        }
    }
}

void producer(sudokuCell *sudoku[9][9] , int emptyPositions[][2] , int emptyPositionsSize)
{
    for(int i = 0 ; i < emptyPositionsSize ; i++)
    {   
        int size = 0;
        int rowSelected = emptyPositions[i][0];
        int columnSelected = emptyPositions[i][1];

        sudokuCell *cell = sudoku[rowSelected][columnSelected];

        if(pthread_mutex_trylock(&cell->mutex) != 0)
            continue;

        if(cell->producer != 0)
        {
            pthread_mutex_unlock(&cell->mutex);
            continue;
        }

        int* possibleAnswers = getPossibleAnswers(sudoku , rowSelected , columnSelected , &size);//Adaptar função

        for(int j = 0 ; j < size ; j++)
        {   
            int operationSucess = 0;
            cell->producer = pthread_self();

            producerConsumerBuffer bufferEntry;
            bufferEntry.rowSelected = rowSelected;
            bufferEntry.columnSelected = columnSelected;
            bufferEntry.clientAnswer = possibleAnswers[j];

            while(operationSucess == 0)
            {
                pthread_mutex_lock(&mutexProducerConsumerBuffer);

                while(numberOfItemsInsideBuffer == BUFFER_PRODUCER_CONSUMER_SIZE)
                    pthread_cond_wait(&producerBufferConditionVariable , &mutexProducerConsumerBuffer);
                
                cell->state = WAITING;

                buffer[bufferProducerIndex] = bufferEntry;
                bufferProducerIndex = (bufferProducerIndex + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
                numberOfItemsInsideBuffer++;

                pthread_cond_signal(&consumerBufferConditionVariable);
                pthread_mutex_unlock(&mutexProducerConsumerBuffer);

                while(cell->state == WAITING)
                    pthread_cond_wait(&cell->conditionVariable , &cell->mutex);

                if(cell->state == FINISHED || cell->state == IDLE)
                    operationSucess = 1;
            }

            if(cell->state == FINISHED)
            {
                pthread_mutex_unlock(&cell->mutex);
                break;
            }
        }

        free(possibleAnswers);
    }   
}

void consumer(int socket , sudokuCell *sudoku[9][9] , int gameId)
{   
    int codeToVerifyAnswer = 2;

    while(1)
    {
        pthread_mutex_lock(&mutexProducerConsumerBuffer);

        while(numberOfItemsInsideBuffer == 0)
            pthread_cond_wait(&consumerBufferConditionVariable , &mutexProducerConsumerBuffer);

        producerConsumerBuffer bufferEntry = buffer[bufferConsumerIndex];
        bufferConsumerIndex = (bufferConsumerIndex + 1) % BUFFER_PRODUCER_CONSUMER_SIZE;
        numberOfItemsInsideBuffer--;

        pthread_cond_signal(&producerBufferConditionVariable);
        pthread_mutex_unlock(&mutexProducerConsumerBuffer);

        char messageToServer[BUFFER_SOCKET_MESSAGE_SIZE];
        sprintf(messageToServer , "%d,%d,%d,%d,%d\n" , codeToVerifyAnswer , gameId , bufferEntry.rowSelected , bufferEntry.columnSelected , bufferEntry.clientAnswer);

        sudokuCell *cell = sudoku[bufferEntry.rowSelected][bufferEntry.columnSelected];
        pthread_mutex_lock(&cell->mutex);

        if(writeSocket(socket , messageToServer , strlen(messageToServer)) != srtlen(messageToServer))
        {
            cell->state = FAILED;
            pthread_cond_signal(&sudoku[bufferEntry.rowSelected][bufferEntry.columnSelected]->conditionVariable);
            pthread_mutex_unlock(&cell->mutex);
            continue;
        }

        char responseFromServer[BUFFER_SOCKET_MESSAGE_SIZE];
        int bytesReceived = readSocket(socket , responseFromServer , sizeof(responseFromServer));

        if(bytesReceived < 0)
        {
            cell->state = FAILED;
            pthread_cond_signal(&sudoku[bufferEntry.rowSelected][bufferEntry.columnSelected]->conditionVariable);
            pthread_mutex_unlock(&cell->mutex);
            continue;
        }

        responseFromServer[bytesReceived - 1] = '\0';

        cell->state = strcmp(responseFromServer , "Correct") == 0 ? FINISHED : IDLE;
        cell->value = strcmp(responseFromServer , "Correct") == 0 ? bufferEntry.clientAnswer : cell->value;
        pthread_cond_signal(&sudoku[bufferEntry.rowSelected][bufferEntry.columnSelected]->conditionVariable);
        pthread_mutex_unlock(&cell->mutex);
    }
}