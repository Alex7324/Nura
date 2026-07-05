#!/bin/sh
# Suite di test di Nura.
# Esegue tanti programmini e confronta l'output con quello atteso.
# Uso:  sh tests/run_tests.sh     (oppure:  make test)

NURA=./nura
pass=0
fail=0

# check "descrizione" "codice" "output atteso"
# Cattura anche stderr (2>&1) per testare i messaggi d'errore.
check() {
    # tr -d '\r' rimuove i ritorni a capo di Windows (\r) per confrontare uguale
    # su Windows e Linux.
    got=$("$NURA" -e "$2" 2>&1 | tr -d '\r')
    if [ "$got" = "$3" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FALLITO: $1"
        echo "   codice:   $2"
        echo "   atteso:   [$3]"
        echo "   ottenuto: [$got]"
    fi
}

echo "== Fase 1-3: espressioni =="
check "somma"                "print 1 + 2;"              "3"
check "precedenza * su +"    "print 1 + 2 * 3;"          "7"
check "parentesi"            "print (1 + 2) * 3;"        "9"
check "assoc. sinistra -"    "print 10 - 4 - 3;"         "3"
check "divisione decimale"   "print 100 / 8;"            "12.5"
check "modulo"               "print 17 % 5;"             "2"
check "modulo e precedenza"  "print 10 + 20 % 7;"        "16"
check "meno unario"          "print -5 + 3;"             "-2"
check "doppio meno unario"   "print --5;"               "5"
check "decimali"             "print 3.14 * 2;"           "6.28"
check "intero grande esteso" "print 1000000;"            "1000000"
check "intero 7 cifre esatto" "print 1234567;"           "1234567"
check "intero 11 cifre"      "print 12345678901;"        "12345678901"
check "decimale resta g"     "print 1 / 3;"              "0.333333"

echo "== Confronti (danno booleani) =="
check "maggiore"             "print 10 > 3;"             "true"
check "minore-uguale"        "print 4 <= 4;"            "true"
check "diverso falso"        "print 5 != 5;"             "false"
check "uguaglianza"          "print 2 + 2 == 4;"         "true"

echo "== Fase 4: variabili =="
check "var e uso"            "var n = 5; print n * 2;"                        "10"
check "due variabili"        "var a = 3; var b = 4; print a * b;"             "12"
check "riassegnamento"       "var x = 10; x = x + 5; print x;"                "15"
check "accumulatore"         "var s = 0; s = s + 10; s = s + 20; print s;"    "30"
check "assegn. concatenato"  "var a=0; var b=0; a = b = 5; print a; print b;" "5
5"
check "ridefinizione"        "var n = 5; var n = 99; print n;"                "99"
check "piu' print"           "var n = 4; print n; print n + 1;"               "4
5"

echo "== Fase 5: controllo di flusso =="
check "if vero"              "if (1) print 10;"                           "10"
check "if falso senza else"  "if (0) print 10;"                           ""
check "if-else ramo vero"    "var x=5; if (x > 3) print 1; else print 2;" "1"
check "if-else ramo falso"   "var x=1; if (x > 3) print 1; else print 2;" "2"
check "not vero"             "print !0;"                                  "true"
check "not falso"            "print !5;"                                  "false"
check "blocco"               "{ var a = 7; print a; }"                    "7"
check "while conta"          "var i=1; while (i<=3) { print i; i=i+1; }"  "1
2
3"
check "fattoriale (while)"   "var n=5; var f=1; var k=1; while (k<=n) { f=f*k; k=k+1; } print f;" "120"

echo "== Operatori logici && || =="
check "and vero"             "print 1 && 1;"                              "true"
check "and falso"            "print 1 && 0;"                              "false"
check "or vero"              "print 0 || 1;"                              "true"
check "corto circuito &&"    "print 0 && (1 / 0);"                        "false"
check "corto circuito ||"    "print 1 || (1 / 0);"                        "true"
check "logici in un if"      "var e = 20; if (e >= 18 && e < 100) print 1;" "1"

echo "== Booleani e stringhe =="
check "true"                 "print true;"                               "true"
check "false"                "print false;"                              "false"
check "stringa"              'print "ciao";'                             "ciao"
check "concatenazione"       'print "Hello, " + "world!";'               "Hello, world!"
check "stringa in variabile" 'var s = "Nura"; print "Ciao " + s;'        "Ciao Nura"
check "confronto stringhe"   'print "a" == "a";'                         "true"
check "tipi diversi diversi" "print true == 1;"                          "false"
check "stringa e' vera"      'if ("x") print 1;'                         "1"

echo "== Fase 6: funzioni =="
check "funzione semplice"    "fun somma(a, b) { return a + b; } print somma(3, 4);"                       "7"
check "fattoriale ricorsivo" "fun f(k) { if (k <= 1) { return 1; } return k * f(k - 1); } print f(5);"    "120"
check "fibonacci"            "fun fib(n) { if (n < 2) { return n; } return fib(n-1) + fib(n-2); } print fib(10);" "55"
check "funzione con stringa" 'fun s(n) { return "Ciao " + n; } print s("Nura");'                          "Ciao Nura"
check "senza return da' 0"   "fun f() { } print f();"                                                     "0"
check "scope: param locale"  "fun f(x) { return x; } var x = 99; print f(1); print x;"                    "1
99"
check "scope blocco locale"  "{ var x = 5; } print x;"     "Errore a runtime: variabile 'x' non definita."
check "argomenti errati"     "fun f(a) { return a; } print f(1, 2);"  "Errore a runtime: la funzione 'f' vuole 1 argomenti, ne hai passati 2."
check "chiamo un numero"     "print 5(3);"                 "Errore a runtime: si possono chiamare solo le funzioni."

echo "== Closures =="
check "closure ricorda"      "fun c() { var n = 0; fun f() { n = n + 1; return n; } return f; } var a = c(); print a(); print a(); print a();" "1
2
3"
check "closure indipendenti" "fun c() { var n = 0; fun f() { n = n + 1; return n; } return f; } var a = c(); var b = c(); a(); print a(); print b();" "2
1"

echo "== Fase 7: array =="
check "literal e stampa"     "print [1, 2, 3];"                              "[1, 2, 3]"
check "array vuoto"          "print [];"                                     "[]"
check "accesso per indice"   "var a = [10, 20, 30]; print a[1];"             "20"
check "indice calcolato"     "var a = [10, 20, 30]; var i = 2; print a[i];"  "30"
check "scrittura per indice" "var a = [1, 2, 3]; a[0] = 99; print a;"        "[99, 2, 3]"
check "array in espressione" "var a = [5, 7]; print a[0] + a[1];"            "12"
check "tipi misti"           'print [1, "x", true];'                         "[1, \"x\", true]"
check "array annidati"       "var m = [[1, 2], [3, 4]]; print m[1][0];"      "3"
check "per riferimento"      "var a = [1, 2]; var b = a; b[0] = 9; print a;" "[9, 2]"
check "uguaglianza identita'" "var a = [1]; var b = a; print a == b;"        "true"
check "contenuto != identita'" "print [1, 2] == [1, 2];"                     "false"
check "array da funzione"    "fun c(x, y) { return [x, y]; } print c(3, 4);" "[3, 4]"
check "somma su array"       "var a=[1,2,3]; var s=0; var i=0; while(i<3){ s=s+a[i]; i=i+1; } print s;" "6"
check "indice fuori limiti"  "var a=[1,2]; print a[5];"    "Errore a runtime: indice 5 fuori dai limiti: l'array ha 2 elementi."
check "un solo elemento"     "var a=[7]; print a[9];"      "Errore a runtime: indice 9 fuori dai limiti: l'array ha 1 elemento."
check "indice negativo"      "var a=[1,2]; print a[-1];"   "Errore a runtime: indice -1 fuori dai limiti: l'array ha 2 elementi."
check "indicizzo un numero"  "var n=3; print n[0];"        "Errore a runtime: si puo' indicizzare solo un array, non un numero."
check "indice non numerico"  'var a=[1]; print a["x"];'    "Errore a runtime: l'indice di un array deve essere un numero, non un stringa."

echo "== Fase 7b: robustezza array (casi limite / anti-crash) =="
check "indice deve essere intero" "var a=[1,2,3]; print a[2.9];"  "Errore a runtime: l'indice di un array deve essere un numero intero, non 2.9."
check "indice intero come 2.0 ok"  "var a=[1,2,3]; print a[2.0];" "3"
check "indice enorme non e' UB"    "var a=[1]; print a[1000000000];" "Errore a runtime: indice 1000000000 fuori dai limiti: l'array ha 1 elemento."
# Array ciclico (a contiene se stesso): la stampa NON deve andare in loop.
# Attesi 100 '[' + '[...]' + 100 ']'  (MAX_PRINT_DEPTH = 100).
CICLICO="$(printf '[%.0s' $(seq 1 100))[...]$(printf ']%.0s' $(seq 1 100))"
check "array ciclico non crasha"   "var a=[1]; a[0]=a; print a;" "$CICLICO"
# Annidamento sintattico eccessivo: errore controllato, niente stack overflow.
TROPPI="$(printf '[%.0s' $(seq 1 1200))"
check "annidamento eccessivo"      "print ${TROPPI}1;" "[riga 1] Errore di sintassi vicino a '[': espressione troppo annidata."
# Catena binaria lunghissima: parser iterativo, ma albero profondo. Deve dare
# errore controllato, NON crashare (ne' in eval ne' in ast_free).
CATENA="$(python -c "print('+'.join(['1']*3000))" 2>/dev/null || printf '1%.0s+1' $(seq 1 1500))"
check "catena binaria lunga"       "print ${CATENA};" "[riga 1] Errore di sintassi vicino a '1': espressione troppo annidata."
check "catena corta ok"            "print 1+1+1+1+1+1+1+1;" "8"

echo "== Fase 8: ciclo for (zucchero sintattico -> while) =="
check "for base 0..2"        "for (var i=0; i<3; i=i+1) print i;"   "0
1
2"
check "for con blocco"       "var s=0; for (var i=1; i<=4; i=i+1) { s=s+i; } print s;" "10"
check "for somma su array"   "var a=[10,20,30]; var s=0; for (var i=0; i<3; i=i+1) s=s+a[i]; print s;" "60"
check "for: var non esce"    "for (var k=0; k<2; k=k+1) print k; print k;" "0
1
Errore a runtime: variabile 'k' non definita."
check "for init esterno"     "var i=0; for (i=0; i<3; i=i+1) {} print i;" "3"
check "for solo condizione"  "var i=0; for (; i<2;) { print i; i=i+1; }" "0
1"
check "for annidati"         "for (var r=0; r<2; r=r+1) for (var c=0; c<2; c=c+1) print r*10+c;" "0
1
10
11"

echo "== Fase 11: break e continue =="
check "break nel for"        "for(var i=0;i<10;i=i+1){ if(i==3) break; print i; }" "0
1
2"
check "continue nel for"     "for(var i=0;i<5;i=i+1){ if(i==2) continue; print i; }" "0
1
3
4"
check "break nel while"      "var i=0; while(true){ if(i==3) break; print i; i=i+1; }" "0
1
2"
check "continue nel while"   "var i=0; while(i<5){ i=i+1; if(i==3) continue; print i; }" "1
2
4
5"
check "for annidati + break interno" "for(var r=0;r<3;r=r+1){ for(var c=0;c<3;c=c+1){ if(c==1) break; print r*10+c; } }" "0
10
20"
check "break fuori da ciclo"  "break;"    "[riga 1] Errore di sintassi vicino a 'break': 'break' fuori da un ciclo."
check "continue fuori da ciclo" "continue;" "[riga 1] Errore di sintassi vicino a 'continue': 'continue' fuori da un ciclo."
check "break non attraversa funzione" "fun f(){ break; } print 1;" "[riga 1] Errore di sintassi vicino a 'break': 'break' fuori da un ciclo."

echo "== Garbage collector (Fase 9) =="
# Cicli lunghi che creano tanta spazzatura: il GC la raccoglie, il risultato
# resta corretto e non si esaurisce la memoria (prima crashava con 'Memoria esaurita').
check "GC: ciclo con garbage"  "var s=0; for(var i=0;i<200000;i=i+1){ var t=[i]; s=s+1; } print s;" "200000"
# Ci che e' RAGGIUNGIBILE non deve essere liberato per errore (no use-after-free):
check "GC: array vivo sopravvive" "var a=[5,6]; for(var i=0;i<200000;i=i+1){ var t=[i]; } print a[1];" "6"
check "GC: closure sopravvive"  "fun mk(){ var n=0; fun i(){ n=n+1; return n; } return i; } var c=mk(); for(var k=0;k<200000;k=k+1){ c(); } print c();" "200001"
check "GC: stringa viva sopravvive" 'var s="Nura"; for(var i=0;i<200000;i=i+1){ var t=[i]; } print s;' "Nura"
# Stringhe ora sotto GC: un ciclo di sole stringhe non leaka piu' e la stringa
# viva sopravvive alla raccolta della spazzatura testuale.
check "GC: ciclo di sole stringhe" 'var s="ok"; for(var i=0;i<200000;i=i+1){ var t="a"+"b"; } print s+"!";' "ok!"
# Il GC gira anche DENTRO le chiamate: un ciclo lungo in una funzione non OOM.
check "GC: ciclo dentro funzione" "fun run(){ var s=0; for(var i=0;i<200000;i=i+1){ var a=[i]; s=s+1; } return s; } print run();" "200000"
# Radici temporanee: oggetti vivi solo in variabili C attraverso una chiamata.
check "GC: array literal span chiamate" "fun mk(x){return [x,x];} print [mk(1),mk(2)];" "[[1, 1], [2, 2]]"
check "GC: arg che alloca tra gli arg" "fun s3(a,b,c){return a+b+c;} fun m(){return [1];} print s3(m()[0],m()[0],m()[0]);" "3"

echo "== Fase 10: funzioni native (len, push) =="
check "len di array"         "print len([10,20,30]);"                 "3"
check "len di stringa"       'print len("ciao");'                     "4"
check "len di array vuoto"   "print len([]);"                         "0"
check "push muta e ritorna len" "var a=[1,2]; print push(a,3); print a;" "3
[1, 2, 3]"
check "costruire con push"   "var a=[]; for(var i=0;i<4;i=i+1){ push(a,i*i); } print a;" "[0, 1, 4, 9]"
check "scorrere con len"     "var a=[5,7,9]; var s=0; for(var i=0;i<len(a);i=i+1){ s=s+a[i]; } print s;" "21"
check "len arieta' errata"   "print len(1,2);"      "Errore a runtime: la funzione 'len' vuole 1 argomenti, ne hai passati 2."
check "len tipo errato"      "print len(42);"       "Errore a runtime: len() vuole un array o una stringa."
check "push tipo errato"     "push(5, 1);"          "Errore a runtime: push() vuole un array come primo argomento."
check "pop toglie l'ultimo"  "var a=[1,2,3]; print pop(a); print a;" "3
[1, 2]"
check "pop su array vuoto"   "pop([]);"             "Errore a runtime: pop() su un array vuoto."
check "str di numero"        'print "n=" + str(42);'                 "n=42"
check "str di decimale"      "print str(3.14);"                      "3.14"
check "str di bool"          "print str(true);"                      "true"
check "str di array annidato" 'print str([1,"x",[2,3]]);'            '[1, "x", [2, 3]]'
check "num da stringa"       'print num("42") + 1;'                  "43"
check "num con spazi"        'print num("  10  ");'                  "10"
check "num non valido"       'print num("ciao");'   "Errore a runtime: num(): la stringa non e' un numero valido."

echo "== Errori a runtime =="
check "divisione per zero"   "print 1 / 0;"       "Errore a runtime: divisione per zero."
check "modulo per zero"      "print 5 % 0;"       "Errore a runtime: modulo per zero."
check "variabile non def."   "print y;"           "Errore a runtime: variabile 'y' non definita."
check "assegn. a non def."   "z = 5;"             "Errore a runtime: assegnamento a variabile 'z' non definita."
check "tipo sbagliato"       'print "a" - 1;'     "Errore a runtime: l'operatore richiede un numero, ma il valore e' di tipo stringa."
check "+ tipi misti"         'print 1 + "a";'     "Errore a runtime: '+' vuole due numeri o due stringhe."
check "ricorsione infinita"  "fun f() { return f(); } print f();"  "Errore a runtime: profondita' massima superata (ricorsione o espressione troppo profonda)."
# Funzione con corpo "pesante" (espressione annidata) ricorsiva a fondo: il guard
# conta la profondita' REALE di evaluate/execute, non solo le chiamate -> niente crash.
check "ricorsione pesante"   "fun f(n){ if(n<=0){return [0];} return [f(n-1)[0]+1]; } print f(5000)[0];" "Errore a runtime: profondita' massima superata (ricorsione o espressione troppo profonda)."

echo "== Errori di sintassi =="
check "espressione monca"    "print 1 +;"         "[riga 1] Errore di sintassi vicino a ';': Mi aspettavo un numero, un nome, una '(' o un '['."
check "parentesi aperta"     "print (1 + 2;"      "[riga 1] Errore di sintassi vicino a ';': Mi aspettavo ')' per chiudere la parentesi."
check "punto e virgola"      "var n = 1 print n;" "[riga 1] Errore di sintassi vicino a 'print': Mi aspettavo ';' dopo la dichiarazione."
check "bersaglio invalido"   "3 = 5;"             "[riga 1] Errore di sintassi vicino a '5': Bersaglio di assegnamento non valido."
check "stringa non chiusa"   'print "ciao;'       "[riga 1] Errore di sintassi: Stringa non chiusa: manca la '\"' finale."
check "un solo &"            "print 1 & 2;"       "[riga 1] Errore di sintassi: Un solo '&' non e' valido: forse intendevi '&&'."

echo ""
echo "-----------------------------------------"
echo "Risultato: $pass passati, $fail falliti"
echo "-----------------------------------------"

# codice di uscita: 0 se tutti passano, 1 se qualcuno fallisce
[ "$fail" -eq 0 ]
