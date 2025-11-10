all : buildCliente buildServidor

buildCliente : Cliente/socketsCliente.o Cliente/utilsCliente.o 
	gcc -o Build/buildCliente Cliente/socketsCliente.o Cliente/utilsCliente.o

buildServidor : Servidor/socketsServidor.o Servidor/utilsServidor.o 
	gcc -o Build/buildServidor Servidor/socketsServidor.o Servidor/utilsServidor.o

Servidor/socketsServidor.o : Servidor/socketsServidor.c Servidor/utilsServidor.h unix.h
	gcc -c Servidor/socketsServidor.c -o Servidor/socketsServidor.o

Servidor/utilsServidor.o : Servidor/utilsServidor.c Servidor/utilsServidor.h
	gcc -c Servidor/utilsServidor.c -o Servidor/utilsServidor.o

Cliente/socketsCliente.o : Cliente/socketsCliente.c Cliente/utilsCliente.h unix.h
	gcc -c Cliente/socketsCliente.c -o Cliente/socketsCliente.o

Cliente/utilsCliente.o : Cliente/utilsCliente.c Cliente/utilsCliente.h
	gcc -c Cliente/utilsCliente.c -o Cliente/utilsCliente.o

clean :
	rm -f Cliente/*.o Servidor/*.o Build/buildCliente Build/buildServidor
