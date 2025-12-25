#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../include/uthash.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"

#define maxLineSizeInCsv 166

gameData *gamesHashTable = NULL;

// --------------------------------------------------------------
// Lê todos os jogos Sudoku disponíveis do CSV
// --------------------------------------------------------------
void readGamesFromCSV() {
    FILE *file = fopen("../Servidor/jogos.csv", "r");
    if (!file) {
        writeLogf("../Servidor/log_servidor.csv", "Não foi possível abrir CSV");
        return;
    }

    char line[maxLineSizeInCsv];
    while (fgets(line, maxLineSizeInCsv, file)) {
        char *dividedLine = strtok(line, ",");
        if (!dividedLine) continue;

        gameData *currentGame = malloc(sizeof(gameData));
        currentGame->id = atoi(dividedLine);

        dividedLine = strtok(NULL, ",");
        if (!dividedLine) { free(currentGame); continue; }
        strcpy(currentGame->partialSolution, dividedLine);

        dividedLine = strtok(NULL, ",");
        if (!dividedLine) { free(currentGame); continue; }
        strcpy(currentGame->totalSolution, dividedLine);

        HASH_ADD_INT(gamesHashTable, id, currentGame);
    }
    fclose(file);
}

// --------------------------------------------------------------
// Verifica a resposta do cliente
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



// Estrutura global que representa uma sala multijogador
// Inicializada a zero (todos os campos começam com valor 0)
SalaMultijogador sala = {0};

// Mutex global para proteger o acesso concorrente à sala
// Garante exclusão mútua entre threads
pthread_mutex_t salaMutex = PTHREAD_MUTEX_INITIALIZER;

//----------------------------------------------------------------
// Função responsável por enviar um jogo Sudoku a um cliente
// Esta função garante que todos os jogadores da sala
// recebem exatamente o mesmo tabuleiro e começam ao mesmo tempo
//----------------------------------------------------------------
void sendGameToClient(int socket, int clientId, int numeroJogadoresSala)
{
    // Bloqueia o mutex para acesso exclusivo à sala
    pthread_mutex_lock(&salaMutex);

    // Se não existir nenhum jogo ativo na sala
    // (ou seja, é o primeiro cliente a entrar)
    if (sala.numClientes == 0)
    {
        // Conta o número total de jogos disponíveis
        int count = HASH_COUNT(gamesHashTable);

        // Se não houver jogos disponíveis, desbloqueia e termina
        if (count == 0)
        {
            pthread_mutex_unlock(&salaMutex);
            return;
        }

        // Escolhe um índice aleatório para selecionar um jogo
        int index = rand() % count;
        int i = 0;
        gameData *game;

        // Percorre a hash table de jogos
        for (game = gamesHashTable; game != NULL; game = game->hh.next)
        {
            // Quando o índice aleatório é atingido
            if (i == index)
            {
                // Guarda o ID do jogo selecionado na sala
                sala.gameId = game->id;

                // Copia a solução parcial do jogo para a sala
                memcpy(sala.partialSolution, game->partialSolution, 81);

                // Garante que a string termina corretamente
                sala.partialSolution[81] = '\0';
                break;
            }
            i++;
        }

        // Inicializa a barreira para sincronizar os jogadores
        // Apenas quando o número esperado de jogadores chegar,
        // todos podem prosseguir
        pthread_barrier_init(&sala.barrier, NULL, numeroJogadoresSala);

        // Marca a barreira como inicializada
        sala.barrierInicializada = 1;
    }

    // Incrementa o número de clientes atualmente na sala
    sala.numClientes++;

    // Regista no log a entrada do cliente na sala
    writeLogf("../Servidor/log_servidor.csv",
              "Cliente %d entrou na sala (%d/%d)",
              clientId, sala.numClientes, numeroJogadoresSala);

    // Liberta o mutex após atualizar o estado da sala
    pthread_mutex_unlock(&salaMutex);

    // Aguarda na barreira até que todos os jogadores
    // da sala estejam prontos
    pthread_barrier_wait(&sala.barrier);

    // Preparação da resposta a enviar ao cliente
    char resposta[512];

    // Escreve o ID do jogo seguido de vírgula
    int pos = sprintf(resposta, "%d,", sala.gameId);

    // Copia a grelha parcial do Sudoku para a mensagem
    memcpy(resposta + pos, sala.partialSolution, 81);
    pos += 81;

    // Adiciona uma quebra de linha no final
    resposta[pos++] = '\n';

    // Envia a resposta completa ao cliente através do socket
    escreveSocket(socket, resposta, pos);

    // Após o envio do jogo, volta a bloquear o mutex
    pthread_mutex_lock(&salaMutex);

    // Diminui o número de clientes na sala
    // (o cliente já recebeu o jogo)
    sala.numClientes--;

    // Se a sala ficar vazia após o envio
    // faz o reset do estado da sala
    if (sala.numClientes == 0)
    {
        // Limpa o ID do jogo
        sala.gameId = 0;

        // Marca a barreira como não inicializada
        sala.barrierInicializada = 0;
    }

    // Liberta o mutex
    pthread_mutex_unlock(&salaMutex);
}
