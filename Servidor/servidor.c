#include "servidor.h"

#define maxLineSizeInCsv 166  // Define o tamanho máximo de cada linha do ficheiro jogos.csv
#define NUM_GAMES_TO_SEND 5//Tirar mais tarde
gameData *gamesHashTable = NULL;

// --------------------------------------------------------------
// Transforma strings em inteiros formando um array bidimensional
// --------------------------------------------------------------
void stringToBidimensionalArray(int solution[9][9] , char solutionInString[82]) 
{                                       // percorre cada caractere na coluna Jogo em jogos.csv e transforma em inteiro                                               
    for(int row = 0 ; row < 9 ; row++)  //e associa esse inteiro a uma célula no jogo Suduku até ter o array bidimensional completo (9x9)
    {
        for(int column = 0 ; column < 9 ; column++)
        {
            solution[row][column] = solutionInString[row * 9 + column] - '0';
        }
    }
}

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

        stringToBidimensionalArray(currentGame->partialSolution,dividedLine);  // Preenche o Sudoku parcial

        dividedLine = strtok(NULL,",");  
        if (dividedLine == NULL) {free(currentGame);continue;}        // Próxima coluna: solução completa

        stringToBidimensionalArray(currentGame->totalSolution,dividedLine);    // Preenche solução completa

        HASH_ADD_INT(gamesHashTable , id , currentGame);
    }

    fclose(file);  // Fecha o arquivo CSV
}
//verfica as respostas do cliente em relacao ao sudoku
char* verifyClientSudokuAnswer(int clientID , int gameId , int rowSelected , int columnSelected , int clientAnswer) 
{                                 // função serve para verificar se um certo valor válido para X Y coordenadas é de facto o correto
    gameData *gameSentToClient;
    HASH_FIND_INT(gamesHashTable , &gameId , gameSentToClient);

    if(!gameSentToClient)
    {
        printf("Erro : Id de jogo enviado pelo cliente is invalido");
        return NULL;
    }

    if(rowSelected < 0 || rowSelected > 8 || columnSelected < 0 || columnSelected > 8)
    {
        printf("Erro : Coordenadas enviadas pelo cliente sao invalidas");
        return NULL;
    }

    char* string = malloc(50);

    printf("A solucao is o numero %d \n" , gameSentToClient->totalSolution[rowSelected][columnSelected]);
    if(gameSentToClient->totalSolution[rowSelected][columnSelected] == clientAnswer)
    {
        printf("Correto\n");
        return strcpy(string , "Correto");
    }

    printf("Errado\n");
    return strcpy(string , "Errado");
}
//funcao para enviar o jogo ao cliente
gameData* sendGameToClient(int clientId)  // dá ao cliente um jogo aleatório
{   
    int count = HASH_COUNT(gamesHashTable);
    if(count == 0)return NULL;

    int index = rand() % count;
    int i = 0;
    gameData * game;

    for(game = gamesHashTable ; game != NULL ; game = game->hh.next)
    {
        if(index == i)
        {   
            gameData* gameCopy = malloc(sizeof(gameData));
            if(gameCopy == NULL) return NULL;

            memcpy(gameCopy , game , sizeof(gameData));
            memset(&gameCopy->hh, 0, sizeof(UT_hash_handle));

            return gameCopy; 
        } 
        i++;
    }

    return NULL;
}
