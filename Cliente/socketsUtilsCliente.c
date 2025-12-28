#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util.h"
#include "socketsUtilsCliente.h"
#include "../log.h"

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------

// Adicionei score na funcao fillSudoku

int* getPossibleAnswers(char partialSolution[81], int rowSelected, int columnSelected , int *size)
{   
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

// --------------------------------
// Display do tabuleiro no terminal
// --------------------------------
void displaySudokuWithCoords(char sudoku[81]) 
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
// Preenche o sudoku
// ------------------------------------------------------------------
void fillSudoku(int socket , char sudoku[81] , int gameId , char logPath[256]) 
{   
    char horaInicioJogo[20]; // buffer para armazenar uma cadeia de carareteres com o tempo em horas, minutos e segundos
    char horaFimJogo[20];
    char tempoJogo[20];

    int positionsToFill[81][2];
    int emptyCount = 0;
    int tentativas_falhadas = 0;
    int score = 0;

    registerGameTimeLive(horaInicioJogo,sizeof(horaInicioJogo));

    writeLogf(logPath, "Hora Inicio Jogo -> %s", horaInicioJogo);

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

    shufflePositionsInArray(positionsToFill , emptyCount);

    writeLogf(logPath, "Fez o shuffle do array de posições por preencher");
    writeLogf(logPath, "Número de posições inicialmente por preencher -> %d", emptyCount);

    for(int i = 0 ; i < emptyCount ; i++)
    {   
        writeLogf(logPath, "Numero de posições por preencher restantes -> %d", emptyCount - i - 1);

        int rowSelected = positionsToFill[i][0];
        int columnSelected = positionsToFill[i][1];

        int size = 0; 
        int* possibleAnswers = getPossibleAnswers(sudoku, rowSelected , columnSelected , &size);
        char answers[256] = "";


        for (int j = 0; j < size; j++) 
            sprintf(answers + strlen(answers), "%d ", possibleAnswers[j]);
        
        writeLogf(logPath, "Possiveis valores para (%d,%d): %s", rowSelected, columnSelected , answers);

        for(int j = 0 ; j < size ; j++)
        {   
            char messageToServer[512];
            int codeVerifyAnswer = 2;
            sprintf(messageToServer , "%d,%d,%d,%d,%d\n", codeVerifyAnswer, gameId, rowSelected, columnSelected, possibleAnswers[j]);

            writeLogf(logPath, "Foi enviado para o server o numero %d na posicao : %d %d", possibleAnswers[j], rowSelected, columnSelected);

            if((escreveSocket(socket , messageToServer , strlen(messageToServer))) != strlen(messageToServer))
            {
                perror("Erro : Não foi possível enviar a solução para o servidor");
                writeLogf(logPath, "Não foi possível enviar a solução para o servidor");
                exit(1);
            }

            writeLogf(logPath, "O envio para o server do numero %d na posicao : %d %d foi feito com sucesso", possibleAnswers[j], rowSelected, columnSelected);

            char serverAnswer[512];
            int bytesReceived = lerLinha(socket , serverAnswer , sizeof(serverAnswer));

            if(bytesReceived  < 0)
            {
                perror("Erro : Não foi possível receber a resposta do servidor sobre uma possível solução");
                writeLogf(logPath, "Não foi possível receber a resposta do servidor sobre uma possível solução");
                exit(1);
            }

           serverAnswer[bytesReceived - 1] = '\0';

            if(strcmp(serverAnswer,"Correct") == 0)
            {   
                writeLogf(logPath, "A resposta enviada para o servidor estava correta");
                score++;                                                                
                sudoku[rowSelected * 9 + columnSelected] = possibleAnswers[j] + '0';
                displaySudokuWithCoords(sudoku);
                break;
            }
            else 
            {
                writeLogf(logPath, "A resposta enviada para o servidor estava errada");
                tentativas_falhadas++;
                score--;
            }
        }

        free(possibleAnswers);
    }

    writeLogf(logPath, "Tentativas falhadas -> %d", tentativas_falhadas);

    writeLogf(logPath,"Score final -> %d", score);

    registerGameTimeLive(horaFimJogo,sizeof(horaFimJogo));
    writeLogf(logPath, "Hora Fim Jogo -> %s", horaFimJogo);

    registerGameTime(horaInicioJogo,horaFimJogo,tempoJogo,sizeof(tempoJogo));
    writeLogf(logPath, "Tempo Jogo -> %s", tempoJogo);
}

void requestGame(int socket , int* gameId , char partialSolution[81] , char logPath[256])
{   
    char messageToServer[512];
    int codeRequestGame = 1;

    sprintf(messageToServer , "%d,\n" , codeRequestGame);

    if((escreveSocket(socket , messageToServer , strlen(messageToServer))) != strlen(messageToServer))
    {
        perror("Erro : Não foi possível enviar ao servidor um pedido de jogo");
        writeLogf(logPath, "Não foi possível enviar ao servidor um pedido de jogo");
        exit(1);
    }

    writeLogf(logPath, "Escreveu com sucesso o pedido do jogo");

    char serverAnswer[512];
    int bytesReceived = lerLinha(socket , serverAnswer , sizeof(serverAnswer));

    if(bytesReceived  < 0 )
    {
        perror("Erro : Não foi possível obter resposta do servidor sobre o pedido de jogo");
        writeLogf(logPath, "Não foi possível obter resposta do servidor sobre o pedido de jogo");
        exit(1);
    }

    writeLogf(logPath, "Foi recebida a resposta do servidor sobre o pedido de jogo");

    char *dividedLine = strtok(serverAnswer , ",");

    if(dividedLine == NULL)
    {
        perror("Erro : Formato incorreto na resposta do servidor com relação ao pedido do jogo -> id");
        writeLogf(logPath, "Formato incorreto na resposta do servidor com relação ao pedido do jogo -> id");
        exit(1);
    }

    *gameId = atoi(dividedLine);

    writeLogf(logPath, "Id de jogo recebido -> %d", *gameId);
    
    dividedLine = strtok(NULL , ",");

    if(dividedLine == NULL)
    {
        perror("Erro : Formato incorreto na resposta do servidor com relação ao pedido do jogo -> solução parcial");
        writeLogf(logPath, "Formato incorreto na resposta do servidor com relação ao pedido do jogo -> solução parcial");
        exit(1);
    }

    dividedLine[strcspn(dividedLine, "\n")] = '\0';

    if(strlen(dividedLine) != 81)
    {
        perror("Erro : Formato incorreto na resposta do servidor com relação ao pedido do jogo -> tamanho da solução parcial");
        writeLogf(logPath, "Formato incorreto na resposta do servidor com relação ao pedido do jogo -> tamanho da solução parcial");
        exit(1);
    }

    strcpy(partialSolution , dividedLine);

    writeLogf(logPath, "Recebeu o jogo perfeitamente");
}


