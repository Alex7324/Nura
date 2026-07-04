#!/bin/sh
# install.sh — compila Nura e lo installa come comando 'nura'.
#
# Uso (dalla cartella del progetto, o da qualsiasi cartella):
#   sh install.sh
#
# Compila l'interprete e copia l'eseguibile in una cartella del PATH, cosi'
# puoi usare 'nura' da qualsiasi parte, come 'gcc'. Rileva il sistema:
#   - Windows (Git Bash):  ~/bin
#   - Linux / Mac:         ~/.local/bin
set -e

# Vai nella cartella dove si trova questo script (la root del progetto).
cd "$(dirname "$0")"

echo "Compilo nura (ottimizzato)..."
gcc -std=c11 -Wall -Wextra -O2 -o nura src/*.c -lm

# Scegli la cartella di installazione in base al sistema operativo.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) BINDIR="$HOME/bin" ;;   # Windows (Git Bash)
    *)                    BINDIR="$HOME/.local/bin" ;;  # Linux / Mac
esac

mkdir -p "$BINDIR"

# gcc su Windows produce 'nura.exe', su Linux/Mac 'nura'.
if [ -f nura.exe ]; then
    cp nura.exe "$BINDIR/nura.exe"
    installed="$BINDIR/nura.exe"
else
    cp nura "$BINDIR/nura"
    installed="$BINDIR/nura"
fi

echo "Installato: $installed"

# Avvisa se la cartella non e' (ancora) nel PATH.
case ":$PATH:" in
    *":$BINDIR:"*)
        echo "OK: '$BINDIR' e' nel PATH. Apri un nuovo terminale e usa:  nura file.nura" ;;
    *)
        echo ""
        echo "ATTENZIONE: '$BINDIR' non e' nel PATH."
        echo "  Aggiungilo (una volta sola) per usare 'nura' da qualsiasi cartella." ;;
esac
