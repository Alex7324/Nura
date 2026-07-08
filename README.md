# Nura

Un piccolo linguaggio di programmazione e il suo interprete, scritti in C.

Ho iniziato questo progetto per capire davvero cosa succede quando si esegue del
codice: come si passa da un file di testo a qualcosa che il computer calcola. Invece
di leggerlo e basta, ho preferito costruirmelo, un pezzo alla volta.

## Cos'è

Nura è un **interprete tree-walking**: legge il codice, ne costruisce un albero e lo
esegue percorrendolo. In pratica sto costruendo due cose insieme — il linguaggio e il
programma che lo esegue.

## Com'è fatto

L'interprete è una catena di tre trasformazioni:

```
codice  →  [lexer]  →  token  →  [parser]  →  albero (AST)  →  [evaluator]  →  risultato
```

- **Lexer** — spezza il testo in unità elementari (token): numeri, identificatori,
  operatori, parole chiave.
- **Parser** — dai token costruisce un albero di sintassi (AST) che cattura la
  struttura e le precedenze tra le operazioni.
- **Evaluator** — percorre l'albero in modo ricorsivo e ne calcola il valore.

## A che punto sono

Nura esegue programmi fatti di istruzioni, con variabili e stampa:

```sh
# compilare
make                                    # oppure: mingw32-make (Windows/MinGW)
                                        # oppure: gcc -std=c11 -Wall -Wextra -o nura src/*.c -lm

# eseguire
./nura                                  # modalita' interattiva (REPL)
./nura examples/demo.nura               # esegue un programma da file
./nura -e "var n = 5; print n * 2;"     # esegue codice inline  ->  10
./nura --tokens "var n = 5;"            # mostra i token (debug)

# testare
make test                               # esegue la suite di test automatici
```

Quattro tipi di valore: **numeri**, **booleani** (`true`/`false`), **stringhe**
(`"..."`, concatenabili con `+`) e **array** (`[1, 2, 3]`, liste indicizzabili con
`arr[i]` e passate **per riferimento**). Gestisce: espressioni con `+ - * / %`, confronti
(`< <= > >= == !=`), operatori logici `&&` e `||` (con corto circuito), meno unario
e il not `!`, parentesi; **variabili** (`var`), **assegnamento** e l'istruzione
**`print`**; **controllo di flusso** con `if`/`else`, `while`, `for` e blocchi `{ }` con
scope locale; **funzioni** (`fun`, `return`) con **ricorsione** e **closures**
(una funzione ricorda le variabili di dove è nata), incluse le **chiamate in coda**
(`recur`) che rendono la ricorsione in coda illimitata. Le variabili vivono in un
ambiente realizzato come **tabella hash**. Segnala sia gli
errori di sintassi (con il numero di riga) sia quelli a runtime (divisione per zero,
variabile non definita, tipi incompatibili).

📖 **La guida completa al linguaggio** (tutti i costrutti, con esempi) è in
[docs/linguaggio.md](docs/linguaggio.md).

Esempio (fattoriale iterativo, in `examples/fattoriale.nura`):

```
var n = 5;
var risultato = 1;
var k = 1;
while (k <= n) {
    risultato = risultato * k;
    k = k + 1;
}
print risultato;   // 120
```

## Installazione (usare `nura` da ovunque)

Di default esegui `./nura file.nura` dalla cartella del progetto. Per usarlo da
qualsiasi cartella come un comando vero (tipo `gcc`), va messo in una cartella del
**PATH**.

**Modo più semplice (Windows con Git Bash, Linux, Mac):**
```sh
sh install.sh          # compila e installa nura in una cartella del PATH
```
Lo script rileva il sistema e copia `nura` in `~/bin` (Windows) o `~/.local/bin`
(Linux/Mac). Poi apri un **nuovo** terminale e usa `nura` da ovunque.

**Linux / Mac (in alternativa):**
```sh
make install                 # copia nura in ~/.local/bin
nura file.nura               # ora funziona da ovunque (se ~/.local/bin e' nel PATH)
```

**Windows:**
1. crea una cartella, es. `C:\Users\<tuo-nome>\bin`;
2. copiaci dentro `nura.exe`;
3. aggiungi quella cartella al PATH utente (Impostazioni → *Variabili d'ambiente*);
4. apri un **nuovo** terminale: ora `nura file.nura` funziona da qualsiasi cartella.

## Il traguardo (raggiunto ✅)

Il programma-obiettivo di Nura — variabili, condizioni e funzioni ricorsive — ora
**gira** (`examples/fattoriale_ricorsivo.nura`):

```
fun fattoriale(k) {
    if (k <= 1) { return 1; }
    return k * fattoriale(k - 1);
}

print fattoriale(5);   // 120
```

## Roadmap

- [x] **Fase 1 — Lexer**: testo → token
- [x] **Fase 2 — Parser + AST**: token → albero, con le precedenze
- [x] **Fase 3 — Evaluator**: percorrere l'albero e calcolare
- [x] **Fase 4 — Variabili e ambiente**: `var`, assegnamento, `print`, tabella hash nome → valore
- [x] **Fase 5 — Controllo di flusso**: `if`/`else`, `while`, blocchi `{ }`, operatore `!`
- [x] **Fase 6 — Funzioni**: definizione, chiamata, `return`, ricorsione, scope locale
- [x] **Fase 7 — Array**: literal `[..]`, indicizzazione `arr[i]` in lettura e scrittura, semantica per riferimento
- [x] **Fase 8 — Ciclo `for`**: `for (init; cond; incr)` (dapprima zucchero sintattico su `while`, poi costrutto a sé per supportare `continue`)
- [x] **Fase 9 — Garbage collector**: mark-and-sweep. Ambienti, array e stringhe sono oggetti gestiti; la memoria dei cicli lunghi viene recuperata anche dentro le funzioni (radici temporanee). Diagnostica con `NURA_GC_LOG` / `NURA_GC_STRESS`
- [x] **Fase 10 — Funzioni native**: funzioni scritte in C ed esposte a Nura (`VAL_NATIVE`): `len`, `push`, `pop`, `str`, `num`, `int`, `rand`, `clock`, `input`. Indicizzazione anche delle stringhe (`s[i]`)
- [x] **Fase 11 — `break` e `continue`**: controllo dei cicli, con `continue` che nel `for` esegue comunque l'incremento
- [x] **Fase 12 — `recur` (tail-call optimization)**: chiamate in coda esplicite. `recur f(x)` riusa il frame corrente (tecnica del *trampolino*) invece di accumularne uno nuovo: la ricorsione in coda — anche mutua — è illimitata e non tocca il tetto di profondità. Il garbage collector radica la chiamata pendente (verificato con `NURA_GC_STRESS`)

## Come l'ho sviluppato

Ho costruito Nura facendomi aiutare da **Claude** (l'assistente AI di Anthropic),
usato come una specie di tutor. Procedo per fasi: mi faccio spiegare i concetti,
scrivo il codice e mi assicuro di capire ogni parte prima di andare avanti — l'idea
è imparare come funziona davvero un interprete, non ottenere il risultato e basta.

## Struttura

```
src/
├── token.h / token.c     tipi di token
├── lexer.h / lexer.c     Fase 1 — testo → token
├── ast.h   / ast.c       Fase 2 — nodi dell'albero (espressioni e istruzioni)
├── parser.h / parser.c   Fase 2 — token → AST
├── value.h / value.c     il tipo Value (numeri, bool, stringhe, funzioni, array)
├── eval.h  / eval.c      Fase 3 — esecuzione dell'albero
├── env.h   / env.c       Fase 4 — ambiente delle variabili (tabella hash)
├── gc.h    / gc.c        Fase 9 — garbage collector (mark-and-sweep)
└── main.c                punto di ingresso: legge file o codice, avvia la catena

debug/                    strumenti didattici per capire il funzionamento
docs/                     guida al linguaggio (docs/linguaggio.md)
tests/                    suite di test automatici (make test)
examples/                 programmi di esempio in Nura (.nura)
Makefile                  compilazione e test (make / make test / make clean)
```

## Licenza

Distribuito con licenza MIT — vedi il file [LICENSE](LICENSE).
