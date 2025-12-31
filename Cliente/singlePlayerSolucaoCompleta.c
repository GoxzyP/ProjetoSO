//Util.h é onde está definido as funções de leitura e escrita dos sockets
#include "../util.h"

//SocketsUtilsCliente.h é onde está definido as funções que tanto o multiplayer quanto o singleplayer iram partilhar
#include "socketsUtilsCliente.h"

//String,h é onde está definido as funções de manipulação de string
#include <string.h>

int solveSudokuUsingRecursion(char sudoku[82] , char wrongValuesAlreadyTried[9][9][10] , int startingRow , int startingColumn)
{
    if(startingRow == 9)
        return 1;

    int nextRow = (startingColumn + 1) == 9 ? startingRow + 1 : startingRow;
    int nextColumn = (startingColumn + 1) == 9 ? 0 : startingColumn + 1;

    if(sudoku[startingRow * 9 + startingColumn] != '0')
        return solveSudokuUsingRecursion(sudoku , wrongValuesAlreadyTried , nextRow , nextColumn);

    int size = 0;
    int* possibleAnswers = getPossibleAnswers(sudoku , startingRow , startingColumn , &size , getValueFromCharGrid); 

    for(int i = 0 ; i < size ; i++)
    {   
        if(wrongValuesAlreadyTried[startingRow][startingColumn][possibleAnswers[i]] == 0)
        {
            sudoku[startingRow * 9 + startingColumn] = possibleAnswers[i] + '0';

            if(solveSudokuUsingRecursion(sudoku , wrongValuesAlreadyTried , nextRow , nextColumn))
            {
                free(possibleAnswers);
                return 1;
            }
        }
    }

    sudoku[startingRow * 9 + startingColumn] = '0';
    free(possibleAnswers);

    return 0;
}

void solveSudokuUsingCompleteSolution(int socket , int gameId , char sudoku[82])
{   
    int codeToVerifyAnswer = 2;
    int codeToVerifyCompleteAnswer = 2;

    int startingRow = 0;
    int startingColumn = 0;

    char cellIsPartOfPartialSolution[81];
    char wrongValuesAlreadyTried[9][9][10] = {{{0}}};

    for(int i = 0 ; i < 81 ; i++)
        cellIsPartOfPartialSolution[i] = (sudoku[i] != '0') ? 1 : 0;

    while(1)
    {   
        if(solveSudokuUsingRecursion(sudoku , wrongValuesAlreadyTried , startingRow , startingColumn) == 0)
        {
            perror("Error : Client could not find a complete solution to the sudoku sent from the server");
            exit(1);
        }

        char messageToServer[BUFFER_SOCKET_MESSAGE_SIZE];
        sprintf(messageToServer , "%d,%d,%d,%s\n" , codeToVerifyAnswer , codeToVerifyCompleteAnswer , gameId , sudoku);

        if(writeSocket(socket , messageToServer , strlen(messageToServer)) != strlen(messageToServer))
        {
            perror("Error : Client could not write the complete answer to the server or the server disconnected from the socket");
            exit(1);
        }

        char messageFromServer[BUFFER_SOCKET_MESSAGE_SIZE];
        int bytesReceived = readSocket(socket , messageFromServer , sizeof(messageFromServer));

        if(bytesReceived <= 0)
        {
            perror("Error : Client could not read the answer regarding the partial solution from the server or the server disconnected from the socket");
            exit(1);
        }

        messageFromServer[bytesReceived - 1] = '\0';

        if(strncmp(messageFromServer , "Correct" , strlen("Correct")) == 0)
        {   
            printf("A solucao completa enviada para o servidor estava correta");
            displaySudokuWithCoords(sudoku , getValueFromCharGrid);
            return;
        }

        else
        {
            startingRow = messageFromServer[0] - '0';
            startingColumn = messageFromServer[2] - '0';

            printf("Foi encontrado um erro na celula %d %d da solucao completa" , startingRow , startingColumn);

            wrongValuesAlreadyTried[startingRow][startingColumn][sudoku[startingRow * 9 + startingColumn] - '0'] = 1;

            for(int row = startingRow ; row < 9 ; row++)
            {   
                int startCol = (row == startingRow) ? startingColumn : 0;
                for(int column = startCol ; column < 9 ; column++)
                {
                    if(cellIsPartOfPartialSolution[row * 9 + column] == 0)
                        sudoku[row * 9 + column] = '0';
                }
            }

            displaySudokuWithCoords(sudoku , getValueFromCharGrid);
        }
    }
}