#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#include "../include/uthash.h"
#include "../util.h"
#include "socketsUtilsServidor.h"
#include "../log.h"

#define maxLineSizeInCsv 166   // Define o tamanho máximo de cada linha do ficheiro jogos.csv
#define NUM_GAMES_TO_SEND 5    // Tirar mais tarde
#define SHM_NAME "/sudoku_server_shm"

gameData *gamesHashTable = NULL;
SharedMemory *sharedMem = NULL;

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
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId, ClientStatsServer **clientsHash, ServerGlobalStats *globalStats)
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
        
        // Incrementar estatísticas
        globalStats->totalAcertos++;
        
        // Atualizar memória partilhada
        if (sharedMem) {
            writerLock();
            sharedMem->totalAcertos++;
            // Encontrar índice do cliente e incrementar seus acertos
            for (int i = 0; i < 1000; i++) {
                if (sharedMem->clienteIds[i] == clientId) {
                    sharedMem->clienteAcertos[i]++;
                    break;
                }
            }
            writerUnlock();
            updateRealtimeStats();
        }
        
        // Encontrar/criar cliente na hash table
        ClientStatsServer *cliente;
        HASH_FIND_INT(*clientsHash, &clientId, cliente);
        if (cliente) {
            cliente->acertos++;
        }
    }
    else
    {
        strcpy(serverResponse, "Wrong\n");
        writeLogf("../Servidor/log_servidor.csv","A resposta enviada pelo cliente %d está errada" , clientId);
        
        // Incrementar estatísticas
        globalStats->totalErros++;
        
        // Atualizar memória partilhada
        if (sharedMem) {
            writerLock();
            sharedMem->totalErros++;
            // Encontrar índice do cliente e incrementar seus erros
            for (int i = 0; i < 1000; i++) {
                if (sharedMem->clienteIds[i] == clientId) {
                    sharedMem->clienteErros[i]++;
                    break;
                }
            }
            writerUnlock();
            updateRealtimeStats();
        }
        
        // Encontrar/criar cliente na hash table
        ClientStatsServer *cliente;
        HASH_FIND_INT(*clientsHash, &clientId, cliente);
        if (cliente) {
            cliente->erros++;
        }
    }

    int responseLength = strlen(serverResponse);

    if (escreveSocket(socket, serverResponse, responseLength) != responseLength)
    {
        perror("Erro: O servidor não conseguiu enviar a resposta à solução do cliente");
        writeLogf("../Servidor/log_servidor.csv","O servidor não conseguiu enviar a resposta à solução do cliente %d" , clientId);
        exit(1);
    }

    writeLogf("../Servidor/log_servidor.csv","Foi enviado para o cliente %d o resultado da sua resposta com sucesso" , clientId);
    
    // Atualizar estatísticas em tempo real
    writeServerStats(globalStats, *clientsHash);
}

// --------------------------------------------------------------
// Envia um jogo aleatório para o cliente
// --------------------------------------------------------------
void sendGameToClient(int socket , int clientId, ClientStatsServer **clientsHash, ServerGlobalStats *globalStats)
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
            
            // Incrementar estatísticas
            globalStats->totalJogos++;
            
            // Atualizar memória partilhada
            if (sharedMem) {
                writerLock();
                sharedMem->totalJogos++;
                // Incrementar jogos do cliente
                for (int i = 0; i < 1000; i++) {
                    if (sharedMem->clienteIds[i] == clientId) {
                        sharedMem->clienteJogos[i]++;
                        break;
                    }
                }
                writerUnlock();
                updateRealtimeStats();
            }
            
            // Encontrar ou criar cliente na hash table
            ClientStatsServer *cliente;
            HASH_FIND_INT(*clientsHash, &clientId, cliente);
            if (!cliente) {
                cliente = malloc(sizeof(ClientStatsServer));
                cliente->idCliente = clientId;
                cliente->acertos = 0;
                cliente->erros = 0;
                cliente->jogos = 0;
                HASH_ADD_INT(*clientsHash, idCliente, cliente);
                globalStats->totalClientes++;
            }
            cliente->jogos++;
            
            // Atualizar estatísticas em tempo real
            writeServerStats(globalStats, *clientsHash);
            
            return;
        }

        i++;
    }
}

// --------------------------------------------------------------
// Escreve estatísticas do servidor no log
// --------------------------------------------------------------
void writeServerStats(ServerGlobalStats *globalStats, ClientStatsServer *clientsHash)
{
    // Calcular taxa de acerto global
    float taxaAcertoGlobal = 0.0;
    int totalJogadas = globalStats->totalAcertos + globalStats->totalErros;
    if (totalJogadas > 0) {
        taxaAcertoGlobal = ((float)globalStats->totalAcertos / (float)totalJogadas) * 100.0;
    }

    // Calcular média de jogadas por cliente
    float mediaJogadasPorCliente = 0.0;
    if (globalStats->totalClientes > 0) {
        mediaJogadasPorCliente = (float)totalJogadas / (float)globalStats->totalClientes;
    }

    // Encontrar cliente com melhor taxa de acerto
    int melhorClienteId = -1;
    float melhorTaxa = 0.0;
    ClientStatsServer *cliente, *tmp;
    
    HASH_ITER(hh, clientsHash, cliente, tmp) {
        int jogadasCliente = cliente->acertos + cliente->erros;
        if (jogadasCliente > 0) {
            float taxaCliente = ((float)cliente->acertos / (float)jogadasCliente) * 100.0;
            if (taxaCliente > melhorTaxa) {
                melhorTaxa = taxaCliente;
                melhorClienteId = cliente->idCliente;
            }
        }
    }

    // Criar caminho para ficheiro de estatísticas separado
    char statsPath[512] = "../Servidor/estatisticas_servidor.csv";

    // Obter número de clientes atualmente conectados
    int clientesAtivos = getActiveClientsCount();

    // SOBRESCREVER ficheiro de estatísticas
    FILE *file = fopen(statsPath, "w");  // Modo "w" sobrescreve
    if (!file) {
        perror("Erro ao abrir ficheiro de estatísticas do servidor");
        return;
    }

    fprintf(file, "========================================\n");
    fprintf(file, "ESTATISTICAS GLOBAIS DO SERVIDOR\n");
    fprintf(file, "========================================\n");
    fprintf(file, "1. Clientes Ativos (Conectados): %d\n", clientesAtivos);
    fprintf(file, "2. Taxa de Acerto Global: %.2f%%\n", taxaAcertoGlobal);
    fprintf(file, "3. Total de Acertos: %d\n", globalStats->totalAcertos);
    fprintf(file, "4. Total de Jogadas: %d\n", totalJogadas);
    fprintf(file, "5. Total de Jogos Distribuidos: %d\n", globalStats->totalJogos);
    fprintf(file, "6. Total de Erros: %d\n", globalStats->totalErros);
    if (melhorClienteId != -1) {
        fprintf(file, "7. Cliente com Melhor Taxa: ID %d com %.2f%%\n", melhorClienteId, melhorTaxa);
    } else {
        fprintf(file, "7. Cliente com Melhor Taxa: N/A\n");
    }
    fprintf(file, "8. Media de Jogadas por Cliente: %.2f\n", mediaJogadasPorCliente);
    fprintf(file, "========================================\n");
    
    fclose(file);
}

// --------------------------------------------------------------
// Atualiza estatísticas em tempo real (lê da memória partilhada)
// --------------------------------------------------------------
void updateRealtimeStats()
{
    if (!sharedMem) return;
    
    char statsPath[512] = "../Servidor/estatisticas_servidor.csv";
    
    readerLock();  // Lock de LEITURA - múltiplos leitores permitidos
    int clientesAtivos = sharedMem->clientesAtivos;
    
    // Calcular estatísticas somando APENAS clientes online
    int totalAcertos = 0;
    int totalErros = 0;
    int totalJogos = 0;
    int clientesOnlineCount = 0;
    int melhorClienteId = -1;
    float melhorTaxa = 0.0;
    
    for (int i = 0; i < 1000; i++) {
        if (sharedMem->clienteIds[i] != -1 && sharedMem->clienteOnline[i] == 1) {
            clientesOnlineCount++;
            totalAcertos += sharedMem->clienteAcertos[i];
            totalErros += sharedMem->clienteErros[i];
            totalJogos += sharedMem->clienteJogos[i];
            
            // Calcular taxa deste cliente
            int jogadasCliente = sharedMem->clienteAcertos[i] + sharedMem->clienteErros[i];
            if (jogadasCliente > 0) {
                float taxaCliente = ((float)sharedMem->clienteAcertos[i] / (float)jogadasCliente) * 100.0;
                if (taxaCliente > melhorTaxa || (taxaCliente == melhorTaxa && melhorClienteId == -1)) {
                    melhorTaxa = taxaCliente;
                    melhorClienteId = sharedMem->clienteIds[i];
                }
            }
        }
    }
    
    int totalJogadas = totalAcertos + totalErros;
    readerUnlock();  // Unlock de LEITURA
    
    // Calcular taxa de acerto
    float taxaAcerto = 0.0;
    if (totalJogadas > 0) {
        taxaAcerto = ((float)totalAcertos / (float)totalJogadas) * 100.0;
    }
    
    FILE *file = fopen(statsPath, "w");
    if (!file) return;
    
    fprintf(file, "========================================\n");
    fprintf(file, "ESTATISTICAS GLOBAIS DO SERVIDOR\n");
    fprintf(file, "========================================\n");
    fprintf(file, "1. Clientes Ativos (Conectados): %d\n", clientesAtivos);
    
    // Só mostrar outras estatísticas se houver jogadas de clientes online
    if (totalJogadas > 0) {
        fprintf(file, "2. Taxa de Acerto Global: %.2f%%\n", taxaAcerto);
        fprintf(file, "3. Total de Acertos: %d\n", totalAcertos);
        fprintf(file, "4. Total de Jogadas: %d\n", totalJogadas);
        fprintf(file, "5. Total de Jogos Distribuidos: %d\n", totalJogos);
        fprintf(file, "6. Total de Erros: %d\n", totalErros);
        if (melhorClienteId != -1) {
            fprintf(file, "7. Cliente com Melhor Taxa: ID %d com %.2f%%\n", melhorClienteId, melhorTaxa);
        } else {
            fprintf(file, "7. Cliente com Melhor Taxa: N/A\n");
        }
        fprintf(file, "8. Media de Jogadas por Cliente: %.2f\n", clientesOnlineCount > 0 ? (float)totalJogadas / clientesOnlineCount : 0.0);
    } else if (clientesAtivos > 0) {
        fprintf(file, "========================================\n");
        fprintf(file, "(Aguardando jogadas dos clientes)\n");
    } else {
        fprintf(file, "========================================\n");
        fprintf(file, "(Nenhum cliente conectado)\n");
    }
    fprintf(file, "========================================\n");
    
    fclose(file);
}

// --------------------------------------------------------------
// Inicializa memória compartilhada
// --------------------------------------------------------------
void initSharedMemory()
{
    // Tentar abrir primeiro (se já existe)
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    int already_exists = (shm_fd != -1);
    
    // Se não existe, criar
    if (!already_exists) {
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("Erro ao criar memória compartilhada");
            exit(1);
        }
        ftruncate(shm_fd, sizeof(SharedMemory));
    }

    sharedMem = mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sharedMem == MAP_FAILED) {
        perror("Erro ao mapear memória compartilhada");
        exit(1);
    }

    // Só inicializar se for reciém criada
    if (!already_exists) {
        // Inicializar semáforos para Leitor/Escritor
        sem_init(&sharedMem->mutexLeitores, 1, 1);
        sem_init(&sharedMem->mutexEscritores, 1, 1);
        sharedMem->numLeitores = 0;
        
        // Inicializar contadores
        sharedMem->clientesAtivos = 0;
        sharedMem->totalAcertos = 0;
        sharedMem->totalErros = 0;
        sharedMem->totalJogos = 0;
        sharedMem->totalJogadas = 0;
        
        for (int i = 0; i < 1000; i++) {
            sharedMem->clienteIds[i] = -1;
            sharedMem->clienteAcertos[i] = 0;
            sharedMem->clienteErros[i] = 0;
            sharedMem->clienteJogos[i] = 0;
            sharedMem->clienteOnline[i] = 0;
        }
    }
}

// --------------------------------------------------------------
// Lock para LEITURA (múltiplos leitores permitidos)
// --------------------------------------------------------------
void readerLock()
{
    if (!sharedMem) return;
    
    sem_wait(&sharedMem->mutexLeitores);
    sharedMem->numLeitores++;
    if (sharedMem->numLeitores == 1) {
        sem_wait(&sharedMem->mutexEscritores);  // Primeiro leitor bloqueia escritores
    }
    sem_post(&sharedMem->mutexLeitores);
}

// --------------------------------------------------------------
// Unlock para LEITURA
// --------------------------------------------------------------
void readerUnlock()
{
    if (!sharedMem) return;
    
    sem_wait(&sharedMem->mutexLeitores);
    sharedMem->numLeitores--;
    if (sharedMem->numLeitores == 0) {
        sem_post(&sharedMem->mutexEscritores);  // Último leitor libera escritores
    }
    sem_post(&sharedMem->mutexLeitores);
}

// --------------------------------------------------------------
// Lock para ESCRITA (escritor exclusivo)
// --------------------------------------------------------------
void writerLock()
{
    if (!sharedMem) return;
    sem_wait(&sharedMem->mutexEscritores);  // Bloqueia tudo
}

// --------------------------------------------------------------
// Unlock para ESCRITA
// --------------------------------------------------------------
void writerUnlock()
{
    if (!sharedMem) return;
    sem_post(&sharedMem->mutexEscritores);  // Libera tudo
}

// --------------------------------------------------------------
// Limpa memória compartilhada
// --------------------------------------------------------------
void cleanupSharedMemory()
{
    if (sharedMem != NULL) {
        munmap(sharedMem, sizeof(SharedMemory));
    }
    shm_unlink(SHM_NAME);
}

// --------------------------------------------------------------
// Incrementa contador de clientes ativos
// --------------------------------------------------------------
void incrementActiveClients()
{
    if (sharedMem == NULL) initSharedMemory();
    
    writerLock();
    sharedMem->clientesAtivos++;
    writerUnlock();
}

// --------------------------------------------------------------
// Decrementa contador de clientes ativos
// --------------------------------------------------------------
void decrementActiveClients()
{
    if (sharedMem == NULL) initSharedMemory();
    
    writerLock();
    if (sharedMem->clientesAtivos > 0) {
        sharedMem->clientesAtivos--;
    }
    writerUnlock();
}

// --------------------------------------------------------------
// Retorna número de clientes atualmente conectados
// --------------------------------------------------------------
int getActiveClientsCount()
{
    if (!sharedMem) return 0;
    
    int count;
    readerLock();  // Lock de LEITURA
    count = sharedMem->clientesAtivos;
    readerUnlock();
    
    return count;
}

// --------------------------------------------------------------
// Registra cliente único no sistema
// --------------------------------------------------------------
void registerUniqueClient(int clientId)
{
    if (!sharedMem) return;
    
    writerLock();
    
    // Procurar slot vazio ou cliente existente
    for (int i = 0; i < 1000; i++) {
        // Se já existe, apenas marcar como online
        if (sharedMem->clienteIds[i] == clientId) {
            sharedMem->clienteOnline[i] = 1;
            writerUnlock();
            return;
        }
        // Se encontrou slot vazio, adicionar novo
        if (sharedMem->clienteIds[i] == -1) {
            sharedMem->clienteIds[i] = clientId;
            sharedMem->clienteOnline[i] = 1;
            sharedMem->clienteAcertos[i] = 0;
            sharedMem->clienteErros[i] = 0;
            sharedMem->clienteJogos[i] = 0;
            writerUnlock();
            return;
        }
    }
    
    writerUnlock();
}

// --------------------------------------------------------------
// Marca cliente como offline (mas mantém ID para histórico)
// --------------------------------------------------------------
void unregisterClientOnline(int clientId)
{
    if (!sharedMem) return;
    
    writerLock();
    for (int i = 0; i < 1000; i++) {
        if (sharedMem->clienteIds[i] == clientId) {
            sharedMem->clienteOnline[i] = 0;
            break;
        }
    }
    writerUnlock();
}
