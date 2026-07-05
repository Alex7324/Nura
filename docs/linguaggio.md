# Il linguaggio Nura — Guida di riferimento

Questa è la guida completa al linguaggio **Nura**: come scrivere programmi, tutti i
costrutti e le regole. Per informazioni sul *progetto* (com'è fatto l'interprete),
vedi il [README](../README.md).

## Indice

1. [Un primo programma](#1-un-primo-programma)
2. [Come eseguire](#2-come-eseguire)
3. [Sintassi di base](#3-sintassi-di-base)
4. [Tipi di valore](#4-tipi-di-valore)
5. [Variabili](#5-variabili)
6. [Operatori](#6-operatori)
7. [Verità (vero/falso)](#7-verità-verofalso)
8. [Stampare: `print`](#8-stampare-print)
9. [Controllo di flusso](#9-controllo-di-flusso)
10. [Funzioni](#10-funzioni)
11. [Closures](#11-closures)
12. [Errori](#12-errori)
13. [Appendice: grammatica](#13-appendice-grammatica)

---

## 1. Un primo programma

```
print "Ciao, mondo!";
```

Salvalo in un file `ciao.nura` ed eseguilo:

```sh
nura ciao.nura
```

Output:
```
Ciao, mondo!
```

## 2. Come eseguire

Ci sono tre modi:

```sh
nura programma.nura        # esegue un file
nura -e "print 1 + 2;"     # esegue codice scritto direttamente
nura                       # modalità interattiva (REPL): scrivi ed esegui riga per riga
```

Nel REPL le variabili restano vive tra una riga e l'altra; si esce con `exit`.

## 3. Sintassi di base

- Ogni **istruzione** termina con un punto e virgola `;`.
- I **commenti** iniziano con `//` e vanno fino a fine riga.
- Le istruzioni si eseguono in ordine, dall'alto verso il basso.
- I **blocchi** raggruppano più istruzioni tra graffe `{ }`.

```
// questo è un commento
var x = 10;      // dichiarazione
print x;         // stampa
{
    var y = 5;   // y esiste solo dentro questo blocco
    print y;
}
```

## 4. Tipi di valore

Nura ha quattro tipi di valore.

### Numeri
Interi o decimali; internamente sono tutti numeri in virgola mobile.
```
print 42;        // 42
print 3.14;      // 3.14
print 10 / 4;    // 2.5
```

### Booleani
Due valori: `true` e `false`.
```
print true;      // true
print 3 > 5;     // false
```

### Stringhe
Testo tra virgolette doppie. Si concatenano con `+`.
```
print "ciao";
print "Hello, " + "world!";   // Hello, world!
```

### Funzioni
Anche le funzioni sono valori: puoi metterle in una variabile e restituirle
(vedi [Funzioni](#10-funzioni) e [Closures](#11-closures)).

## 5. Variabili

Si dichiarano con `var` e si riassegnano con `=`.

```
var n = 5;       // dichiarazione (obbligatorio un valore iniziale)
n = n + 1;       // riassegnamento
print n;         // 6
```

- Assegnare a una variabile **non dichiarata** è un errore.
- L'assegnamento è anche un'espressione: `a = b = 5;` assegna `5` a entrambi.

### Scope (dove "vive" una variabile)

Una variabile è visibile nel blocco in cui è dichiarata e in quelli interni:

```
var globale = 1;
{
    var locale = 2;
    print globale;   // 1  (si vede da dentro)
    print locale;    // 2
}
print locale;        // ERRORE: 'locale' non esiste più qui fuori
```

## 6. Operatori

### Aritmetici
`+` `-` `*` `/` `%` (modulo, il resto della divisione).
```
print 2 + 3 * 4;     // 14  (il * ha precedenza sul +)
print 17 % 5;        // 2
print -5;            // meno unario
```

Il `+` funziona sui numeri (somma) e sulle stringhe (concatenazione), ma **non**
mescolati: `1 + "a"` è un errore.

### Confronto
`<` `<=` `>` `>=` `==` `!=` — danno un booleano.
```
print 10 > 3;        // true
print 2 == 2;        // true
print "a" == "a";    // true
```
Nota: `==` confronta anche il **tipo**, quindi `true == 1` è `false` (booleano ≠ numero).

### Logici
`&&` (e), `||` (o), `!` (non). Danno un booleano e usano il **corto circuito**:
`&&` non valuta il secondo operando se il primo è falso; `||` non lo valuta se il
primo è vero.
```
print true && false;         // false
print false || true;         // true
print !true;                 // false
print 0 && (1 / 0);          // false  (il /0 non viene mai eseguito!)
```

### Parentesi
Cambiano l'ordine di valutazione: `(1 + 2) * 3` fa `9`.

### Precedenza (dal più forte al più debole)

| Livello | Operatori |
|---|---|
| più forte | `()` chiamata di funzione |
| | `-` `!` (unari) |
| | `*` `/` `%` |
| | `+` `-` |
| | `<` `<=` `>` `>=` |
| | `==` `!=` |
| | `&&` |
| | `\|\|` |
| più debole | `=` (assegnamento) |

## 7. Verità (vero/falso)

`if`, `while`, `!`, `&&`, `||` decidono in base alla "verità" di un valore:

- `false` e il numero `0` sono **falsi**;
- **tutto il resto** è vero (qualsiasi altro numero, qualsiasi stringa, qualsiasi funzione).

```
if (0)    print "no";      // non stampa
if (42)   print "si";      // stampa: si
if ("x")  print "anche";   // stampa: anche  (le stringhe sono vere)
```

## 8. Stampare: `print`

`print` valuta un'espressione e ne stampa il valore, seguito da un a-capo.
```
print 3 * 3;                 // 9
print "totale: " + "ok";     // totale: ok
print 5 > 2;                 // true
```

## 9. Controllo di flusso

### if / else
```
if (voto >= 60) {
    print "promosso";
} else {
    print "bocciato";
}
```
Le graffe sono facoltative se il ramo è una sola istruzione:
```
if (x > 0) print "positivo"; else print "non positivo";
```
Concatenabili in `else if`:
```
if (n > 0)      print "positivo";
else if (n < 0) print "negativo";
else            print "zero";
```

### while
Ripete finché la condizione resta vera.
```
var i = 1;
while (i <= 5) {
    print i;
    i = i + 1;
}
```

## 10. Funzioni

Si definiscono con `fun`, si chiamano con `nome(argomenti)`, e restituiscono un
valore con `return`.

```
fun somma(a, b) {
    return a + b;
}
print somma(3, 4);           // 7
```

- I **parametri** sono variabili locali alla funzione.
- `return` termina subito la funzione col valore dato.
- Una funzione **senza** `return` restituisce `0`.
- Chiamare qualcosa che non è una funzione, o con il numero sbagliato di
  argomenti, è un errore a runtime.

### Ricorsione
Una funzione può richiamare sé stessa:
```
fun fattoriale(k) {
    if (k <= 1) { return 1; }
    return k * fattoriale(k - 1);
}
print fattoriale(5);         // 120
```

> ⚠️ **Limite di profondità.** Per evitare crash, la ricorsione ha un tetto: oltre
> circa **1000 chiamate annidate**, Nura interrompe con un errore a runtime
> ("profondità di ricorsione massima superata") invece di bloccarsi.

## 11. Closures

Una funzione **ricorda** le variabili del posto dove è stata definita, anche dopo
che quel posto ha finito di eseguire. Questo permette, per esempio, di creare
"contatori" con uno stato privato:

```
fun contatore() {
    var n = 0;
    fun conta() {
        n = n + 1;
        return n;
    }
    return conta;            // restituisce la funzione interna
}

var c = contatore();
print c();                   // 1
print c();                   // 2

var d = contatore();
print d();                   // 1   (contatore indipendente, con la sua n)
```

## 12. Errori

Nura distingue due tipi di errore, entrambi stampati con il numero di riga quando
possibile.

- **Errori di sintassi**: il programma è scritto male e non può nemmeno partire
  (es. manca un `;`, una parentesi non è chiusa).
  ```
  print 1 +;   →  [riga 1] Errore di sintassi ...
  ```
- **Errori a runtime**: il programma è scritto bene ma fa qualcosa di illegale
  mentre gira (divisione per zero, variabile non definita, tipi incompatibili).
  ```
  print 1 / 0;   →  Errore a runtime: divisione per zero.
  ```

## 13. Appendice: grammatica

Grammatica di Nura in forma sintetica (`*` = zero o più, `?` = facoltativo).

```
programma   -> istruzione* EOF

istruzione  -> varDecl | funDecl | print | if | while | return | blocco | exprStmt
varDecl     -> "var" IDENT "=" espressione ";"
funDecl     -> "fun" IDENT "(" parametri? ")" blocco
parametri   -> IDENT ( "," IDENT )*
print       -> "print" espressione ";"
if          -> "if" "(" espressione ")" istruzione ( "else" istruzione )?
while       -> "while" "(" espressione ")" istruzione
return      -> "return" espressione? ";"
blocco      -> "{" istruzione* "}"
exprStmt    -> espressione ";"

espressione -> assegnamento
assegnamento-> IDENT "=" assegnamento | logic_or
logic_or    -> logic_and ( "||" logic_and )*
logic_and   -> uguaglianza ( "&&" uguaglianza )*
uguaglianza -> confronto ( ( "==" | "!=" ) confronto )*
confronto   -> somma ( ( "<" | "<=" | ">" | ">=" ) somma )*
somma       -> prodotto ( ( "+" | "-" ) prodotto )*
prodotto    -> unario ( ( "*" | "/" | "%" ) unario )*
unario      -> ( "-" | "!" ) unario | chiamata
chiamata    -> primario ( "(" argomenti? ")" )*
argomenti   -> espressione ( "," espressione )*
primario    -> NUMERO | STRINGA | "true" | "false" | IDENT | "(" espressione ")"
```

---

*Guida aggiornata alla versione corrente di Nura. Man mano che il linguaggio cresce,
questa pagina cresce con lui.*
