# stm32-serial-control

## Il progetto

Questo è un firmware bare-metal per STM32F405, in cui il compito del firmware è rispondere a comandi inviati sulla porta seriale ed eseguire il comando solo se è autentico. Tutto il resto viene ignorato.

Il firmware è stato scritto e testato usando QEMU. Insieme al firmware c'è uno script Python per testare i vari scenari: messaggi corretti ed autentici, messaggi alterati, ripetuti e malformati.

Ogni comando è firmato con un HMAC-SHA256 calcolato con una chiave condivisa e porta con sé un contatore che impedisce di far accettare di nuovo messaggi vecchi. 

Per lo SHA-256 ho usato un'implementazione esistente ([amosnier/sha-2](https://github.com/amosnier/sha-2)), che ho messo in un unico header e adattato per funzionare senza libc. L'HMAC sopra lo SHA-256 invece l'ho scritto io, sono poche righe, e l'ho verificato confrontandolo con il modulo `hmac` di Python.

## Compilare e far girare

Serve `arm-none-eabi-gcc`, `qemu-system-arm` e Python 3. Su Arch si installano con `make install`, su Debian/Ubuntu con:

```sh
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm
```

Poi in un terminale:

```sh
make run
```

che compila e avvia QEMU. QEMU aspetta che qualcuno si colleghi alla porta 4444 prima di far partire il firmware. In un secondo terminale:

```sh
python3 test/test_serial_control.py
```

Le porte sono queste:

- `127.0.0.1:4444` comandi e risposte (USART1)
- `127.0.0.1:8888` debug (USART2), si legge con `nc 127.0.0.1 8888`
- il display a sette segmenti (USART3) esce direttamente nel terminale di QEMU

Per uscire da QEMU `Ctrl-A` e poi `X`. Lo script parte sempre dal contatore 1, quindi per rilanciarlo bisogna riavviare QEMU, altrimenti il firmware scarta tutto come replay (giustamente).

## Architettura

### Perché bare-metal

La motivazione della scelta del bare-metal è stata quella per la semplicità di realizzazione. 
Questa è la prima volta che scrivo su piattaforma STM32, in precedenza ho usato RISCV sempre bare-metal, quindi avevo già conoscenze di QEMU e programmazione senza RTOS/OS, per velocizzare il tutto ho preferito avere controllo su tutto, dal layout della memoria alle implementazioni di alto livello.

Il firmware fa una cosa alla volta, quindi uno scheduler non mi serviva. Il prezzo è che la seriale è letta in polling: su una scheda vera, mentre il firmware calcola l'HMAC, potrebbe perdere dei byte. In QEMU non succede. Con più tempo userei gli interrupt e un buffer circolare.

### File

| File | Cosa contiene |
|------|---------------|
| `src/init.s` | Tabella dei vettori e reset handler |
| `src/main.c` | Avvio (copia di `.data`, azzeramento di `.bss`), inizializzazione delle periferiche e loop |
| `include/stm32_qemu.h` | Definizioni dei registri e funzioni di supporto per le USART |
| `include/messages.h` | Struct dei frame, parsing, validazione, risposte, dump di debug |
| `include/sensors.h` | Bitmap e disegno del display a sette segmenti |
| `include/sha256.h`, `include/hmac.h` | SHA-256 e HMAC-SHA256 |
| `include/secrets.h` | La chiave condivisa |
| `linker.ld` | Layout della memoria per flash e RAM |
| `Makefile` | Preparazione, compilazione e run per QEMU |
| `test/test_serial_control.py` | Script lato PC: comandi validi e attacchi |

### Il loop principale

Tutto il firmware è un unico loop che si ripete all'infinito:

1. **`read_message`** aspetta finché non arriva un frame completo e ben formato.
2. **`validate_message`** decide se il frame è autentico, recente e sensato.
3. Se lo è, il comando viene eseguito e **`write_message`** invia la risposta.

### Formato del frame

Ogni comando è una struct senza padding di dimensione fissa, 31 byte:

| Offset | Dimensione | Campo |
|-------:|-----------:|-------|
| 0      | 1          | marcatore di inizio `0xAA` |
| 1      | 1          | comando |
| 2      | 8          | argomenti |
| 10     | 4          | contatore (little-endian) |
| 14     | 16         | tag HMAC-SHA256 (troncato) |
| 30     | 1          | marcatore di fine `0xFF` |

Del tag HMAC tengo solo i primi 16 byte: 128 bit sono comunque impossibili da indovinare mandando tentativi sulla seriale.

### Parsing (`read_message`)

Il parser legge un byte alla volta:

1. Quando non è dentro un frame, scarta tutto finché non trova `0xAA`.
2. Poi raccoglie byte finché la struct non è piena e controlla che l'ultimo sia `0xFF`.
3. Se non lo è, il parser non butta via tutto il buffer. Cerca il prossimo `0xAA` tra i byte che ha già, li sposta all'inizio e riparte da lì.

In questo modo si risincronizza dopo il rumore o un frame troncato. Il buffer ha una dimensione fissa, quindi un messaggio troppo lungo non può mai farlo andare in overflow: semplicemente non supera il controllo dell'end. I marcatori vengono controllati solo nelle loro posizioni fisse, quindi `0xAA` o `0xFF` dentro gli argomenti, il contatore o l'HMAC non confondono il parser.

### Validazione (`validate_message`)

I controlli vengono fatti in quest'ordine, e al primo che fallisce il frame viene scartato:

1. **Autenticità.** Il firmware ricalcola l'HMAC su comando, argomenti e contatore con la chiave condivisa e lo confronta sempre per intero, così il tempo impiegato non dice quanti byte erano giusti.
2. **Nuovo.** Il contatore deve essere strettamente maggiore dell'ultimo accettato (`last_counter`). Questo impedisce a qualcuno di registrare un frame e rimandarlo più tardi, finché il dispositivo non viene riavviato.
3. **Contenuto.** Il comando deve essere uno di quelli definiti e gli argomenti devono essere nel range (per `CMD_SSEGMENT`, una cifra da 0 a 9).

`last_counter` viene aggiornato solo quando tutti e tre i controlli passano, così un frame scartato non "brucia" un valore del contatore.

## IO della scheda

Uso tre USART, ognuna con il suo compito:

| Porta  | Direzione   | Cosa fa |
|--------|-------------|---------|
| USART1 | ingresso / uscita | Console per comandi |
| USART2 | uscita      | Console di debug |
| USART3 | uscita      | Il display a sette segmenti |

Tenere il debug su una porta separata è stata una delle prime decisioni. Mischiare log e frame binari sulla stessa linea non era una buona idea, e QEMU permette di avere più seriali separate per questa scheda.

### Comandi

| Comando | Valore | Argomenti | Effetto |
|---------|-------:|-----------|---------|
| `CMD_SSEGMENT` | `0x01` | `args[0]` = cifra 0–9 | Mostra la cifra sul display a sette segmenti |

### Risposte

Il dispositivo risponde solo se il comando è valido ed è stato eseguito, con 11 byte su USART1. Se il comando viene rifiutato non risponde niente.

| Offset | Dimensione | Campo |
|-------:|-----------:|-------|
| 0      | 1          | marcatore di inizio `0x55` |
| 1      | 1          | comando |
| 2      | 8          | argomenti (`args[0]` = 1) |
| 10     | 1          | marcatore di fine `0x99` |

*La risposta non è autenticata: il dispositivo non prende decisioni in base a essa, e l'obiettivo era proteggere il dispositivo, non il client. In un sistema reale andrebbe firmata anche la risposta.*

### Script lato PC

`test/test_serial_control.py` costruisce e firma i frame con la stessa chiave e manda:

- 10 comandi validi, cifre da 0 a 9
- comandi alterati: cifra cambiata, contatore cambiato, firmati con la chiave sbagliata
- replay: lo stesso frame due volte, lo stesso contatore con un'altra cifra, un contatore vecchio
- messaggi malformati: frame troncato, marcatore di fine sbagliato, byte a caso

Per ogni test stampa `PASS` o `FAIL`.

## Messaggi malformati e sbagliati: perché li ignoro

Questa è forse la scelta che può sembrare più strana.

Il dispositivo non dice mai *perché* un messaggio è stato scartato. Un frame malformato viene semplicemente buttato via dal parser, e un frame che non supera i controlli di autenticità, di replay o di contenuto non riceve nessuna spiegazione. I motivi sono due.

### Robusto non vuol dire sicuro

In *Robust Programming by Example*, Matt Bishop e Chip Elliott spiegano la differenza tra programmazione robusta e programmazione sicura. Un programma robusto, quando qualcosa va storto, dovrebbe dire chiaramente cosa è successo, con indicatori di errore non ambigui e dettagliati. Per la maggior parte del software è esattamente quello che si vuole, perché rende il debug facile.

Ma quando un'interfaccia deve essere sicura, un errore dettagliato diventa un regalo per l'attaccante. Ogni risposta di errore diversa è un *indizio*, una domanda sì/no che l'attaccante può fare tutte le volte che vuole, finché non capisce come funziona il sistema all'interno. Per questo qui tutti i rifiuti sembrano uguali dall'esterno.

### Quello che ho visto facendo reverse engineering

Questo l'ho imparato soprattutto in prima persona. Quando ho lavorato per un periodo ad un reverse engineering di una GPU NVIDIA, i messaggi sbagliati inviati alla GPU non restituivano nulla, nessun errore o segnale. Da software non si poteva fare debug né capire cosa stesse facendo: quando un messaggio era sbagliato, niente ti diceva perché.

L'unico modo rimasto per capire cosa facesse la GPU era scendere a livello hardware. E questo, porta molte persone ad abbandonare il reversing e a cercare altre strade. È esattamente quello che ho sperimentato su pelle e che mi ha portato a scegliere questa strategia: se l'attaccante non riceve nessun indizio dal software, capire il firmware diventa molto più costoso.

## Modello di minaccia

L'attaccante che ho considerato ha accesso alla linea seriale: può leggere il traffico, mandare frame suoi, modificare quelli veri e rimandare frame registrati. Non conosce la chiave.

Da cosa mi protegge il firmware:

- comandi inventati o modificati, perché senza la chiave l'HMAC non torna
- comandi rimandati, perché il contatore deve sempre crescere
- messaggi troppo lunghi, troncati o spazzatura, che vengono scartati senza crash

Da cosa invece no:

- **Replay dopo un riavvio.** Il contatore sta in RAM e dopo un reset riparte da 0, quindi i vecchi frame tornano validi. È il problema più grosso.
- **Chi legge la linea vede i comandi**, perché non sono cifrati. L'HMAC garantisce che sono autentici, non che sono segreti.
- **Le risposte** si possono falsificare o bloccare, perché non sono firmate.
- **Chi vuole bloccare il dispositivo** può sempre riempire la linea di spazzatura o tagliare il cavo.
- **Attacchi fisici**, cioè dump della flash, glitch, side channel.
- **La chiave è nel repository** e nello script Python. Per una demo va bene, per un prodotto no.

### Dove terrei la chiave

Adesso la chiave è compilata nel firmware, quindi chi fa il dump della flash ce l'ha. Su un dispositivo vero:

- attiverei la read-out protection (RDP) dell'STM32. Il livello 1 però si può aggirare con il glitching, il livello 2 blocca il debug per sempre ed è il minimo per un prodotto.
- userei una chiave diversa per ogni dispositivo, ricavata da una chiave master e dall'ID univoco del chip. Così se qualcuno estrae la chiave da un dispositivo, gli altri restano al sicuro.
- se possibile, metterei la chiave in un secure element (tipo ATECC608), da cui non esce mai, e farei calcolare l'HMAC a lui.

## L'attacco che avevo in mente

Ad essere sincero, mentre lo programmavo non avevo in mente un attacco preciso. Mi sono concentrato soprattutto su due principi:

1. **Controlli rigorosi sull'IO.** Ogni byte che entra viene trattato come ostile. Il frame ha una dimensione fissa, i marcatori sono controllati in posizioni fisse, l'HMAC deve corrispondere, il contatore deve essere nuovo e gli argomenti devono essere nel range. Tutto quello che non passa viene scartato, e il dispositivo torna ad aspettare come se niente fosse.
2. **Non dare mai un indizio all'attaccante.** L'attaccante non deve poter capire niente di quello che succede dentro il firmware: né dai messaggi di errore e né da risposte diverse.

A lavoro finito ho provato a mettermi dalla parte dell'attaccante.

Immagino che il display mostri a un operatore un livello di allarme da 0 a 9. L'attaccante si collega ai fili della seriale e vuole fargli vedere 0 quando il livello reale è 9.

- Se inventa un comando, senza la chiave non può calcolare l'HMAC e il frame viene scartato.
- Se modifica un comando vero cambiando la cifra, l'HMAC non corrisponde più.
- Se rimanda un vecchio comando con la cifra 0, il contatore è vecchio e viene scartato.
- Se manda spazzatura per far crashare il parser, il dispositivo continua a funzionare.

E in nessun caso riceve una risposta, quindi non capisce cosa l'ha fermato.

Ci riesce invece se può togliere e ridare corrente: il contatore torna a 0 e il vecchio comando con la cifra 0 viene accettato. Oppure se mette le mani sulla scheda e legge la chiave dalla flash.

## Cosa farei meglio

- **Contatore dopo un reset.** Lo salverei nella backup SRAM dell'STM32 (che resta alimentata dalla batteria) o in flash. Oppure userei un challenge-response: il dispositivo genera un numero casuale e il client deve firmarlo insieme al comando, così non serve ricordarsi niente tra un riavvio e l'altro.
- **Conservare la chiave nel modo giusto**, come descritto sopra.
- **Rallentare gli attaccanti.** Un meccanismo che rallenti o blocchi temporaneamente il dispositivo dopo un certo numero di messaggi non validi renderebbe il brute force ancora meno conveniente, anche se questo potrebbe portare a dover identificare gli utenti, così da non rallentare anche chi richiede le informazioni in maniera corretta.