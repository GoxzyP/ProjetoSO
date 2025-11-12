# -----------------------------------------------------------
# Configurações Gerais
# -----------------------------------------------------------

# Compilador a usar
CC = gcc

# Opções de compilação:
# -Wall   → ativa todos os avisos comuns (boa prática)
# -g      → inclui informações de debug (para usar com gdb)
# -Iinclude → adiciona a pasta "include" ao caminho de pesquisa de ficheiros .h
CFLAGS = -Wall -g -Iinclude

# Diretórios principais do projeto
BUILD_DIR = Build
CLIENT_DIR = Cliente
SERVER_DIR = Servidor

# -----------------------------------------------------------
# Ficheiros Fonte
# -----------------------------------------------------------

# Lista de ficheiros fonte (código C) do cliente
# Inclui:
#   - Configuração e utilitários do socket (Cliente)
#   - util.c (ficheiro comum partilhado)
CLIENT_SRCS = $(CLIENT_DIR)/socketsSetupCliente.c $(CLIENT_DIR)/socketsUtilsCliente.c util.c

# Lista de ficheiros fonte do servidor
# Inclui:
#   - Configuração e utilitários do socket (Servidor)
#   - util.c (ficheiro comum partilhado)
SERVER_SRCS = $(SERVER_DIR)/socketsSetupServidor.c $(SERVER_DIR)/socketsUtilsServidor.c util.c

# Converte cada ficheiro .c numa versão .o dentro da pasta Build/
# Exemplo: Cliente/socketsSetupCliente.c → Build/Cliente/socketsSetupCliente.o
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(BUILD_DIR)/%.o)
SERVER_OBJS = $(SERVER_SRCS:%.c=$(BUILD_DIR)/%.o)

# -----------------------------------------------------------
# Alvos Principais (executáveis finais)
# -----------------------------------------------------------

# O alvo 'all' cria tanto o cliente como o servidor
all: cliente servidor

# Alvos individuais
cliente: $(BUILD_DIR)/cliente_exec
servidor: $(BUILD_DIR)/servidor_exec

# -----------------------------------------------------------
# Ligação (Link)
# -----------------------------------------------------------

# Cria o executável do cliente a partir dos seus ficheiros objeto (.o)
$(BUILD_DIR)/cliente_exec: $(CLIENT_OBJS)
	@mkdir -p $(BUILD_DIR)                     # Garante que a pasta Build existe
	$(CC) $(CFLAGS) $^ -o $@                   # Compila e gera o executável final
	@echo "✅ Cliente compilado: $@"           # Mensagem simpática no terminal

# Cria o executável do servidor a partir dos seus ficheiros objeto (.o)
$(BUILD_DIR)/servidor_exec: $(SERVER_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "✅ Servidor compilado: $@"

# -----------------------------------------------------------
# Compilação de cada ficheiro .c em .o
# -----------------------------------------------------------

# Regra genérica:
# Compila qualquer ficheiro .c em .o dentro da estrutura Build/
#   $< → nome do ficheiro fonte (.c)
#   $@ → nome do ficheiro de saída (.o)
#   $(dir $@) → cria a pasta destino se ainda não existir
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compilado: $< -> $@"

# -----------------------------------------------------------
# Limpeza
# -----------------------------------------------------------

.PHONY: clean
clean:
	@echo "🧹 A limpar ficheiros compilados..."
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/$(CLIENT_DIR) $(BUILD_DIR)/$(SERVER_DIR) \
	       $(BUILD_DIR)/cliente_exec $(BUILD_DIR)/servidor_exec
	@echo "Limpeza concluída."
