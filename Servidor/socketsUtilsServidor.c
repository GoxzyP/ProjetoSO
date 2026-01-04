#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../include/uthash.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"
#include <time.h>

#define maxLineSizeInCsv 166

gameData *gamesHashTable = NULL;


Sala sala = {
    .numClientes = 0,
    .barrierInicializada = 0
};

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

// -------------------------------------------------------------------
// Verifica a jogada de um cliente e atualiza o tabuleiro compartilhado
// -------------------------------------------------------------------
// -----------------------------
// Verifica jogada do cliente
// -----------------------------
void verifyClientSudokuAnswer(int socketCliente,
                              int gameId,
                              int row,
                              int col,
                              int answer,
                              int clientId)
{
    pthread_mutex_lock(&sala.mutex);

    // jogo errado ou célula já preenchida
    if (gameId != sala.gameId || sala.sudoku[row*9 + col] != '0')
    {
        escreveSocket(socketCliente, "Invalid\n", 8);
        pthread_mutex_unlock(&sala.mutex);
        return;
    }

    int correto = (sala.solution[row*9 + col] - '0') == answer;

    if (correto)
    {
        sala.sudoku[row*9 + col] = answer + '0';
        sala.emptyCells--;

        broadcastUpdate(row, col, answer, clientId);
        escreveSocket(socketCliente, "Correct\n", 8);

        //  FIM DO JOGO
        if (sala.emptyCells == 0)
        {
            for (int i = 0; i < sala.numClientes; i++)
                escreveSocket(sala.clientes[i].socket,
                              "GAME_OVER\n", 10);
        }
    }
    else
    {
        escreveSocket(socketCliente, "Wrong\n", 6);
    }

    pthread_mutex_unlock(&sala.mutex);
}


// -------------------------------------------------------------------
// Função que verifica se a jogada do cliente está correta
// -------------------------------------------------------------------
int checkSudokuAnswer(int row, int col, int answer)
{
    extern gameData *gamesHashTable; // hash table com os jogos
    gameData *game;

    // procura o jogo atual
    for (game = gamesHashTable; game != NULL; game = game->hh.next)
    {
        if (game->id == sala.gameId)
        {
            return (game->totalSolution[row * 9 + col] - '0') == answer;
        }
    }
    return 0; // jogo não encontrado => errado
}
// -----------------------------
// Envia jogo para o cliente
// -----------------------------
void sendGameToClient(int socketCliente, int clientId, int numeroJogadoresSala)
{
    pthread_mutex_lock(&sala.mutex);

    // Primeiro cliente escolhe o jogo
    if (sala.numClientes == 0)
    {
        int count = HASH_COUNT(gamesHashTable);
        if (count == 0)
        {
            writeLogf("../Servidor/log_servidor.csv", "Nenhum jogo disponível no servidor");
            pthread_mutex_unlock(&sala.mutex);
            return;
        }

        // Escolhe aleatoriamente um jogo
        int index = rand() % count;
        int i = 0;
        gameData *game;
        for (game = gamesHashTable; game != NULL; game = game->hh.next)
        {
            if (i == index)
            {
                sala.gameId = game->id;

                // Copia tabuleiro parcial e solução completa
                memcpy(sala.sudoku, game->partialSolution, 81);
                memcpy(sala.solution, game->totalSolution, 81);

                sala.sudoku[81] = '\0';
                sala.solution[81] = '\0';

                // Conta células vazias
                sala.emptyCells = 0;
                for (int k = 0; k < 81; k++)
                    if (sala.sudoku[k] == '0')
                        sala.emptyCells++;

                writeLogf("../Servidor/log_servidor.csv",
                          "Jogo escolhido: %d, células vazias: %d",
                          sala.gameId, sala.emptyCells);

                break;
            }
            i++;
        }

        // Inicializa barreira para todos os jogadores
        pthread_barrier_init(&sala.barrier, NULL, numeroJogadoresSala);
        sala.barrierInicializada = 1;
    }

    // Adiciona cliente à sala
    sala.clientes[sala.numClientes].socket = socketCliente;
    sala.clientes[sala.numClientes].clientId = clientId;
    sala.numClientes++;

    pthread_mutex_unlock(&sala.mutex);

    // Espera todos os jogadores entrarem na sala
    int res = pthread_barrier_wait(&sala.barrier);

    if (res != 0 && res != PTHREAD_BARRIER_SERIAL_THREAD)
    {
        writeLogf("../Servidor/log_servidor.csv",
                  "Aviso: Barreira liberada parcialmente (algum cliente saiu?)");
    }

    // Envia tabuleiro completo ao cliente no formato correto
    char resposta[512];
    int pos = sprintf(resposta, "%d,", sala.gameId);  // gameId + vírgula
    memcpy(resposta + pos, sala.sudoku, 81);          // 81 caracteres do Sudoku
    pos += 81;
    resposta[pos++] = '\n';

    int bytes = escreveSocket(socketCliente, resposta, pos);
    if (bytes != pos)
    {
        writeLogf("../Servidor/log_servidor.csv",
                  "Cliente %d desconectou antes de receber o tabuleiro", clientId);
    }
    else
    {
        writeLogf("../Servidor/log_servidor.csv",
                  "Tabuleiro enviado para o cliente %d com sucesso", clientId);
    }
}




// -----------------------------
// Broadcast para todos os clientes
// -----------------------------
void broadcastUpdate(int row, int col, int value, int authorClientId)
{
    char msg[128];
    snprintf(msg, sizeof(msg), "UPDATE,%d,%d,%d,%d\n",
             row, col, value, authorClientId);


    for (int i = 0; i < sala.numClientes; i++)
        escreveSocket(sala.clientes[i].socket, msg, strlen(msg));
}


// Estrutura global da fila de pedidos de validação
// Inicializada com head e tail nulos (fila vazia)
static FilaPedidos fila = { NULL, NULL };

// Contador global de pedidos
// Usado para atribuir IDs únicos a cada pedido
static int pedidoCounter = 0;

// Mutex para proteger o acesso concorrente à fila
pthread_mutex_t filaMutex = PTHREAD_MUTEX_INITIALIZER;

// Variável de condição para bloquear/desbloquear threads consumidoras
// quando a fila estiver vazia ou houver novos pedidos
pthread_cond_t filaCond = PTHREAD_COND_INITIALIZER;


// ---------------------------------------------------------
// Função auxiliar que gera uma string representando os IDs
// dos clientes atualmente na fila e escreve no log
// ---------------------------------------------------------
void logFilaAtual()
{
    char filaIds[512]; // buffer para armazenar string da fila
    int pos = 0;       // posição atual no buffer

    // Começa a string com "[ "
    pos += snprintf(filaIds + pos, sizeof(filaIds) - pos, "[ ");

    // Percorre a fila ligada a partir do head
    PedidoValidacao *p = fila.head;
    while (p)
    {
        // Adiciona o ID do cliente ao buffer
        pos += snprintf(filaIds + pos,
                        sizeof(filaIds) - pos,
                        "%d",
                        p->clientId);

        // Adiciona vírgula entre IDs, exceto no último elemento
        if (p->next)
            pos += snprintf(filaIds + pos,
                            sizeof(filaIds) - pos,
                            ", ");

        p = p->next;
    }

    // Fecha a representação da fila com "]"
    pos += snprintf(filaIds + pos, sizeof(filaIds) - pos, " ]");

    // Escreve no log
    writeLogf("../Servidor/log_servidor.csv",
              "Fila atual de pedidos de validação parcial: %s",
              filaIds);
}

// ---------------------------------------------------------
// Função para enfileirar um pedido de validação na fila FIFO
// ---------------------------------------------------------
void enqueuePedido(PedidoValidacao *p)
{
    // Bloqueia o mutex para garantir exclusão mútua
    // e proteger a estrutura da fila durante alterações
    pthread_mutex_lock(&filaMutex);

    // Incrementa o contador global de pedidos e atribui ao pedido
    // Isso garante que cada pedido tenha um ID único
    pedidoCounter++;
    p->idPedido = pedidoCounter;

    // Registra no log que o pedido foi adicionado à fila
    writeLogf("../Servidor/log_servidor.csv",
              "Pedido de validação parcial %d enfileirado do cliente %d. (Coluna:%d,Linha:%d)",
              p->idPedido, p->clientId, p->col, p->row);

    // O próximo ponteiro do pedido é nulo, pois será o último da fila
    p->next = NULL;

    // Se a fila estiver vazia, este pedido será o head e tail
    if (fila.tail == NULL) {
        fila.head = fila.tail = p;
    } else {
        // Caso contrário, adiciona no final da fila e atualiza o tail
        fila.tail->next = p;
        fila.tail = p;
    }

    // Registra no log a fila atualizada
    logFilaAtual();

    // Sinaliza a condição para acordar uma thread que esteja esperando por pedidos
    pthread_cond_signal(&filaCond);

    // Libera o mutex, permitindo que outras threads acessem a fila
    pthread_mutex_unlock(&filaMutex);
}


// ---------------------------------------------------------
// Função para remover (desenfileirar) o próximo pedido da fila
// ---------------------------------------------------------
PedidoValidacao* dequeuePedido()
{
    // Bloqueia o mutex para acesso exclusivo à fila
    pthread_mutex_lock(&filaMutex);

    // Se a fila estiver vazia, espera até que algum pedido seja enfileirado
    while (fila.head == NULL) {
        pthread_cond_wait(&filaCond, &filaMutex);
    }

    // Remove o primeiro pedido da fila
    PedidoValidacao *p = fila.head;
    fila.head = p->next;

    // Se a fila ficar vazia após remover o pedido, tail também deve ser nulo
    if (fila.head == NULL)
        fila.tail = NULL;

    // Libera o mutex
    pthread_mutex_unlock(&filaMutex);

    // Retorna o pedido removido
    return p;
}


// ---------------------------------------------------------
// Thread que processa continuamente os pedidos da fila
// ---------------------------------------------------------
void* workerValidacoes(void *arg)
{
    while (1)
    {
        // Remove o próximo pedido da fila (bloqueante se fila vazia)
        PedidoValidacao *p = dequeuePedido();

        // Loga o processamento do pedido
        pthread_mutex_lock(&filaMutex);
        writeLogf("../Servidor/log_servidor.csv",
                  "Pedido de validação parcial %d processado do cliente %d",
                  p->idPedido, p->clientId);

        // Mostra a fila atualizada depois de remover o pedido
        logFilaAtual();
        pthread_mutex_unlock(&filaMutex);

        // Chama a função que verifica a resposta do cliente no Sudoku
        verifyClientSudokuAnswer(
            p->socketCliente,
            p->gameId,
            p->row,
            p->col,
            p->answer,
            p->clientId
        );

        // Libera a memória do pedido
        free(p);
    }

    return NULL; // Não será alcançado, loop infinito
}

