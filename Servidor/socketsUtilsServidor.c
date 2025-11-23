#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/uthash.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"

#define maxLineSizeInCsv 166   // Define o tamanho máximo de cada linha do ficheiro jogos.csv
#define NUM_GAMES_TO_SEND 5    // Tirar mais tarde

gameData *gamesHashTable = NULL;

// --------------------------------------------------------------
// Lê todos os jogos Sudoku disponíveis
// --------------------------------------------------------------
void readGamesFromCSV()
{
    FILE *file = fopen("../Servidor/jogos.csv", "r");  // Abre CSV para leitura

    if (file == NULL)
    {
        printf("Erro : Não foi possível abrir o ficheiro onde estão armazenados os jogos\n");
        writeLogf("../Servidor/log_servidor.csv","Não foi possível abrir o ficheiro onde estão armazenados os jogos");
        return;
    }

    char line[maxLineSizeInCsv];  // Buffer para ler cada linha do CSV

    while (fgets(line, maxLineSizeInCsv, file))  // Lê linha a linha
    {
        char *dividedLine = strtok(line, ",");  // Separa usando vírgula
        if (dividedLine == NULL) continue;

        gameData *currentGame = malloc(sizeof(gameData));  // Cria a estrutura do jogo
        currentGame->id = atoi(dividedLine);               // Converte ID

        dividedLine = strtok(NULL, ",");  // Sudoku parcial
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->partialSolution, dividedLine);

        dividedLine = strtok(NULL, ",");  // Sudoku solução total
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->totalSolution, dividedLine);

        HASH_ADD_INT(gamesHashTable, id, currentGame);
    }

    fclose(file);
}

// --------------------------------------------------------------
// Verifica as respostas do cliente em relação ao sudoku
// --------------------------------------------------------------
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId)
{
    gameData *gameSentToClient;
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient);

    if (!gameSentToClient)
    {
        perror("Erro : O servidor não conseguiu encontar o jogo com o id enviado pelo cliente");
        writeLogf("../Servidor/log_servidor.csv","O servidor não conseguiu encontar o jogo com o id enviado pelo cliente %d" , clientId);
        exit(1);
    }

    if (rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8)
    {
        perror("Erro: Coordenadas enviadas pelo cliente eram inválidas");
        writeLogf("../Servidor/log_servidor.csv","Coordenadas enviadas pelo cliente %d eram inválidas" , clientId);
        exit(1);
    }

    writeLogf("../Servidor/log_servidor.csv","O id do jogo enviado pelo cliente %d e a sua resposta são válidos" , clientId);

    char serverResponse[50];

    writeLogf("../Servidor/log_servidor.csv","Id do jogo do cliente %d -> %d", clientId , gameId);

    writeLogf("../Servidor/log_servidor.csv","Resposta do cliente %d -> %d", clientId , clientAnswer);

    writeLogf("../Servidor/log_servidor.csv", "A solucao para a linha %d e coluna %d é o número -> %d",rowSelected,columnSelected,gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] - '0');

    if (gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0')
    {
        strcpy(serverResponse, "Correct\n");
        writeLogf( "../Servidor/log_servidor.csv","A resposta enviada pelo cliente %d está correta" , clientId);
    }
    else
    {
        strcpy(serverResponse, "Wrong\n");
        writeLogf("../Servidor/log_servidor.csv","A resposta enviada pelo cliente %d está errada" , clientId);
    }

    int responseLength = strlen(serverResponse);

    if (escreveSocket(socket, serverResponse, responseLength) != responseLength)
    {
        perror("Erro: O servidor não conseguiu enviar a resposta à solução do cliente");
        writeLogf("../Servidor/log_servidor.csv","O servidor não conseguiu enviar a resposta à solução do cliente %d" , clientId);
        exit(1);
    }

    writeLogf("../Servidor/log_servidor.csv","Foi enviado para o cliente %d o resultado da sua resposta com sucesso" , clientId);
}

// --------------------------------------------------------------
// Envia um jogo aleatório para o cliente
// --------------------------------------------------------------
void sendGameToClient(int socket , int clientId)
{
    int count = HASH_COUNT(gamesHashTable);

    if (count == 0)
    {
        perror("Erro : Nenhum jogo encontrado na base de dados");
        writeLogf("../Servidor/log_servidor.csv","Nenhum jogo encontrado na base de dados");
        exit(1);
    }

    int index = rand() % count;
    int i = 0;

    gameData *game;

    for (game = gamesHashTable; game != NULL; game = game->hh.next)
    {
        if (i == index)
        {
            char serverResponse[512];

            int positionInServerResponse = sprintf(serverResponse, "%d,", game->id);

            memcpy(serverResponse + positionInServerResponse, game->partialSolution, 81);
            positionInServerResponse += 81;

            serverResponse[positionInServerResponse++] = '\n';

            if (escreveSocket(socket, serverResponse, positionInServerResponse) != positionInServerResponse)
            {
                perror("Erro : O servidor não conseguiu enviar um jogo ao cliente");
                writeLogf("../Servidor/log_servidor.csv","O servidor não conseguiu enviar um jogo ao cliente");
                exit(1);
            }
            
            writeLogf("../Servidor/log_servidor.csv","Enviou para o cliente %d o jogo com o id -> %d" , clientId , game->id);
        }

        i++;
    }
}
