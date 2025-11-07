ProjetoSo : Cliente/cliente.o Servidor/servidor.o 
	gcc Cliente/cliente.o Servidor/servidor.o -o Build/build

cliente.o : Cliente/cliente.c Cliente/cliente.h
	gcc -c Cliente/cliente.c

servidor.o : Servidor/servidor.c Servidor/servidor.h include/uthash.h
	gcc -c Servidor/servidor.c

clean :
	rm -f Cliente/*.o Servidor/*.o Build/build