#include "unix.h"
#include <stdio.h>
#include <errno.h>
#include "util.h"

int lerSocket(int socket , char *buffer , int numeroBytes)
{   
    int bytesPorLer = numeroBytes;
    int bytesLidos;

	while (bytesPorLer > 0)
	{
		bytesLidos = read(socket , buffer , bytesPorLer);

		if (bytesLidos < 0)
			return (bytesLidos);

		else if (bytesLidos == 0)
			break;

		bytesPorLer -= bytesLidos;
		buffer += bytesPorLer;
	}
	return (numeroBytes - bytesPorLer);
}

int escreveSocket(int socket , char *buffer , int numeroBytes)
{
	int bytesPorEscrever = numeroBytes; 
    int bytesEscritos;

	while (bytesPorEscrever > 0)
	{
		bytesEscritos = write(socket , buffer , numeroBytes);

		if (bytesEscritos <= 0)
			return (bytesEscritos);

		bytesPorEscrever -= bytesEscritos;
		buffer += bytesEscritos;
	}
	return (numeroBytes - bytesPorEscrever);
}

int lerLinha(int socket, char *buffer, int tamanhoMaximo)
{   
    int numeroCaracteresLidos , resultado;
    char caracter;

	for (numeroCaracteresLidos = 1; numeroCaracteresLidos < tamanhoMaximo; numeroCaracteresLidos++)
	{
		if ((resultado = read(socket , &caracter , 1)) == 1)
		{
			*buffer++ = caracter;
			if (caracter == '\n')
				break;
		}
		else if (resultado == 0)
		{
			if (numeroCaracteresLidos == 1)
				return (0);
			else
				break;
		}
		else
			return (-1);
	}

	/* Não esquecer de terminar a string */
	*buffer = 0;

	/* Note-se que n foi incrementado de modo a contar
	   com o \n ou \0 */
	return (numeroCaracteresLidos);
}