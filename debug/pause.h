#ifndef DEBUG_PAUSE_H
#define DEBUG_PAUSE_H

/*
 * Piccola utilita' condivisa dagli strumenti di debug.
 *
 * Gli strumenti "girano una volta e finiscono": col doppio-clic la finestra
 * si chiuderebbe subito. Questa funzione, alla fine del programma, aspetta un
 * Invio prima di chiudere -- ma SOLO quando ha senso:
 *
 *   - se il programma e' stato avviato SENZA argomenti (tipico del doppio-clic)
 *   - e l'output va davvero a un terminale/console (non a un file o a una pipe)
 *
 * Cosi' il doppio-clic tiene la finestra aperta, mentre l'uso in pipe o
 * redirezione (es. ./tool | grep ...) NON si blocca. Funziona su Windows,
 * Linux e Mac grazie al piccolo blocco condizionale qui sotto.
 */

#include <stdio.h>

#ifdef _WIN32
  #include <io.h>
  #define DBG_ISATTY(fd) _isatty(fd)
  #define DBG_FILENO(f)  _fileno(f)
#else
  #include <unistd.h>
  #define DBG_ISATTY(fd) isatty(fd)
  #define DBG_FILENO(f)  fileno(f)
#endif

static void wait_before_closing(int argc) {
    if (argc <= 1 && DBG_ISATTY(DBG_FILENO(stdout))) {
        printf("\n(premi Invio per chiudere)\n");
        getchar();
    }
}

#endif /* DEBUG_PAUSE_H */
