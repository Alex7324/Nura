# debug/ — strumenti per capire come funziona l'interprete

Questi file non fanno parte dell'interprete vero (quello è in `src/`): sono
"lenti d'ingrandimento" che mostrano cosa succede dentro, passo per passo.
Si compilano tutti con un solo comando:

```sh
# dalla cartella principale del progetto:
sh debug/build.sh
```

## 1. `nura_flow` — il narratore del flusso (Fasi 1–3)

Mostra, con l'indentazione che segue la profondità della ricorsione, tutto
quello che accade quando si esegue un'espressione:

- quando il parser entra in una funzione della grammatica,
- quando chiede un token al lexer (si vede che lexer e parser sono *intrecciati*),
- quando crea un nodo dell'albero,
- come l'evaluator percorre l'albero (post-order),
- come l'albero viene liberato alla fine.

```sh
./nura_flow "1 + 2 * 3"
./nura_flow "(1 + 2) * 3"
./nura_flow "1 == 2 + 3"
```

Sono copie "parlanti" di `parser.c`, `eval.c` e `main.c`: `flow_parser.c`,
`flow_eval.c`, `flow_main.c`.

## 2. `nura_run` — il narratore dell'esecuzione (Fase 4)

Mostra come viene eseguito un **programma** intero: prima gli alberi delle
istruzioni prodotte dal parser, poi l'esecuzione passo per passo con le
operazioni sull'**ambiente** (la tabella hash) — `env_define` quando si
dichiara una variabile, `env_get` quando la si legge, `env_assign` quando la
si riassegna. Dopo ogni modifica stampa lo stato dell'ambiente, così si vede
la memoria del programma cambiare.

```sh
./nura_run "var n = 5; print n * 2;"
./nura_run "var s = 0; s = s + 10; s = s + 20; print s;"
```

È in `nura_run.c` e usa il parser e l'ambiente veri (`src/parser.c`, `src/env.c`).

## 3. `env_demo` — il visualizzatore dell'ambiente (Fase 4)

Mostra com'è fatta dentro la **tabella hash** che tiene le variabili
(`nome -> valore`): disegna i bucket e rende visibili le **collisioni**
(bucket con più voci, gestite col "chaining", una lista concatenata per bucket).

```sh
./env_demo
```

È in `env_demo.c` e usa direttamente `src/env.c`.

## Nota

Gli strumenti sono fotografie delle fasi già fatte. Quando il linguaggio
crescerà (istruzioni, funzioni), li aggiorneremo se servirà rivedere
l'ordine o le strutture delle nuove parti.
