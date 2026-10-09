# ViKi — un linguaggio di programmazione scritto in C, per capire come funzionano i linguaggi

ViKi è un piccolo linguaggio di programmazione con il suo interprete, scritto da zero in C.
È nato per passione, con il manuale *K&R* (Kernighan & Ritchie) come riferimento, e ha un
obiettivo semplice: **mostrare cosa succede davvero quando scrivi codice e premi "esegui"**.

Se stai iniziando a programmare, probabilmente usi Python, JavaScript o Scratch e ti sembra che
il computer "capisca" il tuo codice per magia. Non c'è nessuna magia: c'è un programma, scritto da
qualcuno, che legge il testo che hai scritto e lo esegue. ViKi è uno di questi programmi, abbastanza
piccolo da poterlo leggere tutto in un pomeriggio.

```
ViKi:> let nome = "Mario";
ViKi:> print "Ciao " + nome + "!";
>> Ciao Mario!
```

---

## Perché può esserti utile

- **Capisci cosa c'è sotto.** Variabili, cicli, funzioni, scope, ricorsione: qui non sono parole
  magiche ma poche decine di righe di C che puoi leggere, modificare e rompere.
- **Quasi tutti i linguaggi si assomigliano.** Al fondo un linguaggio è fatto di tipi di dati,
  strutture di controllo (`if`, `while`, funzioni) e librerie. Imparato questo, passare da un
  linguaggio a un altro diventa molto più facile.
- **Vedi la memoria al lavoro.** In C la memoria si gestisce a mano (`malloc`, `free`). Nel codice
  trovi esempi concreti di chi "possiede" un dato e di chi deve liberarlo, che nei linguaggi ad
  alto livello restano nascosti.
- **È piccolo.** Pochi file, nessuna dipendenza esterna, si compila con un solo comando.

---

## Come si compila e si usa

Ti serve solo un compilatore C (`gcc` o `clang`).

```bash
gcc -Wall -Wextra -o ViKi lexer.c parser.c interpreter.c main.c
```

**Eseguire un file** di programma:

```bash
./ViKi esempi.txt
```

**Usare la shell interattiva** (senza argomenti): scrivi una riga, ViKi la esegue subito.
Le variabili e le funzioni restano disponibili da una riga all'altra. Scrivi `exit` per uscire.

```bash
./ViKi
```

> Suggerimento per chi sviluppa: compila con `-g -fsanitize=address,undefined` e il compilatore
> ti segnalerà errori di memoria (accessi fuori limite, memoria usata dopo `free`, perdite) che
> altrimenti sono difficilissimi da trovare.

---

## Il linguaggio in 5 minuti

Ogni istruzione termina con `;`.

### Variabili e tipi

```
let x = 10;          // dichiarare una variabile
x = x * 2;           // cambiare il valore di una variabile già dichiarata
let nome = "Mario";  // le stringhe si scrivono con "..." oppure '...'
```

ViKi ha tre tipi di valori:

| Tipo      | Esempio                  | Note                                                    |
|-----------|--------------------------|---------------------------------------------------------|
| numero    | `42`, `3.14`             | internamente è sempre un `double`                       |
| stringa   | `"ciao"`                 | escape supportate: `\n`, `\t`, `\"`, `\\`               |
| booleano  | risultato di `5 == 5`    | si stampa `true` / `false`; per ora nasce solo dai confronti |

### Operatori

```
+  -  *  /            aritmetica (e il meno unario: -3)
== != < > <= >=       confronti, il risultato è true oppure false
```

Il `+` tra una stringa e un numero concatena: `"giro " + 3` dà `"giro 3"`.
Per le condizioni sono "falsi" `false`, il numero `0` e la stringa vuota; tutto il resto è vero.

### Stampare

```
print 1 + 2;
```
```
>> 3
```

### Decisioni e cicli

```
let x = 20;
if (x > 15) print "grande"; else print "piccolo";

let i = 1;
while (i <= 3) {
    print "giro " + i;
    i = i + 1;
}
```
```
>> grande
>> giro 1
>> giro 2
>> giro 3
```

Le graffe `{ ... }` raggruppano più istruzioni in un blocco.

### Funzioni

```
fn fatt(n) {
    if (n < 2) { return 1; }
    return n * fatt(n - 1);
}
print fatt(5);
```
```
>> 120
```

Le funzioni possono richiamare sé stesse (ricorsione). Per evitare che un errore mandi in crash
l'interprete, ViKi si ferma con un messaggio dopo 500 chiamate annidate.

### Scope: dove "vivono" le variabili

Questa è una delle idee più importanti in programmazione e in ViKi si vede in poche righe.

- Ogni chiamata di funzione ha le **sue** variabili locali, che spariscono quando la funzione finisce.
- Se una funzione usa un nome che non è locale, lo cerca tra le variabili **globali**.
- `let` crea sempre una variabile nello scope corrente; l'assegnamento senza `let` modifica
  quella già esistente (locale o, se non c'è, globale).

```
let g = 100;
fn locale()  { let g = 1; return g + 1; }   // crea una g locale, che "nasconde" quella globale
fn globale() { g = g + 1; }                 // modifica la g globale

print locale();   // >> 2
print g;          // >> 100   (la globale non è cambiata)
globale();
print g;          // >> 101
```

Le funzioni si definiscono solo al livello superiore, non dentro altre funzioni (come in C).

---

## Come funziona dentro: da testo a risultato

Quando esegui un programma, ViKi lo trasforma in tre passaggi. Ogni passaggio è un file.

```
 testo del programma
        │
        ▼
  ┌───────────┐   lexer.c      spezza il testo in "parole": i token
  │   LEXER   │
  └─────┬─────┘
        ▼
  ┌───────────┐   parser.c     capisce la struttura e costruisce un albero (AST)
  │  PARSER   │
  └─────┬─────┘
        ▼
  ┌───────────┐   interpreter.c   percorre l'albero ed esegue
  │INTERPRETE │
  └─────┬─────┘
        ▼
     risultato
```

Prendiamo `print 1 + 2;`:

1. **Lexer** — il testo diventa una lista di token:
   `PRINT` `NUMBER(1)` `PLUS` `NUMBER(2)` `SEMICOLON`
2. **Parser** — i token diventano un albero che rispetta le precedenze (la moltiplicazione prima
   della somma, ecc.):
   ```
   PRINT
     └── +
         ├── 1
         └── 2
   ```
3. **Interprete** — visita l'albero dal basso: calcola `1 + 2 = 3`, poi lo stampa.

Questa idea (testo → token → albero → esecuzione) è la stessa usata, con molte più rifiniture,
da Python, JavaScript e dalla maggior parte dei linguaggi.

### I file del progetto

| File | Cosa contiene |
|------|----------------|
| `lexer.h` / `lexer.c` | i tipi di token e il codice che trasforma il testo in token |
| `parser.h` / `parser.c` | i tipi di nodo dell'albero e il parser (*recursive descent*) |
| `interpreter.h` / `interpreter.c` | valori, variabili, scope, funzioni e la valutazione dell'albero |
| `main.c` | avvio del programma: esecuzione di un file e shell interattiva |
| `esempi.txt` | programmi di prova che funzionano |
| `errori.txt` | programmi che provocano di proposito errori, per vedere i messaggi |

### Se vuoi leggere il codice, parti da qui

1. `lexer.h` — guarda l'elenco dei token: è il "vocabolario" del linguaggio.
2. `parser.c` — le funzioni `expression`, `term`, `factor`, `primary` mostrano come le precedenze
   degli operatori diventano struttura del codice.
3. `interpreter.c` — la funzione `evaluate` è un grande `switch` che dice cosa fa ogni tipo di nodo.

---

## Limiti attuali (e idee per migliorarlo)

ViKi è volutamente piccolo. Quello che manca è un ottimo esercizio: ogni punto qui sotto si può
aggiungere toccando uno o due file.

| Idea | Cosa devi toccare | Difficoltà |
|------|-------------------|------------|
| Commenti con `//` | lexer | facile |
| Letterali `true` e `false` | lexer, parser | facile |
| Operatore resto `%` | lexer, parser, interprete | facile |
| `and`, `or`, `not` | lexer, parser, interprete | media |
| Messaggi di errore con numero di riga per gli errori di sintassi | parser | media |
| Ciclo `for` | parser (anche trasformandolo in `while`) | media |
| Funzioni predefinite (`len`, `input`, `sqrt`...) | interprete | media |
| Array / liste | interprete, parser, gestione memoria | difficile |
| Funzioni dentro funzioni e chiusure | scope, memoria | difficile |
| Compilare in bytecode invece di percorrere l'albero | nuovo modulo | difficile |

Limiti noti oggi:

- gli errori di sintassi (una parentesi mancante, per esempio) sono per lo più silenziosi;
- non ci sono commenti, array, ciclo `for` né operatori logici;
- i booleani nascono solo dai confronti, non esistono ancora `true` e `false` scrivibili a mano.

---

## Cosa si impara costruendolo

- Come si legge un testo carattere per carattere e lo si divide in pezzi con significato.
- Perché `2 + 3 * 4` fa 14 e non 20, e come lo "sa" un programma.
- Cosa sono davvero uno scope, una variabile locale, una chiamata ricorsiva.
- Come si gestisce la memoria a mano: chi alloca, chi libera, e cosa succede se ci si sbaglia.
- Come usare strumenti come AddressSanitizer per trovare i bug invisibili.

---

## Contribuire e sperimentare

Questo è un progetto di apprendimento: cambiare, rompere e riscrivere il codice è lo scopo.
Se aggiungi una funzionalità dalla tabella sopra, o trovi un bug, apri pure una *issue* o una
*pull request*.

Buon divertimento, e ricorda: ogni linguaggio che usi è stato scritto da persone, quindi puoi
capirlo anche tu.
