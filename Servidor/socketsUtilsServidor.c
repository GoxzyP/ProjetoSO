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

