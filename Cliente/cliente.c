#include "cliente.h"
#include "../Servidor/servidor.h"

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(int partialSolution[9][9], int rowSelected, int columnSelected , int *size)
{
    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        // Se a linha já contém o número, remove da lista de possíveis
        if(partialSolution[rowSelected][i] != 0 && allPossibleAnswers[partialSolution[rowSelected][i] - 1] != 0)
        {
            allPossibleAnswers[partialSolution[rowSelected][i] - 1] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if(partialSolution[i][columnSelected] != 0 && allPossibleAnswers[partialSolution[i][columnSelected] - 1] != 0)
        {
            allPossibleAnswers[partialSolution[i][columnSelected] - 1] = 0; // lógica igual às linhas
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
            int val = partialSolution[startRow + i][startColumn + j];
            if(val != 0 && allPossibleAnswers[val - 1] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[val - 1] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
                arraySize--;                     // decrementa o número de respostas válidas
            }
        }
    }

    // Aloca memória para armazenar dinamicamente os números válidos que podem ser colocados na célula
    int *possibleAnswersBasedOnPlacement = malloc(arraySize * sizeof(int)); 
    if (!possibleAnswersBasedOnPlacement) 
    {
        printf("Erro ao alocar memoria!\n");
        exit(EXIT_FAILURE); // termina programa se falhar a alocação
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

// ------------------------------------------------------------
// Mostrar Sudoku com coordenadas
// ------------------------------------------------------------
void displaySudokuWithCoords(int sudoku[9][9]) 
{
    // Cabeçalho com coordenadas das colunas (X)
    printf("xy "); // marcador no canto superior esquerdo
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
            if (sudoku[row][col] == 0)
                printf("-");  // se for 0 então não há valor ainda, substituimos por "-"
            else
                printf("%d", sudoku[row][col]); // imprime o número presente na célula

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

void shufflePositionsInArray(int positions[][2] , int count) // funcao para baralhar as coordenadas num array
{
    for(int i = count - 1 ; i > 0 ; i--)
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

// ------------------------------------------------------------------
// Input do utilizador ( valor em X Y célula) e modificação do sudoku
// ------------------------------------------------------------------
void fillSudoku(int sudoku[9][9] , int gameId,LogJogo *log) 
{   
    int positionsToFill[81][2];
    int emptyCount = 0;
    log->failedAttempts = 0; // variavel que conta as tentativas falhadas de um jogo

    for(int row = 0 ; row < 9 ; row++)
    {
        for(int column = 0 ; column < 9 ; column++)
        {
            if(sudoku[row][column] == 0)
            {
                positionsToFill[emptyCount][0] = row;
                positionsToFill[emptyCount][1] = column;
                emptyCount++;
            }
        }
    }

    shufflePositionsInArray(positionsToFill , emptyCount); // esta funcao faz com que os valores no array ( coordenadas vazias ) sejam baralhados

    for(int i = 0 ; i < emptyCount ; i++)
    {   
        printf("Empty left: %d\n", emptyCount - i - 1);
        int rowSelected = positionsToFill[i][0];
        int columnSelected = positionsToFill[i][1];

        int size = 0; 
        int* possibleAnswers = getPossibleAnswers(sudoku, rowSelected , columnSelected , &size); //valor de size (número de respostas válidas) vem da função getPossibleAnswers 

        printf("Possiveis valores para (%d,%d): ", rowSelected, columnSelected);
        for(int j = 0; j < size; j++) {printf("%d ", possibleAnswers[j] , " ");}//imprime valores possíveis para uma certa posição na célula
    
        for(int j = 0 ; j < size ; j++)
        {   
            printf("\nFoi enviado para o server o numero %d na posicao : %d %d \n" , possibleAnswers[j] , rowSelected , columnSelected);
            char* serverAnswer = verifyClientSudokuAnswer(1,gameId,rowSelected,columnSelected,possibleAnswers[j]);

            if(strcmp(serverAnswer,"Correto") == 0)
            {   
                sudoku[rowSelected][columnSelected] = possibleAnswers[j];
                displaySudokuWithCoords(sudoku);
                free(serverAnswer);
                break;
            }
            else
                log->failedAttempts++;
            
            free(serverAnswer);
        }

        free(possibleAnswers);
    }
}


// ------------------------------------------------------------------
// Obtém a hora atual do sistema e a formata como string
// ------------------------------------------------------------------
static void currentTime(char *buffer, size_t size) {   // função para nos dar a hora atual
    time_t current = time(NULL);   // Obtém o tempo atual em segundos desde 1970
    struct tm *tm_info = localtime(&current); // Converte para tempo local (ano, mês, dia, hora, etc)
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info); // Formata como string "YYYY-MM-DD HH:MM:SS"
} 


// -------------------------------------------------------------------
// Regista o início de um jogo dentro da struct LogJogo
// Inicializa a hora de início e prepara a struct para futuras informações
// -------------------------------------------------------------------
static void registerGameStart(LogJogo *log, int id) { //função que regista em uma struct LogJogo a sua hora de Inicio
    log->idJogo = id;
    currentTime(log->horaInicio, sizeof(log->horaInicio));
    strcpy(log->horaFim, "-");
    //registarInicioJogo
}


// -------------------------------------------------
// Gravação dos detalhes de um jogo num ficheiro CSV
// -------------------------------------------------
static void writeLogToFile(LogJogo *log, int horas, int minutos, int segundos) {
    FILE *fp = fopen("Cliente/log_cliente.csv", "a+");
    if (!fp) { perror("Erro ao abrir ficheiro"); return; }

    fseek(fp, 0, SEEK_END); // Se o arquivo está vazio, escreve no cabeçalho
    if(ftell(fp) == 0)
        fprintf(fp, "ID_JOGO,HORA_INICIO,HORA_FIM,DURACAO,TENTATIVAS_FALHADAS\n");

    fprintf(fp, "%d,%s,%s,%02d:%02d:%02d,%d\n",         // imprime no ficheiro CSV os detalhes de um jogo
            log->idJogo, log->horaInicio, log->horaFim, horas, minutos, segundos,log->failedAttempts); 

    fclose(fp); // fecha o ficheiro
}

// ---------------------------------------------------------
// Registra a hora final de um jogo e calcula a sua duração.
// Chama a função que vai escrever no ficheiro CSV
// ---------------------------------------------------------
static void registerGameEndWithLog(LogJogo *log) 
{  // Regista numa struct LogJogo a sua hora de Fim. Calcula tambem a duração de cada jogo
    currentTime(log->horaFim, sizeof(log->horaFim));

    struct tm tm_inicio = {0}, tm_fim = {0}; // Cria structs tm para manipular datas/hora
    int ano, mes, dia, h, m, s;

    // Parse horaInicio
    sscanf(log->horaInicio, "%d-%d-%d %d:%d:%d", &ano,&mes,&dia,&h,&m,&s);
    tm_inicio.tm_year = ano - 1900;  // Anos desde 1900
    tm_inicio.tm_mon  = mes - 1; // Meses de 0 a 11
    tm_inicio.tm_mday = dia;
    tm_inicio.tm_hour = h;
    tm_inicio.tm_min  = m;
    tm_inicio.tm_sec  = s;

    // Parse horaFim
    sscanf(log->horaFim, "%d-%d-%d %d:%d:%d", &ano,&mes,&dia,&h,&m,&s);
    tm_fim.tm_year = ano - 1900;
    tm_fim.tm_mon  = mes - 1;
    tm_fim.tm_mday = dia;
    tm_fim.tm_hour = h;
    tm_fim.tm_min  = m;
    tm_fim.tm_sec  = s;

    // Converte para time_t (segundos desde 1970)
    time_t t_inicio = mktime(&tm_inicio);
    time_t t_fim = mktime(&tm_fim);

    double duracao = difftime(t_fim, t_inicio);
    if(duracao < 0) duracao += 24*3600; // Ajuste caso passe da meia-noite

    // Converte duração para horas, minutos e segundos
    int horas = (int)duracao / 3600;
    int minutos = ((int)duracao % 3600) / 60;
    int segundos = (int)duracao % 60;

    writeLogToFile(log, horas, minutos, segundos);
}

void printCSV(const char* filename) {
    FILE* file = fopen(filename, "r");
    if(!file) {
        perror("Erro ao abrir o ficheiro");
        return;
    }

    char linha[1024];  // buffer para cada linha
    while(fgets(linha, sizeof(linha), file)) {
        printf("%s", linha);  // fgets mantém o '\n' no final
    }

    fclose(file);
}

int main() {
    srand(time(NULL));
    readGamesFromCSV();

    int continuar = 1; // controla o loop do programa

    while(continuar) {
        int escolha;
        do {
            printf("\nEscolha uma opcao:\n");
            printf("0 = Jogar\n");
            printf("1 = Digitalizar jogos do ficheiro jogos.csv\n");
            printf("2 = Sair\n");
            printf("Opcao: ");
            scanf("%d", &escolha);
        } while(escolha != 0 && escolha != 1 && escolha != 2);

        if(escolha == 1) {
            printf("\n--- Digitalizando jogos de jogos.csv ---\n");
            printCSV("Servidor/jogos.csv");
        }
        else if(escolha == 0) {
            // Jogar um jogo
            gameData* game = sendGameToClient(1); // id do cliente
            displaySudokuWithCoords(game->partialSolution);

            LogJogo logAtual;
            registerGameStart(&logAtual, game->id);

            fillSudoku(game->partialSolution, game->id, &logAtual);

            registerGameEndWithLog(&logAtual);

            free(game);
        }
        else if(escolha == 2) {
            continuar = 0;
            printf("Programa terminado.\n");
        }
    }

    return 0;
}
