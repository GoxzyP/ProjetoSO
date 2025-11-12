#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/uthash.h"
#include "../util.h"
#include "socketsUtilsServidor.h"

#define maxLineSizeInCsv 166  // Define o tamanho máximo de cada linha do ficheiro jogos.csv
#define NUM_GAMES_TO_SEND 5//Tirar mais tarde
gameData *gamesHashTable = NULL;

// --------------------------------------------------------------
// Lê todos os jogos Sudoku disponíveis
// --------------------------------------------------------------

void readGamesFromCSV()  // alterei readGamesFromCSv para readGamesFromCSV para retirar função do main.c
{
    FILE *file = fopen("Servidor/jogos.csv" , "r");  // Abre CSV para leitura

    if(file == NULL)
    {
        printf("Error opening file jogos.csv");  //Se não existir dá-nos uma mensagem de erro
        return;
    }

    char line[maxLineSizeInCsv];  // Buffer para ler cada linha do CSV

    while(fgets(line,maxLineSizeInCsv,file))  // Lê linha a linha
    {   
        char *dividedLine = strtok(line,",");  // Separa usando vírgula
        if (dividedLine == NULL) continue;     // Se for uma linha vazia ou mal formatada a função strtok devolve NULL
                                               // e pula para a próxima linha
        gameData *currentGame = malloc(sizeof(gameData));            // Cria variável para o jogo atual ( jogo a ser lido no ficheiro)
        currentGame->id = atoi(dividedLine);  // Converte ID de string para int

        dividedLine = strtok(NULL,",");         // Próxima coluna: tabuleiro parcial
        if (dividedLine == NULL) {free(currentGame);continue;}

        strcpy(currentGame->partialSolution,dividedLine);  // Preenche o Sudoku parcial

        dividedLine = strtok(NULL,",");  
        if (dividedLine == NULL) {free(currentGame);continue;}        // Próxima coluna: solução completa

        strcpy(currentGame->totalSolution,dividedLine);

        HASH_ADD_INT(gamesHashTable , id , currentGame);
    }

    fclose(file);  // Fecha o arquivo CSV
}

// --------------------------------------------------------------
// Verfica as respostas do cliente em relacao ao sudoku
// --------------------------------------------------------------

void verifyClientSudokuAnswer(int socket , int gameId , int rowSelected , int columnSelected , int clientAnswer) 
{   
    printf("Entrou na função de verificar a resposta do cliente\n");

    gameData *gameSentToClient;
    HASH_FIND_INT(gamesHashTable , &gameId , gameSentToClient);

    if(!gameSentToClient)
    {
        perror("Error: Server could not find the game for the given gameId");
        exit(1);
    }

    if(rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8)
    {
        perror("Error: Coordinates sent to the server are invalid");
        exit(1);
    }

    printf("O jogo do cliente e a sua resposta são válidos\n");

    char serverResponse[50];

    printf("O id do jogo do cliente é %d\n" , gameId);
    printf("A resposta do cliente foi %d\n" , clientAnswer + '0');
    printf("The solution is number %d\n", gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected]);

    if(gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0')
    {
        strcpy(serverResponse , "Correct\n");
        printf("A resposta do cliente estava correta\n");
    }
    else
    {   
        strcpy(serverResponse , "Wrong\n");
        printf("A resposta do cliente estava errada\n");
    }

    int responseLength = strlen(serverResponse);

    if(escreveSocket(socket , serverResponse , responseLength) != responseLength)
    {   
        perror("Error: Server could not send Sudoku response to client");
        exit(1);
    }

    printf("Foi enviado para o cliente o resultado da sua resposta com sucesso\n");
}

// --------------------------------------------------------------
// Envia um jogo aleatório para o cliente
// --------------------------------------------------------------

void sendGameToClient(int socket) 
{   
    printf("Entrou na função sendGameToClient\n");
    int count = HASH_COUNT(gamesHashTable);

    if(count == 0)
    {
        perror("Error: No games found in the database to send to the client");
        exit(1);
    }

    int index = rand() % count;
    int i = 0;

    gameData * game;

    for(game = gamesHashTable ; game != NULL ; game = game->hh.next)
    {   
        if(i == index)
        {
            char serverResponse[512];
            
            int positionInServerResponse = sprintf(serverResponse , "%d," , game->id);

            memcpy(serverResponse + positionInServerResponse, game->partialSolution , 81);
            positionInServerResponse += 81;

            serverResponse[positionInServerResponse++] = '\n';

            for(int i = 0 ; i < positionInServerResponse ; i++)
                printf("%d\n" , serverResponse[i]);
            
            if((escreveSocket(socket , serverResponse , positionInServerResponse)) != positionInServerResponse)
            {
                perror("Error: Could not send partial solution to client");
                exit(1);
            }
            
            printf("Enviou o jogo para o cliente\n");
        }
        
        i++;
    }
}
