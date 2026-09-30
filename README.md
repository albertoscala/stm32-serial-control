# stm32-serial-control

## Il progetto

Questo è un firmware bare-metal per STM32F405, in cui il compito del firmware è rispondere a comandi inviati sulla porta seriale ed eseguire il comando solo se è autentico. Tutto il resto viene ignorato.

Il firmware è stato scritto e testato usando QEMU. Insieme al firmware c'è uno script Python per testare i vari scenari: messaggi corretti ed autentici, messaggi alterati, ripetuti e malformati.

Ogni comando è firmato con un HMAC-SHA256 calcolato con una chiave condivisa e porta con sé un contatore che impedisce di far accettare di nuovo messaggi vecchi. 

Per la parte crittografica ho usato un'implementazione esistente di SHA-256/HMAC.

## Architettura

### Perché bare-metal

La motivazione della scelta del bare-metal è stata quella per la semplicità di realizzazione. 
Questa è la prima volta che scrivo su piattaforma STM32, in precedenza ho usato RISCV sempre bare-metal, quindi avevo già conoscenze di QEMU e programmazione senza RTOS/OS, per velocizzare il tutto ho preferito avere controllo su tutto, dal layout della memoria alle implementazioni di alto livello.

### File

| File | Cosa contiene |
|------|---------------|
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

### Parsing (`read_message`)

Il parser legge un byte alla volta:

1. Quando non è dentro un frame, scarta tutto finché non trova `0xAA`.
2. Poi raccoglie byte finché la struct non è piena e controlla che l'ultimo sia `0xFF`.
3. Se non lo è, il parser non butta via tutto il buffer. Cerca il prossimo `0xAA` tra i byte che ha già, li sposta all'inizio e riparte da lì.

In questo modo si risincronizza dopo il rumore o un frame troncato, senza perdere un frame valido che magari era iniziato in mezzo alla spazzatura. Il buffer ha una dimensione fissa, quindi un messaggio troppo lungo non può mai farlo andare in overflow: semplicemente non supera il controllo dell end. I marcatori vengono controllati solo nelle loro posizioni fisse, quindi `0xAA` o `0xFF` dentro gli argomenti, il contatore o l HMAC non confondono il parser.

### Validazione (`validate_message`)

I controlli vengono fatti in quest'ordine, e al primo che fallisce il frame viene scartato:

1. **Autenticità.** Il firmware ricalcola l'HMAC su comando, argomenti e contatore con la chiave condivisa e lo confronta.
2. **Nuovo.** Il contatore deve essere strettamente maggiore dell'ultimo accettato (`last_counter`). Questo impedisce a qualcuno di registrare un frame e rimandarlo più tardi.
3. **Contenuto.** Il comando deve essere uno di quelli definiti e gli argomenti devono essere nel range (per `CMD_SSEGMENT`, una cifra da 0 a 9).

`last_counter` viene aggiornato solo quando tutti e tre i controlli passano, così un frame scartato non "brucia" un valore del contatore.

## IO della scheda

Uso tre USART, ognuna con il suo compito:

| Porta  | Direzione   | Cosa fa |
|--------|-------------|---------|
| USART1 | ingresso / uscita | Console per comandi |
| USART2 | uscita      | Console di debug |
| USART3 | uscita      | Il display a sette segmenti |

Tenere il debug su una porta separata è stata una delle prime decisioni. Mischiare log e frame binari sulla stessa linea non era una buona idea dato che QEMU fornisce fino a 5 seriali per questa scheda.

### Comandi

| Comando | Valore | Argomenti | Effetto |
|---------|-------:|-----------|---------|
| `CMD_SSEGMENT` | `0x01` | `args[0]` = cifra 0–9 | Mostra la cifra sul display a sette segmenti |

### Risposte

Quando un comando è valido viene eseguito, e `write_message` rimanda una risposta di 11 byte su USART1:

| Offset | Dimensione | Campo |
|-------:|-----------:|-------|
| 0      | 1          | marcatore di inizio `0x55` |
| 1      | 1          | comando |
| 2      | 8          | argomenti (`args[0]` = 1 successo, 0 fallimento) |
| 10     | 1          | marcatore di fine `0x99` |

*La risposta non è autenticata: il dispositivo non prende decisioni in base a essa, e l'obiettivo era proteggere il dispositivo, non il client. In un sistema reale andrebbe firmata anche la risposta.*

### Script lato PC

`test/test_serial_control.py` costruisce e firma i frame con la stessa chiave, e testa sia i comandi sia il parser: frame troncati, rumore, marcatori dentro il payload, replay, tag sbagliati e cifre fuori range.

```sh
python3 test/test_serial_control.py bin/firmware.elf   # avvia QEMU da solo
python3 test/test_serial_control.py                    # QEMU già avviato (make run)
```

## Messaggi malformati e sbagliati: perché li ignoro

Questa è forse la scelta che può sembrare più strana.

Il dispositivo non dice mai *perché* un messaggio è stato scartato. Un frame malformato viene semplicemente buttato via dal parser, e un frame che non supera i controlli di autenticità, di replay o di contenuto non riceve nessuna spiegazione. I motivi sono due.

### Robusto non vuol dire sicuro

In *Robust Programming by Example*, Matt Bishop e Chip Elliott spiegano la differenza tra programmazione robusta e programmazione sicura. Un programma robusto, quando qualcosa va storto, dovrebbe dire chiaramente cosa è successo, con indicatori di errore non ambigui e dettagliati. Per la maggior parte del software è esattamente quello che si vuole, perché rende il debug facile.

Ma quando un'interfaccia deve essere sicura, un errore dettagliato diventa un regalo per l'attaccante. Ogni risposta di errore diversa è un *indizio*, una domanda sì/no che l'attaccante può fare tutte le volte che vuole, finché non capisce come funziona il sistema all'interno. Per questo qui tutti i rifiuti sembrano uguali dall'esterno.

### Quello che ho visto facendo reverse engineering

Questo l'ho imparato soprattutto in prima persona. Quando ho lavorato per un periodo ad un reverse engineering di una GPU NVIDIA, i messaggi sbagliati inviati alla GPU non restituivano nulla, nessun errore o segnale. Da software non si poteva fare debug né capire cosa stesse facendo: quando un messaggio era sbagliato, niente ti diceva perché.

L'unico modo rimasto per capire cosa facesse la GPU era scendere a livello hardware. E questo, porta molte persone ad abbandonare il reversing e a cercare altre strade. È esattamente quello che ho sperimentato su pelle e che mi ha portato a scegliere questa strategia: se l'attaccante non riceve nessun indizio dal software, capire il firmware diventa molto più costoso.

## Cosa farei meglio

- **Conservare la chiave nel modo giusto.** Adesso la chiave sta nel firmware stesso (`secrets.h`, compilata nella flash). Non è assolutamente un buon modo di conservarla in produzione: chiunque faccia il dump della flash ha la chiave. Con più tempo, e soprattutto su hardware reale, la proteggerei attivando la read-out protection (RDP), che impedisce di leggere la flash dall'esterno.
- **Contatore dopo un reset.** `last_counter` sta in RAM, quindi dopo un riavvio torna a 0 e i frame vecchi tornano validi. Andrebbe salvato in flash, oppure sostituito da un meccanismo più robusto.
- **Rallentare gli attaccanti.** Un meccanismo che rallenti o blocchi temporaneamente il dispositivo dopo un certo numero di messaggi non validi renderebbe il brute force ancora meno conveniente, anche se questo potrebbe portare dover identificare gli utenti, così da non rallentare anche chi richiede le informazioni in maniera corretta.

## L'attacco che avevo in mente

Ad essere sincero, mentre lo programmavo non avevo in mente un attacco preciso. Mi sono concentrato soprattutto su due principi:

1. **Controlli rigorosi sull'IO.** Ogni byte che entra viene trattato come ostile. Il frame ha una dimensione fissa, i marcatori sono controllati in posizioni fisse, l'HMAC deve corrispondere, il contatore deve essere nuovo e gli argomenti devono essere nel range. Tutto quello che non passa viene scartato, e il dispositivo torna ad aspettare come se niente fosse.
2. **Non dare mai un indizio all'attaccante.** L'attaccante non deve poter capire niente di quello che succede dentro il firmware: né dai messaggi di errore e né da risposte diverse.

In pratica seguendo questi due principi si comprono la maggior parte degli attacchi software, ovviamente rimangono scoperti gli attacchi hardware.