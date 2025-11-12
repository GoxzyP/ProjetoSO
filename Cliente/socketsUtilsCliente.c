#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util.h"
#include "socketsUtilsCliente.h"

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(char partialSolution[81], int rowSelected, int columnSelected , int *size)
{   
    printf("Entrou na função que consegue as respostas possíveis\n");
    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        int rowIndex = rowSelected * 9 + i;
        int columnIndex = i * 9 + columnSelected;

        // Se a linha já contém o número, remove da lista de possíveis
        if(partialSolution[rowIndex] != '0' && allPossibleAnswers[partialSolution[rowIndex] - '1'] != 0)
        {
            allPossibleAnswers[partialSolution[rowIndex] - '1'] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if(partialSolution[columnIndex] != '0' && allPossibleAnswers[partialSolution[columnIndex] - '1'] != 0)
        {
            allPossibleAnswers[partialSolution[columnIndex] - '1'] = 0; // lógica igual às linhas
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
            int index = (startRow + i) * 9 + (startColumn + j);

            if(partialSolution[index] != '0' && allPossibleAnswers[partialSolution[index] - '1'] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[partialSolution[index] - '1'] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
                arraySize--;                     // decrementa o número de respostas válidas
            }
        }
    }

    // Aloca memória para armazenar dinamicamente os números válidos que podem ser colocados na célula
    int *possibleAnswersBasedOnPlacement = malloc(arraySize * sizeof(int)); 
    if (!possibleAnswersBasedOnPlacement) 
    {
        perror("Error: Could not allocate memory for the dynamic array storing possible answers");
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

    printf("Finalizou a função que consegue as respostas possíveis\n");

    return possibleAnswersBasedOnPlacement; // retorna ponteiro para array dinâmico
}

// --------------------------------
// Display do tabuleiro no terminal
// --------------------------------
void displaySudokuWithCoords(char sudoku[81]) 
{
    // Cabeçalho com coordenadas das colunas (X)
    printf("xy "); // marcador no canto superior esquerdo
    for (int col = 0; col < 9; col++) {
        printf("%d", col); // imprime o número da coluna
        if ((col + 1) % 3 == 0 && col != 8)
            printf(" |"); // adiciona um separador entre blocos 3x3
        else if (col != 8)
            printf(" "); // adiciona espaço entre colunas
    }
    printf("\n");

    // Linha horizontal logo abaixo do cabeçalho
    printf("  ---------------------\n");

    // Itera sobre cada linha do Sudoku
    for (int row = 0; row < 9; row++) {
        printf("%d| ", row); // imprime o número da linha no lado esquerdo

        // Itera sobre cada coluna da linha
        for (int col = 0; col < 9; col++) {
            if (sudoku[row * 9 + col] == '0')
                printf("-");  // se for 0 então não há valor ainda, substituimos por "-"
            else
                printf("%c", sudoku[row * 9 + col]); // imprime o número presente na célula

            if ((col + 1) % 3 == 0 && col != 8)
                printf(" |"); // separador entre blocos 3x3
            else if (col != 8)
                printf(" ");  // espaço entre colunas
        }
        printf("\n");

        // Linha divisória horizontal entre blocos de 3 linhas
        if ((row + 1) % 3 == 0 && row != 8)
            printf("  ---------------------\n");
    }
}

// ------------------------------------------------------------------
// Mistura o array com o algoritmo de Fisher-Yates
// ------------------------------------------------------------------
void shufflePositionsInArray(int positions[][2] , int count) // funcao para baralhar as coordenadas num array
{
    for(int i = count - 1 ; i > 0 ; i--)
    {
        int j = rand() % (i + 1);

        int tempRow = positions[i][0];
        int tempColumn = positions[i][1];

        positions[i][0] = positions[j][0];
        positions[i][1] = positions[j][1];

        positions[j][0] = tempRow;
        positions[j][1] = tempColumn;
    }
}

// ------------------------------------------------------------------
// Preencher o sudoku
// ------------------------------------------------------------------
void fillSudoku(int socket , char sudoku[81] , int gameId) 
{   
    printf("Entrou na função de preencher o sudoku\n");

    int positionsToFill[81][2];
    int emptyCount = 0;

    for(int row = 0 ; row < 9 ; row++)
    {
        for(int column = 0 ; column < 9 ; column++)
        {
            if(sudoku[row * 9 + column] == '0')
            {
                positionsToFill[emptyCount][0] = row;
                positionsToFill[emptyCount][1] = column;
                emptyCount++;
            }
        }
    }

    shufflePositionsInArray(positionsToFill , emptyCount); // esta funcao faz com que os valores no array ( coordenadas vazias ) sejam baralhados

    printf("Fez o shuffle do array de posições por preencher\n");
    printf("Empty count inical -> %d\n" , emptyCount);

    for(int i = 0 ; i < emptyCount ; i++)
    {   
        printf("Empty left: %d\n", emptyCount - i - 1);
        int rowSelected = positionsToFill[i][0];
        int columnSelected = positionsToFill[i][1];

        int size = 0; 
        int* possibleAnswers = getPossibleAnswers(sudoku, rowSelected , columnSelected , &size); //valor de size (número de respostas válidas) vem da função getPossibleAnswers 

        printf("Possiveis valores para (%d,%d): ", rowSelected, columnSelected);
        for(int j = 0; j < size; j++) {printf("%d ", possibleAnswers[j] , " ");}//imprime valores possíveis para uma certa posição na célula
        printf("\n");
    
        for(int j = 0 ; j < size ; j++)
        {   
            char messageToServer[512];
            int codeVerifyAnswer = 2;
            sprintf(messageToServer , "%d,%d,%d,%d,%d\n" , codeVerifyAnswer , gameId , rowSelected , columnSelected , possibleAnswers[j]);

            printf("Foi enviado para o server o numero %d na posicao : %d %d \n" , possibleAnswers[j] , rowSelected , columnSelected);

            if((escreveSocket(socket , messageToServer , strlen(messageToServer))) != strlen(messageToServer))
            {
                perror("Error : Could not send possible answer to server");
                exit(1);
            }

            printf("O envio para o server do numero %d na posicao : %d %d foi feito com sucesso\n" , possibleAnswers[j] , rowSelected , columnSelected);

            char serverAnswer[512];
            int bytesReceived = lerLinha(socket , serverAnswer , sizeof(serverAnswer));

            if(bytesReceived  < 0)
            {
                perror("Error : Could not receive response from server about possible answer");
                exit(1);
            }

           serverAnswer[bytesReceived - 1] = '\0';

            if(strcmp(serverAnswer,"Correct") == 0)
            {   
                printf("A resposta enviada para o servidor estava correta\n");
                sudoku[rowSelected * 9 + columnSelected] = possibleAnswers[j] + '0';
                displaySudokuWithCoords(sudoku);
                break;
            }
            else
                printf("A resposta enviada para o servidor estava errada\n");
        }

        free(possibleAnswers);
    }
}

void requestGame(int socket , int* gameId , char partialSolution[81])
{   
    printf("Entrou na função de pedido de jogo ao servidor\n");

    char messageToServer[512];
    int codeRequestGame = 1;

    sprintf(messageToServer , "%d,\n" , codeRequestGame);

    if((escreveSocket(socket , messageToServer , strlen(messageToServer))) != strlen(messageToServer))
    {
        perror("Error : Could not send a game request to server");
        exit(1);
    }

    printf("Escreveu com sucesso o pedido de request do jogo\n");

    char serverAnswer[512];
    int bytesReceived = lerLinha(socket , serverAnswer , sizeof(serverAnswer));

    if(bytesReceived  < 0 )
    {
        perror("Error : Could not receive response from server about a game request");
        exit(1);
    }

    printf("Received a response from server about the game request\n");

    char *dividedLine = strtok(serverAnswer , ",");

    if(dividedLine == NULL)
    {
        perror("Error : Invalid server response format in id");
        exit(1);
    }

    *gameId = atoi(dividedLine);

    printf("O game id recebido foi %d\n" , *gameId);

    dividedLine = strtok(NULL , ",");

    if(dividedLine == NULL)
    {
        perror("Error : Invalid server response format in partial solution");
        exit(1);
    }

    dividedLine[strcspn(dividedLine, "\n")] = '\0';

    if(strlen(dividedLine) != 81)
    {
        perror("Error: partialSolution received from server has invalid length");
        exit(1);
    }

    strcpy(partialSolution , dividedLine);

    printf("Recebeu o jogo perfeitamente\n");
}