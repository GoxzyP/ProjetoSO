# -----------------------------------------------------------
# Configurações Gerais
# -----------------------------------------------------------

# Compilador a usar
CC = gcc

# Opções de compilação normais
CFLAGS = -Wall -g -O0 -Iinclude -pthread -fsanitize=address -fno-omit-frame-pointer

# Opções de compilação debug
CFLAGS_DEBUG = -Wall -g -O0 -fsanitize=address -fno-omit-frame-pointer -Iinclude -pthread 

# Diretórios principais do projeto
BUILD_DIR = Build
CLIENT_DIR = Cliente
SERVER_DIR = Servidor

# -----------------------------------------------------------
# Ficheiros Fonte
# -----------------------------------------------------------

# Lista de ficheiros fonte do cliente
CLIENT_SRCS = $(CLIENT_DIR)/socketsSetupClienteSinglePlayer.c \
              $(CLIENT_DIR)/socketsUtilsCliente.c \
              $(CLIENT_DIR)/socketsUtilsClienteSinglePlayer.c \
			  $(CLIENT_DIR)/singlePlayerSolucaoCompleta.c \
              util.c \
              log.c

# Lista de ficheiros fonte do servidor
SERVER_SRCS = $(SERVER_DIR)/socketsSetupServidorSinglePlayer.c \
              $(SERVER_DIR)/socketsUtilsServidor.c \
              $(SERVER_DIR)/socketsUtilsServidorSinglePlayer.c \
              $(SERVER_DIR)/servidorGameManager.c \
              util.c \
              log.c

# Converte cada ficheiro .c numa versão .o dentro da pasta Build/
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(BUILD_DIR)/%.o)
SERVER_OBJS = $(SERVER_SRCS:%.c=$(BUILD_DIR)/%.o)

# -----------------------------------------------------------
# Alvos Principais (executáveis finais)
# -----------------------------------------------------------

all: cliente servidor

# Alvos individuais
cliente: $(BUILD_DIR)/cliente_exec
servidor: $(BUILD_DIR)/servidor_exec

# Versões debug
debug: cliente_debug servidor_debug

cliente_debug: $(CLIENT_OBJS:.o=_debug.o)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS_DEBUG) $^ -o $(BUILD_DIR)/cliente_exec_debug
	@echo "✅ Cliente (debug) compilado: $(BUILD_DIR)/cliente_exec_debug"

servidor_debug: $(SERVER_OBJS:.o=_debug.o)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS_DEBUG) $^ -o $(BUILD_DIR)/servidor_exec_debug
	@echo "✅ Servidor (debug) compilado: $(BUILD_DIR)/servidor_exec_debug"

# -----------------------------------------------------------
# Compilação de cada ficheiro .c em .o
# -----------------------------------------------------------

# Regra genérica para versão normal
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compilado: $< -> $@"

# Regra genérica para versão debug
$(BUILD_DIR)/%_debug.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@
	@echo "Compilado (debug): $< -> $@"

# -----------------------------------------------------------
# Ligação (Link) normal
# -----------------------------------------------------------

$(BUILD_DIR)/cliente_exec: $(CLIENT_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "✅ Cliente compilado: $@"

$(BUILD_DIR)/servidor_exec: $(SERVER_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "✅ Servidor compilado: $@"

# -----------------------------------------------------------
# Limpeza
# -----------------------------------------------------------

.PHONY: clean
clean:
	@echo "🧹 A limpar ficheiros compilados..."
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/$(CLIENT_DIR) $(BUILD_DIR)/$(SERVER_DIR) \
	       $(BUILD_DIR)/cliente_exec $(BUILD_DIR)/servidor_exec \
	       $(BUILD_DIR)/cliente_exec_debug $(BUILD_DIR)/servidor_exec_debug
	@echo "Limpeza concluída."
