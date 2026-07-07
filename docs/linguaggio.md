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

Nura ha cinque tipi di valore.

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
Dentro una stringa puoi usare le **sequenze di escape**: `\n` (a-capo), `\t`
(tabulazione), `\"` (virgoletta), `\\` (backslash), `\r` (ritorno carrello).
```
print "riga 1\nriga 2";        // due righe
print "dice \"ciao\"";         // dice "ciao"
print "percorso: c:\\dati";    // percorso: c:\dati
```
Una sequenza di escape sconosciuta (es. `\q`) è un errore di sintassi.
Puoi leggere un **carattere** per indice, come per gli array (parte da `0`); il
risultato è una stringa di un carattere. La lunghezza si ottiene con `len`.
```
var s = "ciao";
print s[0];        // c
print len(s);      // 4
```
Le stringhe sono **immutabili**: `s[0] = "x"` è un errore (non si modificano sul posto).

### Funzioni
Anche le funzioni sono valori: puoi metterle in una variabile e restituirle
(vedi [Funzioni](#10-funzioni) e [Closures](#11-closures)).

### Array
Una lista ordinata di valori, racchiusa tra parentesi quadre. Gli elementi
possono essere di **tipi diversi** e anche altri array (annidati).
```
var a = [1, 2, 3];
print a;                 // [1, 2, 3]
print [1, "ciao", true]; // [1, "ciao", true]
print [];                // []  (array vuoto)
```

**Accesso per indice** con `arr[i]`. Gli indici partono da `0`:
```
var a = [10, 20, 30];
print a[0];              // 10
print a[2];              // 30
a[1] = 99;               // scrittura: ora a è [10, 99, 30]
print a[1] + a[2];       // 129
```

L'indice dev'essere un **numero intero** (`a[2]` o l'equivalente `a[2.0]`); un
indice con parte decimale come `a[2.9]` è un errore, non viene troncato. Un indice
**fuori dai limiti** (negativo o ≥ lunghezza) è un errore a runtime, non un crash:
```
var a = [1, 2];
print a[5];              // Errore a runtime: indice 5 fuori dai limiti...
print a[0.5];            // Errore a runtime: l'indice ... dev'essere un numero intero
```

**Per riferimento** — questa è la parte importante. Assegnare un array a
un'altra variabile (o passarlo a una funzione) **non lo copia**: le due
variabili condividono lo *stesso* array. Modificarlo da una si vede dall'altra.
```
var a = [1, 2, 3];
var b = a;               // a e b sono lo STESSO array
b[0] = 99;
print a;                 // [99, 2, 3]  <- cambiato anche a!
```
Di conseguenza `==` fra array confronta l'**identità** (sono lo stesso array?),
non il contenuto:
```
var a = [1, 2];
print a == a;            // true
print [1, 2] == [1, 2];  // false  (stesso contenuto, ma array diversi)
```

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
| più forte | `()` chiamata di funzione, `[]` indice di array |
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
- **tutto il resto** è vero (qualsiasi altro numero, qualsiasi stringa, qualsiasi funzione, qualsiasi array — anche vuoto).

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

### for
Il classico ciclo con tre clausole: `for (inizializzazione; condizione; incremento) corpo`.
```
for (var i = 0; i < 5; i = i + 1) {
    print i;                 // stampa 0 1 2 3 4
}
```
- La **variabile** dichiarata nell'inizializzazione vive solo dentro il ciclo
  (fuori non esiste più).
- Tutte e tre le clausole sono **facoltative**: `for (;;) { ... }` è un ciclo
  infinito, `for (; i < n;)` usa solo la condizione, ecc.

Comodo per scorrere un array (con `len`, vedi sotto):
```
var a = [10, 20, 30];
for (var i = 0; i < len(a); i = i + 1) {
    print a[i];
}
```

### break e continue
Dentro un ciclo (`while` o `for`):
- **`break`** esce subito dal ciclo;
- **`continue`** salta al giro successivo. In un `for`, `continue` esegue comunque
  l'**incremento** prima di ricominciare.

```
for (var i = 0; i < 10; i = i + 1) {
    if (i == 5) break;        // si ferma: stampa 0 1 2 3 4
    if (i % 2 == 0) continue; // salta i pari
    print i;                  // stampa 1 3 (poi break a 5)
}
```
Usare `break` o `continue` **fuori** da un ciclo è un errore di sintassi.

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

> ⚠️ **Limite di profondità.** Per evitare crash, la ricorsione — e le espressioni
> molto profonde — hanno un tetto: oltre un certo livello di annidamento Nura
> interrompe con un errore a runtime ("profondità massima superata") invece di
> bloccarsi con uno *stack overflow*.

### Funzioni native
Alcune funzioni sono **già pronte**: sono scritte in C dentro l'interprete (si
chiamano *native*), ma si usano come qualsiasi altra funzione.

| Nativa | Cosa fa |
|---|---|
| `len(x)` | lunghezza di un array o di una stringa |
| `push(arr, x)` | aggiunge `x` in coda all'array (che *cresce*); ritorna la nuova lunghezza |
| `pop(arr)` | toglie e restituisce l'ultimo elemento |
| `str(x)` | converte un valore in stringa (utile per concatenare: `"n=" + str(3)`) |
| `num(s)` | converte una stringa in numero |
| `int(x)` | parte intera di un numero, verso lo zero (`int(2.9)` = 2). Comoda per gli indici |
| `rand()` | numero pseudocasuale in `[0, 1)`. Per un intero in `[0, n)`: `int(rand() * n)` |
| `clock()` | secondi di CPU dall'avvio (per misurare) |
| `input()` | legge una riga da tastiera come stringa |

`push` modifica l'array sul posto (per riferimento). Un dado da 1 a 6:
`int(rand() * 6) + 1`.

```
var a = [];
for (var i = 0; i < 5; i = i + 1) {
    push(a, i * i);          // costruisco la lista un pezzo alla volta
}
print a;                     // [0, 1, 4, 9, 16]
print len(a);                // 5

// scorrere un array senza sapere la lunghezza a memoria:
var somma = 0;
for (var i = 0; i < len(a); i = i + 1) {
    somma = somma + a[i];
}
print somma;                 // 30

print len("ciao");           // 4
```

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

istruzione  -> varDecl | funDecl | print | if | while | for | break | continue | return | blocco | exprStmt
break       -> "break" ";"
continue    -> "continue" ";"
varDecl     -> "var" IDENT "=" espressione ";"
funDecl     -> "fun" IDENT "(" parametri? ")" blocco
parametri   -> IDENT ( "," IDENT )*
print       -> "print" espressione ";"
if          -> "if" "(" espressione ")" istruzione ( "else" istruzione )?
while       -> "while" "(" espressione ")" istruzione
for         -> "for" "(" ( varDecl | exprStmt | ";" ) espressione? ";" espressione? ")" istruzione
return      -> "return" espressione? ";"
blocco      -> "{" istruzione* "}"
exprStmt    -> espressione ";"

espressione -> assegnamento
assegnamento-> ( IDENT | chiamata "[" espressione "]" ) "=" assegnamento | logic_or
logic_or    -> logic_and ( "||" logic_and )*
logic_and   -> uguaglianza ( "&&" uguaglianza )*
uguaglianza -> confronto ( ( "==" | "!=" ) confronto )*
confronto   -> somma ( ( "<" | "<=" | ">" | ">=" ) somma )*
somma       -> prodotto ( ( "+" | "-" ) prodotto )*
prodotto    -> unario ( ( "*" | "/" | "%" ) unario )*
unario      -> ( "-" | "!" ) unario | chiamata
chiamata    -> primario ( "(" argomenti? ")" | "[" espressione "]" )*
argomenti   -> espressione ( "," espressione )*
array       -> "[" ( espressione ( "," espressione )* )? "]"
primario    -> NUMERO | STRINGA | "true" | "false" | IDENT | "(" espressione ")" | array
```

---

*Guida aggiornata alla versione corrente di Nura. Man mano che il linguaggio cresce,
questa pagina cresce con lui.*
