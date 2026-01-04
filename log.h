#ifndef LOG_H
#define LOG_H

#include <pthread.h>

// Mutex global para proteger escritas em ficheiros (logs e stats)
extern pthread_mutex_t mutexFileWrite;

void initLogSystem();
void writeLogf(const char *filePath, const char *format, ...);

void registerGameTime(char *horaInicio, char *horaFim, char *tempoJogo, size_t tamanho);

void registerGameTimeLive(char *horaInicio, size_t size);

void currentTime(char *buffer, size_t size);

#endif