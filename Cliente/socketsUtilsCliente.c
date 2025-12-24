#include "../util.h"
#include "socketsUtilsCliente.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --------------------------------
// Display do tabuleiro no terminal
// --------------------------------
void displaySudokuWithCoords(sudokuCell (*sudoku)[9][9]) 
{
    // Cabeçalho com coordenadas das colunas (X)
    printf("\n\nxy "); // marcador no canto superior esquerdo
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
            if ((*sudoku)[row][col].value == 0)
                printf("-");  // se for 0 então não há valor ainda, substituimos por "-"
            else
                printf("%d", (*sudoku)[row][col].value); // imprime o número presente na célula

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

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(sudokuCell (*sudoku)[9][9] , int rowSelected, int columnSelected , int *size)
{   
    printf("A thread produtora %d entrou com sucesso na função responsável por pegar as respostas possíveis para a célula %d %d \n" , pthread_self() , rowSelected , columnSelected);

    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        // Se a linha já contém o número, remove da lista de possíveis
        if((*sudoku)[rowSelected][i].value != 0 && allPossibleAnswers[(*sudoku)[rowSelected][i].value - 1] != 0)
        {
            allPossibleAnswers[(*sudoku)[rowSelected][i].value - 1] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if((*sudoku)[i][columnSelected].value != 0 && allPossibleAnswers[(*sudoku)[i][columnSelected].value - 1] != 0)
        {
            allPossibleAnswers[(*sudoku)[i][columnSelected].value - 1] = 0; // lógica igual às linhas
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
            if((*sudoku)[startRow + i][startColumn + j].value != 0 && allPossibleAnswers[(*sudoku)[startRow + i][startColumn + j].value - 1] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[(*sudoku)[startRow + i][startColumn + j].value - 1] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
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

void requestGame(int socket , int *gameId , char partialSolution[82])
{   
    printf("Entrou na função que irá pedir o jogo ao server\n");

    int operationSuccesss = 0;
    int codeRequestGame = 1;

    while(operationSuccesss == 0)
    {
        char messageToServer[BUFFER_SOCKET_MESSAGE_SIZE];

        sprintf(messageToServer , "%d,\n" , codeRequestGame);

        printf("Escreveu o pedido do jogo ao servidor\n");

        if(writeSocket(socket , messageToServer , strlen(messageToServer)) != strlen(messageToServer))
        {
            perror("Error : Client could not write game request to the server or the server disconnected from the socket");
            exit(1);
        }

        char messageFromServer[BUFFER_SOCKET_MESSAGE_SIZE];
        int bytesReceived = readSocket(socket , messageFromServer , sizeof(messageFromServer));

        printf("Leu a mensagem do servidor em relação ao pedido do jogo\n");

        if(bytesReceived <= 0)
        {
            perror("Error : Client could not read the game sent from the server or the server disconnected from the socket");
            exit(1);
        }

        printf("Mensagem recebida do servidor -> %s\n" , messageFromServer);

        char *dividedLine = strtok(messageFromServer , ",");

        if(dividedLine == NULL)
        {
            perror("Error : Client could not process format of the game sent from the server regarding the id");
            continue;
        }

        *gameId = atoi(dividedLine);

        dividedLine = strtok(NULL , ",");

        if(dividedLine == NULL)
        {
            perror("Error : Client could not process format of the game sent from the server regarding the partial solution");
            continue;
        }

        dividedLine[strcspn(dividedLine, "\n")] = '\0';

        if(strlen(dividedLine) != 81)
        {
            perror("Error : Wrong size in the partial solution sent from the server");
            continue;
        }

        strcpy(partialSolution , dividedLine);

        printf("Finalizou a função responsável por receber o jogo do servidor\n");
        operationSuccesss = 1;
    }
}

