#include <pthread.h>

#include <string.h>

enum cellState 
{
    IDLE ,
    WAITING ,
    FINISHED
};

typedef struct SudokuCell 
{
    int value;
    pthread_mutex_t mutex;
    pthread_cond_t condAnswer;
    enum cellState state;
}sudokuCell;

// --------------------------------------------------------------
// Obter respostas possíveis para uma célula específica do Sudoku
// --------------------------------------------------------------
int* getPossibleAnswers(char partialSolution[81], int rowSelected, int columnSelected , int *size)
{   
    // Inicializa array com todos os possíveis valores numa celula (1 a 9)
    int allPossibleAnswers[] = {1,2,3,4,5,6,7,8,9};
    int arraySize = 9; // contador do número de possíveis respostas validas restantes

    // Remove valores já presentes na mesma linha e coluna
    for(int i = 0 ; i < 9 ; i++)
    {   
        int rowIndex = rowSelected * 9 + i;
        int columnIndex = i * 9 + columnSelected;

        // Se a linha já contém o número, remove da lista de possíveis
        if(partialSolution[rowIndex] != '0' && allPossibleAnswers[partialSolution[rowIndex] - '1'] != 0)
        {
            allPossibleAnswers[partialSolution[rowIndex] - '1'] = 0; //põe o valor repetido a zeros no array de respostas possíveis
            arraySize--; //diminui o tamanho do array que contém o número das respostas válidas
        }

        // Se a coluna já contém o número, remove da lista de possíveis
        if(partialSolution[columnIndex] != '0' && allPossibleAnswers[partialSolution[columnIndex] - '1'] != 0)
        {
            allPossibleAnswers[partialSolution[columnIndex] - '1'] = 0; // lógica igual às linhas
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
            int index = (startRow + i) * 9 + (startColumn + j);

            if(partialSolution[index] != '0' && allPossibleAnswers[partialSolution[index] - '1'] != 0) // verifica se o valor da célula não é zero e se não é um valor repetido
            {   
                allPossibleAnswers[partialSolution[index] - '1'] = 0;  // põe o valor (referente à posicao no bloco) a 0 no array das respostas válidas
                arraySize--;                     // decrementa o número de respostas válidas
            }
        }
    }

    // Aloca memória para armazenar dinamicamente os números válidos que podem ser colocados na célula
    int *possibleAnswersBasedOnPlacement = malloc(arraySize * sizeof(int)); 
    if (!possibleAnswersBasedOnPlacement) 
    {
        perror("Erro : Não foi possível alocar memória no array dinâmico onde fica armazenado as respostas possíveis");
        exit(1); // termina programa se falhar a alocação
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

// -----------------------------------------------------------------------------------
// Função que inicializa o sudoku onde cada célula tem o seu valor,estado,mutex e cond 
// -----------------------------------------------------------------------------------
void inicializeSudoku(sudokuCell *sudoku[9][9] , char partialSolution[81])
{   
    //Recebe o tamanho do array de caracteres
    int partialSolutionSize = strlen(partialSolution);

    //Se o array de caracteres não for igual a 81 significa que o jogo foi mal passado
    if(partialSolutionSize != 81)
        return;

    //Inicializa as variáveis que iremos usar como indexes para o array bidimensional
    int cellLine = 0;
    int cellColumn = 0;

    for(int i = 0 ; i < partialSolutionSize ; i++)
    {   
        //Pega o endereço de uma das células do sudoku
        sudokuCell *cell = sudoku[cellLine][cellColumn];

        //Preenche os valores da células , subtraimos o value por '0' para passar de ASCII para int , inicializamos o mutex e o cond 
        //e definimos o estado , se o valor seja 0 significa que ainda precisa ser preenchido logo vai para idle caso contrário assume que a célula já veio preenchida
        cell->value = partialSolution[i] - '0';
        pthread_mutex_init(&cell->mutex , NULL);
        pthread_cond_init(&cell->condAnswer , NULL);
        cell->state = cell->value == 0 ? IDLE : FINISHED;

        //Incrementa a coluna
        cellColumn++;
        
        //Caso a coluna seja igual a 9 após a incrementação significa que já chegamos ao final da linha , ent zeramos a coluna e passamos para a próxima linha
        if(cellColumn == 9)
        {
            cellLine++;
            cellColumn = 0;
        }
    }
}

void producer(sudokuCell *sudoku[9][9] , char partialSolution[81] , int emptyPosition[][])
{
    while(1)
    {
        
    }
}