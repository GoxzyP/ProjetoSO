#include "../util.h"
#include "../log.h"
#include "socketsUtilsCliente.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --------------------------------
// Display do tabuleiro no terminal
// --------------------------------
void displaySudokuWithCoords(void *sudoku , int (*getValueFunction)(void* , int , int)) 
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
            int value = getValueFunction(sudoku , row , col);
            if (value == 0)
                printf("-");  // se for 0 então não há valor ainda, substituimos por "-"
            else
                printf("%d", value); // imprime o número presente na célula

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
void shufflePositionsInArray(int positions[][2], int count)
{
    for (int i = count - 1; i > 0; i--)
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

int getValueFromSudokuCell(void *sudoku , int rowSelected , int columnSelected)
{
    sudokuCell (*grid)[9][9] = sudoku;
    return (*grid)[rowSelected][columnSelected].value;
}

int getValueFromCharGrid(void *sudoku , int rowSelected , int columnSelected)
{
    char *grid = sudoku;
    int index = rowSelected * 9 + columnSelected;

    return grid[index] - '0';
}

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(void *sudoku , int rowSelected, int columnSelected , int *size , int (*getValueFunction)(void* , int , int))
{   
    //printf("A thread produtora %d entrou com sucesso na função responsável por pegar as respostas possíveis para a célula %d %d \n" , pthread_self() , rowSelected , columnSelected);

    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        int rowValue = getValueFunction(sudoku , rowSelected , i);
        int columnValue = getValueFunction(sudoku , i , columnSelected);

        // Se a linha já contém o número, remove da lista de possíveis
        if(rowValue != 0 && allPossibleAnswers[rowValue - 1] != 0)
        {
            allPossibleAnswers[rowValue - 1] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if(columnValue != 0 && allPossibleAnswers[columnValue - 1] != 0)
        {
            allPossibleAnswers[columnValue - 1] = 0; // lógica igual às linhas
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
            int value = getValueFunction(sudoku , startRow + 1 , startColumn + 1);

            if(value != 0 && allPossibleAnswers[value - 1] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[value - 1] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
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

void requestGame(int socket , int *gameId , char partialSolution[82], char logPath[256])
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
        
        writeLogf(logPath, "Id do jogo recebido -> %d", *gameId);

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
        writeLogf(logPath, "Recebeu o jogo perfeitamente");
        operationSuccesss = 1;
    }
}

// ------------------------------------------------------------------
// Preenche o sudoku
// ------------------------------------------------------------------
void fillSudoku(int socket, char sudoku[81], int gameId, char logPath[256], ClientStats *stats, int clientId)
{
    char horaInicioJogo[20];
    char horaFimJogo[20];
    char tempoJogo[20];

    int positionsToFill[81][2];
    int emptyCount = 0;
    int tentativas_falhadas = 0;

    registerGameTimeLive(horaInicioJogo, sizeof(horaInicioJogo));
    writeLogf(logPath, "Hora Inicio Jogo -> %s", horaInicioJogo);

    // Incrementa estatísticas
    stats->totalJogos++;

    for (int row = 0; row < 9; row++)
    {
        for (int column = 0; column < 9; column++)
        {
            if (sudoku[row * 9 + column] == '0')
            {
                positionsToFill[emptyCount][0] = row;
                positionsToFill[emptyCount][1] = column;
                emptyCount++;
            }
        }
    }

    shufflePositionsInArray(positionsToFill, emptyCount);

    writeLogf(logPath, "Fez o shuffle do array de posições por preencher");
    writeLogf(logPath, "Número de posições inicialmente por preencher -> %d", emptyCount);

    // Adiciona células vazias ao total
    stats->totalCelulasPreenchidas += emptyCount;

    for (int i = 0; i < emptyCount; i++)
    {
        writeLogf(logPath, "Numero de posições por preencher restantes -> %d", emptyCount - i - 1);

        int rowSelected = positionsToFill[i][0];
        int columnSelected = positionsToFill[i][1];

        int size = 0;
        int *possibleAnswers = getPossibleAnswers(sudoku, rowSelected, columnSelected, &size, getValueFromCharGrid);
        char answers[256] = "";

        for (int j = 0; j < size; j++)
        {
            char temp[10];
            sprintf(temp, "%d ", possibleAnswers[j]);
            strcat(answers, temp);
        }

        writeLogf(logPath, "As respostas possíveis para a linha %d e coluna %d são -> %s", rowSelected, columnSelected, answers);

        if (size > 0)
        {
            int selectedAnswerIndex = rand() % size;
            int clientAnswer = possibleAnswers[selectedAnswerIndex];

            writeLogf(logPath, "A resposta escolhida foi o número %d", clientAnswer);

            char messageToServer[BUFFER_SOCKET_MESSAGE_SIZE];
            sprintf(messageToServer, "2,1,%d,%d,%d,%d,%d\n", gameId, rowSelected, columnSelected, clientAnswer, clientId);

            if (writeSocket(socket, messageToServer, strlen(messageToServer)) != strlen(messageToServer))
            {
                perror("Erro : O cliente não conseguiu enviar a tentativa para o servidor");
                writeLogf(logPath, "O cliente não conseguiu enviar a tentativa para o servidor");
                free(possibleAnswers);
                return;
            }

            writeLogf(logPath, "Enviou a tentativa para o servidor");

            char serverResponse[BUFFER_SOCKET_MESSAGE_SIZE];
            int bytesReceived = readSocket(socket, serverResponse, sizeof(serverResponse));

            if (bytesReceived <= 0)
            {
                perror("Erro : O cliente não recebeu resposta do servidor");
                writeLogf(logPath, "O cliente não recebeu resposta do servidor");
                free(possibleAnswers);
                return;
            }

            serverResponse[bytesReceived] = '\0';
            serverResponse[strcspn(serverResponse, "\n")] = '\0';

            writeLogf(logPath, "Recebeu resposta do servidor -> %s", serverResponse);

            if (strcmp(serverResponse, "Correct") == 0)
            {
                sudoku[rowSelected * 9 + columnSelected] = clientAnswer + '0';
                displaySudokuWithCoords(sudoku, getValueFromCharGrid);
                writeLogf(logPath, "A resposta enviada para o servidor estava correta");
                stats->totalAcertos++;
                stats->totalJogadas++;
            }
            else
            {
                writeLogf(logPath, "A resposta enviada para o servidor estava errada");
                stats->totalErros++;
                stats->totalJogadas++;
                tentativas_falhadas++;
            }
        }

        free(possibleAnswers);
    }

    writeLogf(logPath, "Tentativas falhadas -> %d", tentativas_falhadas);

    registerGameTimeLive(horaFimJogo, sizeof(horaFimJogo));
    writeLogf(logPath, "Hora Fim Jogo -> %s", horaFimJogo);

    registerGameTime(horaInicioJogo, horaFimJogo, tempoJogo, sizeof(tempoJogo));
    writeLogf(logPath, "Tempo Jogo -> %s", tempoJogo);
}

// ------------------------------------------------------------------
// Escreve estatísticas do cliente no log E mostra no terminal
// ------------------------------------------------------------------
void writeClientStats(ClientStats *stats, char logPath[256], int clientId)
{
    // Calcular métricas derivadas
    float taxaAcerto = 0.0;
    if (stats->totalJogadas > 0)
    {
        taxaAcerto = ((float)stats->totalAcertos / (float)stats->totalJogadas) * 100.0;
    }

    float mediaTentativasPorCelula = 0.0;
    if (stats->totalCelulasPreenchidas > 0)
    {
        mediaTentativasPorCelula = (float)stats->totalJogadas / (float)stats->totalCelulasPreenchidas;
    }

    // =========================================================================
    // MOSTRAR ESTATÍSTICAS NO TERMINAL (CMD)
    // =========================================================================
    printf("\n");
    printf("========================================\n");
    printf("ESTATISTICAS DO CLIENTE %d\n", clientId);
    printf("========================================\n");
    printf("1. Taxa de Acerto Geral: %.2f%%\n", taxaAcerto);
    printf("2. Total de Acertos: %d\n", stats->totalAcertos);
    printf("3. Total de Jogadas: %d\n", stats->totalJogadas);
    printf("4. Total de Jogos: %d\n", stats->totalJogos);
    printf("5. Total de Erros: %d\n", stats->totalErros);
    printf("6. Media de Tentativas por Celula: %.2f\n", mediaTentativasPorCelula);
    printf("========================================\n\n");
    fflush(stdout);  // Força a exibição imediata no terminal

    // Criar caminho para ficheiro de estatísticas separado
    char statsPath[512];
    snprintf(statsPath, sizeof(statsPath), "../Cliente/estatisticas_cliente%d.csv", clientId);

    // SOBRESCREVER ficheiro de estatísticas
    FILE *file = fopen(statsPath, "w"); // Modo "w" sobrescreve
    if (!file)
    {
        perror("Erro ao abrir ficheiro de estatísticas do cliente");
        return;
    }

    fprintf(file, "========================================\n");
    fprintf(file, "ESTATISTICAS DO CLIENTE %d\n", clientId);
    fprintf(file, "========================================\n");
    fprintf(file, "1. Taxa de Acerto Geral: %.2f%%\n", taxaAcerto);
    fprintf(file, "2. Total de Acertos: %d\n", stats->totalAcertos);
    fprintf(file, "3. Total de Jogadas: %d\n", stats->totalJogadas);
    fprintf(file, "4. Total de Jogos: %d\n", stats->totalJogos);
    fprintf(file, "5. Total de Erros: %d\n", stats->totalErros);
    fprintf(file, "6. Media de Tentativas por Celula: %.2f\n", mediaTentativasPorCelula);
    fprintf(file, "========================================\n");

    fclose(file);

    writeLogf(logPath, "Estatísticas do cliente escritas com sucesso");
}
