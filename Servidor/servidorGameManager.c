#include "servidorGameManager.h" // Inclui o header do gerenciador de jogos do servidor
#include "socketsUtilsServidorSinglePlayer.h" // Para acesso às stats
#include "../util.h"            // Inclui funções utilitárias, ex: writeSocket
#include "../log.h"             // Para sistema de logs

#include <stdio.h>   // Para printf, perror
#include <stdlib.h>  // Para malloc, exit, atoi, rand
#include <string.h>  // Para strcpy, strlen, strtok

extern int epollFd; // Declarar a variável global epollFd, usada para controlar sockets com epoll
#include <sys/epoll.h> // Para usar EPOLL_CTL_DEL
#include <unistd.h>    // Para a função close(), fechar sockets

// Variáveis externas de estatísticas
extern ClientStatsServer *clientsStatsHash;
extern ServerGlobalStats globalStats;
// Funções de lock leitor-escritor
extern void readerLock();
extern void readerUnlock();
extern void writerLock();
extern void writerUnlock();
extern void writeServerStats();

gameData *gamesHashTable = NULL;     // Hash table para armazenar os jogos carregados
pthread_mutex_t mutexGameData;       // Mutex para proteger acesso a gamesHashTable

#define MAX_LINE_SIZE_IN_CSV 166       // Tamanho máximo de cada linha no CSV que contém informações do jogo

// -----------------------------------------
// Lê os jogos do CSV e insere na hash table
// -----------------------------------------
void readGamesFromCSV()
{   
    // Tenta primeiro o caminho relativo do diretório raiz, depois de Build/
    FILE *file = fopen("Servidor/jogos.csv", "r");
    if (file == NULL)
        file = fopen("../Servidor/jogos.csv", "r");

    if (file == NULL)
    {
        printf("Erro : Não foi possível abrir o ficheiro onde estão armazenados os jogos\n");
        printf("Tentou: Servidor/jogos.csv e ../Servidor/jogos.csv\n");
        writeLogf("../Servidor/log_servidor.txt", "ERRO: Não foi possível abrir o ficheiro de jogos");
        return;
    }

    char line[MAX_LINE_SIZE_IN_CSV];
    int jogosCarregados = 0;

    while (fgets(line, MAX_LINE_SIZE_IN_CSV, file)) //Lê o ficheiro linha a linha
    {   
        char *dividedLine = strtok(line, ","); // Separa utilizando vírgula
        if (dividedLine == NULL) continue;

        //Cria uma estrutura gameData que vai armazenar a informação do jogo que está a ser lido e copia os dados para a estrutura
        gameData *currentGame = malloc(sizeof(gameData)); 
        currentGame->id = atoi(dividedLine);              

        dividedLine = strtok(NULL, ",");  
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->partialSolution, dividedLine);

        dividedLine = strtok(NULL, ","); 
        if (dividedLine == NULL) { free(currentGame); continue; }
        strcpy(currentGame->totalSolution, dividedLine);;

        HASH_ADD_INT(gamesHashTable, id, currentGame); //Adiciona o jogo à hashTable
        jogosCarregados++;
    }

    fclose(file);
    writeLogf("../Servidor/log_servidor.txt", "Carregados %d jogos do CSV com sucesso", jogosCarregados);
}

// --------------------------------------
// Envia um jogo aleatório para o cliente
// --------------------------------------
void sendGameToClient(int socket, int clientId)
{   
    printf("Thread produtora %lu entrou na função de enviar o jogo para o cliente %d\n" , pthread_self(), clientId); 
    // Mostra no console que a thread produtora começou a enviar o jogo

    int numberOfGamesInServer = HASH_COUNT(gamesHashTable); // Conta quantos jogos existem no servidor

    if(numberOfGamesInServer == 0) // Se não houver jogos
    {
        perror("Error : Server could not find any games in the data base"); // Mostra erro
        exit(1); // Sai do programa
    }

    int index = rand() % numberOfGamesInServer; // Escolhe um jogo aleatório
    int i = 0;

    gameData *game;

    for(game = gamesHashTable ; game != NULL ; game = game->hh.next) // Percorre a hash table
    {   
        if(i == index) // Encontra o jogo escolhido
        {   
            // Prepara a mensagem para enviar ao cliente no formato "id,solução parcial\n"
            char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

            snprintf(messageToClient , sizeof(messageToClient) , "%d,%s\n" , game->id , game->partialSolution);
            printf("Mensagem que será enviada para o cliente %d pela thread produtora %lu -> %s\n" , clientId, pthread_self() , messageToClient);

            if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
            {   
                // Caso falhar, remove o socket do epoll e fecha o socket
                perror("Error : Server could not sent game to client or client disconnected from the socket");
                handleClientDisconnection(socket);
                return;           
            }

            printf("Thread produtora %lu enviou o jogo com sucesso para o cliente %d\n" , pthread_self(), clientId);
            
            writeLogf("../Servidor/log_servidor.txt", "Jogo ID %d enviado ao cliente %d pela thread %lu", game->id, clientId, pthread_self());
            
            // Atualizar estatísticas
            writerLock();  // Lock de ESCRITA - acesso exclusivo
            
            globalStats.totalJogos++;
            
            // Encontrar ou criar cliente na hash table
            ClientStatsServer *cliente;
            HASH_FIND_INT(clientsStatsHash, &clientId, cliente);
            if (!cliente) {
                cliente = malloc(sizeof(ClientStatsServer));
                cliente->idCliente = clientId;
                cliente->acertos = 0;
                cliente->erros = 0;
                cliente->jogos = 0;
                HASH_ADD_INT(clientsStatsHash, idCliente, cliente);
                globalStats.totalClientes++;
            }
            cliente->jogos++;
            
            writerUnlock();  // Unlock de ESCRITA
            
            // Atualizar ficheiro de estatísticas
            writeServerStats();

            break; // Sai do loop, já enviou o jogo
        }

        i++;
    }

    printf("Thread produtora %lu finalizou a função responsável por enviar o jogo ao cliente %d\n" , pthread_self(), clientId);
}


void verifyClientPartialSolution(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer, int clientId)
{   
    gameData *gameSentToClient;
    pthread_mutex_lock(&mutexGameData); // Protege acesso à hash table
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient); // Procura o jogo na hashTable utilizando o seu id
    pthread_mutex_unlock(&mutexGameData);
    
    // Prepara a mensagem para enviar ao cliente com o formato "Correct\n ou Wrong\n"
    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    if (!gameSentToClient || (rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8))
    {
        perror("Error : Server could not find a game with the id sent from the client or the coordinates are wrong");
        handleClientDisconnection(socket);
        return;
    }
    
    // Verifica se a resposta está correta
    int isCorrect = (gameSentToClient->totalSolution[(rowSelected * 9) + columnSelected] == clientAnswer + '0');
    
    if (isCorrect) {
        strcpy(messageToClient, "Correct\n");
        printf("A thread consumidora %lu avaliou a tentativa do cliente %d e esta estava correta\n" , pthread_self(), clientId);
        writeLogf("../Servidor/log_servidor.txt", "Cliente %d: resposta CORRETA na posição [%d,%d] do jogo %d", clientId, rowSelected, columnSelected, gameId);
        
        // Incrementar estatísticas de acertos
        writerLock();  // Lock de ESCRITA - acesso exclusivo
        globalStats.totalAcertos++;
        globalStats.totalJogadas++;
        
        ClientStatsServer *cliente;
        HASH_FIND_INT(clientsStatsHash, &clientId, cliente);
        if (!cliente) {
            // Criar cliente se não existir
            cliente = malloc(sizeof(ClientStatsServer));
            cliente->idCliente = clientId;
            cliente->acertos = 0;
            cliente->erros = 0;
            cliente->jogos = 0;
            HASH_ADD_INT(clientsStatsHash, idCliente, cliente);
            globalStats.totalClientes++;
        }
        cliente->acertos++;
        writerUnlock();  // Unlock de ESCRITA
    } else {
        strcpy(messageToClient, "Wrong\n");
        printf("A thread consumidora %lu avaliou a tentativa do cliente %d e esta estava errada\n" , pthread_self(), clientId);
        writeLogf("../Servidor/log_servidor.txt", "Cliente %d: resposta ERRADA na posição [%d,%d] do jogo %d", clientId, rowSelected, columnSelected, gameId);
        
        // Incrementar estatísticas de erros
        writerLock();  // Lock de ESCRITA - acesso exclusivo
        globalStats.totalErros++;
        globalStats.totalJogadas++;
        
        ClientStatsServer *cliente;
        HASH_FIND_INT(clientsStatsHash, &clientId, cliente);
        if (!cliente) {
            // Criar cliente se não existir
            cliente = malloc(sizeof(ClientStatsServer));
            cliente->idCliente = clientId;
            cliente->acertos = 0;
            cliente->erros = 0;
            cliente->jogos = 0;
            HASH_ADD_INT(clientsStatsHash, idCliente, cliente);
            globalStats.totalClientes++;
        }
        cliente->erros++;
        writerUnlock();  // Unlock de ESCRITA
    }
    
    printf("A thread consumidora %lu está a preparar-se para enviar a resposta da tentativa ao cliente %d\n" , pthread_self(), clientId);

    if (writeSocket(socket, messageToClient, strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client partial solution or the client disconnected from the socket");
        handleClientDisconnection(socket);
        return;   
    }

    printf("A thread consumidora %lu enviou a resposta ao cliente %d com sucesso\n" , pthread_self(), clientId);
    
    // Atualizar ficheiro de estatísticas
    writeServerStats();
}

// --------------------------------------
// Verifica soluções completas do cliente
// --------------------------------------
void verifyClientCompleteSolution(int socket , int gameId , char completeSolution[82], int clientId)
{
    gameData *gameSentToClient;
    pthread_mutex_lock(&mutexGameData); // Protege acesso à hash table
    HASH_FIND_INT(gamesHashTable, &gameId, gameSentToClient); // Procura o jogo na hashTable utilizando o seu id
    pthread_mutex_unlock(&mutexGameData);

    if (!gameSentToClient)
    {   
        perror("Error : Server could not find a game with the id sent from the client");
        handleClientDisconnection(socket);
        return;
    }

    char messageToClient[BUFFER_SOCKET_MESSAGE_SIZE];

    // Passa por todas as células do Sudoku e envia ao cliente a primeira célula incorreta
    for(int row = 0 ; row < 9 ; row++)
    {
        for(int column = 0 ; column < 9 ; column++)
        {
            if(gameSentToClient->totalSolution[row * 9 + column] != completeSolution[row * 9 + column])
            {
                sprintf(messageToClient , "%d,%d\n" , row , column);

                printf("A thread consumidora %lu encontrou um erro na celula %d %d da solucao completa enviada pelo cliente %d\n" , pthread_self(), row , column, clientId);

                // Atualizar estatísticas - solução completa incorreta
                writerLock();
                globalStats.totalErros++;
                globalStats.totalJogadas++;
                
                ClientStatsServer *cliente;
                HASH_FIND_INT(clientsStatsHash, &clientId, cliente);
                if (!cliente) {
                    cliente = malloc(sizeof(ClientStatsServer));
                    cliente->idCliente = clientId;
                    cliente->acertos = 0;
                    cliente->erros = 0;
                    cliente->jogos = 0;
                    HASH_ADD_INT(clientsStatsHash, idCliente, cliente);
                    globalStats.totalClientes++;
                }
                cliente->erros++;
                writerUnlock();
                
                writeServerStats();

                if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
                {
                    perror("Error : Server could not write the answer of the client complete solution or the client disconnected from the socket");
                    handleClientDisconnection(socket);
                    return;
                }

                return;
            }
        }
    }

    printf("A thread consumidora %lu nao encontrou nenhum erro na solucao completa enviada pelo cliente %d\n", pthread_self(), clientId);
    writeLogf("../Servidor/log_servidor.txt", "Cliente %d: solução COMPLETA CORRETA do jogo %d", clientId, gameId);
    
    // Atualizar estatísticas - solução completa correta
    writerLock();
    globalStats.totalAcertos++;
    globalStats.totalJogadas++;
    
    ClientStatsServer *cliente;
    HASH_FIND_INT(clientsStatsHash, &clientId, cliente);
    if (!cliente) {
        cliente = malloc(sizeof(ClientStatsServer));
        cliente->idCliente = clientId;
        cliente->acertos = 0;
        cliente->erros = 0;
        cliente->jogos = 0;
        HASH_ADD_INT(clientsStatsHash, idCliente, cliente);
        globalStats.totalClientes++;
    }
    cliente->acertos++;
    writerUnlock();
    
    writeServerStats();
    
    strcpy(messageToClient, "Correct\n");

    if(writeSocket(socket , messageToClient , strlen(messageToClient)) != strlen(messageToClient))
    {
        perror("Error : Server could not write the answer of the client complete solution or the client disconnected from the socket");
        handleClientDisconnection(socket);
        return;
    }
}