#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "../include/uthash.h" // <--- isto garante que UT_hash_handle é conhecido

// Estrutura que representa um jogo do Sudoku
typedef struct gamedata{
    int id;                          // identificador do jogo
    char partialSolution[81];       // array bidimensional com a solução parcial (jogo inicial)
    char totalSolution[81];         // array bidimensional com a solução completa
    UT_hash_handle hh; 
} gameData;


// Estrutura que representa uma sala multijogador no servidor
// É usada para agrupar jogadores que vão partilhar o mesmo jogo Sudoku
// e garantir que todos começam ao mesmo tempo
typedef struct sala_multijogador {

    // Identificador único do jogo Sudoku atribuído à sala
    // Corresponde ao ID lido do ficheiro CSV
    int gameId;

    // Grelha parcial do Sudoku (estado inicial do jogo)
    // Contém exatamente 81 caracteres (9x9)
    // O espaço extra é reservado para o caractere terminador '\0'
    char partialSolution[82];   // 81 + '\0'

    // Número de clientes atualmente associados à sala
    // Serve para saber quantos jogadores já entraram
    // e quando a sala fica vazia
    int numClientes;

    // Barreira de sincronização POSIX
    // Usada para bloquear os jogadores até que
    // todos os participantes da sala estejam prontos
    pthread_barrier_t barrier;

    // Flag que indica se a barreira já foi inicializada
    // 0 -> barreira não inicializada
    // 1 -> barreira inicializada
    // Evita múltiplas inicializações indevidas da barreira
    int barrierInicializada;

} SalaMultijogador;


// Funções implementadas em socketUtils.c
void readGamesFromCSV();
void verifyClientSudokuAnswer(int socket, int gameId, int rowSelected, int columnSelected, int clientAnswer , int clientId);
void sendGameToClient(int socket, int clientId,int numeroJogadoresSala);

#endif
