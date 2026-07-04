# Makefile di Nura
#
# Uso (Linux/Mac):        make        make test        make clean
# Uso (Windows/MinGW):    mingw32-make   mingw32-make test   ...
#
# Nota: 'test' e 'debug' usano 'sh', quindi su Windows vanno lanciati
# da Git Bash (dove 'sh' e 'rm' sono disponibili).

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra
LDLIBS  = -lm            # libreria matematica (fmod) — richiesta su Linux/Mac
SRC     = $(wildcard src/*.c)
BIN     = nura

# Compila l'interprete (bersaglio di default)
$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDLIBS)

# Compila (se serve) ed esegue la suite di test
test: $(BIN)
	sh tests/run_tests.sh

# Compila gli strumenti di debug (nura_flow, nura_run, env_demo)
debug:
	sh debug/build.sh

# Rimuove i file compilati
clean:
	rm -f $(BIN) nura_flow nura_run env_demo

.PHONY: test debug clean
