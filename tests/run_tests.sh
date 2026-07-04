#!/bin/sh
# Suite di test di Nura.
# Esegue tanti programmini e confronta l'output con quello atteso.
# Uso:  sh tests/run_tests.sh     (oppure:  make test)

NURA=./nura
pass=0
fail=0

# check "descrizione" "codice" "output atteso"
# Cattura anche stderr (2>&1) cosi' possiamo testare i messaggi d'errore.
check() {
    # tr -d '\r' rimuove i ritorni a capo di Windows (\r), cosi' il confronto
    # funziona uguale su Windows e Linux.
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

echo "== Confronti =="
check "maggiore"             "print 10 > 3;"             "1"
check "minore-uguale"        "print 4 <= 4;"            "1"
check "diverso falso"        "print 5 != 5;"             "0"
check "uguaglianza"          "print 2 + 2 == 4;"         "1"

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

echo "== Errori a runtime =="
check "divisione per zero"   "print 1 / 0;"       "Errore a runtime: divisione per zero."
check "modulo per zero"      "print 5 % 0;"       "Errore a runtime: modulo per zero."
check "variabile non def."   "print y;"           "Errore a runtime: variabile 'y' non definita."
check "assegn. a non def."   "z = 5;"             "Errore a runtime: assegnamento a variabile 'z' non definita."

echo "== Errori di sintassi =="
check "espressione monca"    "print 1 +;"         "[riga 1] Errore di sintassi vicino a ';': Mi aspettavo un numero, un nome o una '('."
check "parentesi aperta"     "print (1 + 2;"      "[riga 1] Errore di sintassi vicino a ';': Mi aspettavo ')' per chiudere la parentesi."
check "punto e virgola"      "var n = 1 print n;" "[riga 1] Errore di sintassi vicino a 'print': Mi aspettavo ';' dopo la dichiarazione."
check "bersaglio invalido"   "3 = 5;"             "[riga 1] Errore di sintassi vicino a '5': Bersaglio di assegnamento non valido."

echo ""
echo "-----------------------------------------"
echo "Risultato: $pass passati, $fail falliti"
echo "-----------------------------------------"

# codice di uscita: 0 se tutti passano, 1 se qualcuno fallisce
[ "$fail" -eq 0 ]
