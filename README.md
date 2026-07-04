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

Per ora funziona come una calcolatrice che rispetta le precedenze. Costruisce l'albero
di un'espressione e lo valuta:

```sh
gcc -Wall -Wextra -o nura src/*.c

./nura "1 + 2 * 3"      # stampa l'albero e il risultato: 7
./nura --tokens "1+2"   # stampa solo il flusso di token
```

Gestisce `+ - * /`, i confronti (`< <= > >= == !=`), il meno unario, le parentesi,
e segnala sia gli errori di sintassi sia quelli a runtime (es. divisione per zero).

## Dove voglio arrivare

L'obiettivo è arrivare a eseguire un programma come questo:

```
var n = 5;

fun fattoriale(k) {
    if (k <= 1) { return 1; }
    return k * fattoriale(k - 1);
}

print fattoriale(n);   // 120
```

Cioè: variabili, condizioni, cicli, e funzioni che possono richiamare sé stesse.

## Roadmap

- [x] **Fase 1 — Lexer**: testo → token
- [x] **Fase 2 — Parser + AST**: token → albero, con le precedenze
- [x] **Fase 3 — Evaluator**: percorrere l'albero e calcolare
- [ ] **Fase 4 — Variabili e ambiente**: `var`, assegnamento, tabella hash nome → valore
- [ ] **Fase 5 — Istruzioni e controllo di flusso**: `print`, `if`/`else`, `while`, blocchi
- [ ] **Fase 6 — Funzioni**: definizione, chiamata, `return`, ricorsione

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
├── ast.h   / ast.c       Fase 2 — nodi dell'albero
├── parser.h / parser.c   Fase 2 — token → AST
├── eval.h  / eval.c      Fase 3 — esecuzione dell'albero
└── main.c                mette insieme la catena
```
