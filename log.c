#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"

// Mutex global para proteger escritas em ficheiros
pthread_mutex_t mutexFileWrite = PTHREAD_MUTEX_INITIALIZER;

// -------------------------------------------------------------------------------------------------
// Inicializa o sistema de logs (caso necessário reinicializar o mutex)
// -------------------------------------------------------------------------------------------------
void initLogSystem() {
    pthread_mutex_init(&mutexFileWrite, NULL);
}

// -------------------------------------------------------------------------------------------------
// Escreve um log formatado num ficheiro. Pode conter mais argumentos que serão juntados à string 
// -------------------------------------------------------------------------------------------------
void writeLogf(const char *filePath, const char *format, ...) { 
    pthread_mutex_lock(&mutexFileWrite);  // PROTEGER ESCRITA NO FICHEIRO
    
    FILE *file = fopen(filePath, "a");
    
    if (!file) 
    {
        perror("Error opening/creating log file");
        pthread_mutex_unlock(&mutexFileWrite);
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);  
    fprintf(file, "\n");           
    va_end(args);

    fclose(file);
    
    pthread_mutex_unlock(&mutexFileWrite);  // LIBERTAR MUTEX
}

// ------------------------------------------------------------------
// Obtém a hora atual do sistema e a formata como string
// ------------------------------------------------------------------
void currentTime(char *buffer, size_t size) {
    time_t current = time(NULL);
    struct tm *tm_info = localtime(&current);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// ------------------------------------------------------------------
// Regista o início de um evento escrevendo a hora numa variável
// ------------------------------------------------------------------
void registerGameTimeLive(char *horaInicio, size_t size) {
    currentTime(horaInicio, size);
}


// ---------------------------
// Calcula duração do jogo
// ---------------------------
void registerGameTime(char *horaInicio, char *horaFim, char *tempoJogo, size_t tamanho) {
    struct tm tm_inicio = {0}, tm_fim = {0};
    int ano, mes, dia, h, m, s;

    sscanf(horaInicio, "%d-%d-%d %d:%d:%d", &ano, &mes, &dia, &h, &m, &s);
    tm_inicio.tm_year = ano - 1900;
    tm_inicio.tm_mon  = mes - 1;
    tm_inicio.tm_mday = dia;
    tm_inicio.tm_hour = h;
    tm_inicio.tm_min  = m;
    tm_inicio.tm_sec  = s;

    sscanf(horaFim, "%d-%d-%d %d:%d:%d", &ano, &mes, &dia, &h, &m, &s);
    tm_fim.tm_year = ano - 1900;
    tm_fim.tm_mon  = mes - 1;
    tm_fim.tm_mday = dia;
    tm_fim.tm_hour = h;
    tm_fim.tm_min  = m;
    tm_fim.tm_sec  = s;

    time_t t_inicio = mktime(&tm_inicio);
    time_t t_fim = mktime(&tm_fim);

    double duracao = difftime(t_fim, t_inicio);
    if (duracao < 0) duracao += 24 * 3600;

    int horas = (int)duracao / 3600;
    int minutos = ((int)duracao % 3600) / 60;
    int segundos = (int)duracao % 60;

    // Preenche a variável tempoJogo com formato "HH:MM:SS"
    snprintf(tempoJogo, tamanho, "%02d:%02d:%02d", horas, minutos, segundos);
}
