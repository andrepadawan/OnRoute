# esp32_test — firmware di collaudo hardware OnRoute

Firmware **di test**: serve a rispondere a una sola domanda — *l'hardware funziona end-to-end?*
GPS NEO-6M → ESP32 → hotspot del telefono → `POST /update-coordinates` su Railway, senza Mac collegato.
Stato visibile su LED e su display OLED 128x64.

Non è il firmware definitivo.

---

## Struttura

```
esp32_test/
├── platformio.ini        board, librerie, aggancio a load_env.py
├── load_env.py           .env  ->  #define ENV_<CHIAVE> "valore"  (a compile time)
├── .env.example          modello del .env (il .env vero è gitignorato)
└── src/
    ├── main.cpp          setup()/loop(): orchestrazione, niente delay() nel loop
    ├── config.h          pin, tempi, default dal .env — tutto ciò che si "gira" sta qui
    ├── Coordinates.h     struct = specchio di PayloadReceived (Server/api.py) + toJson()
    ├── GpsReader.*       UART2 + TinyGPS++, lettura non bloccante, diagnostica
    ├── Settings.*        avvio WiFi (reti note -> captive portal) + override in NVS; token e linea in coppia
    ├── KnownNetworks.*   elenco di max 6 reti WiFi in NVS: sopravvive al flash del firmware
    ├── WifiKeeper.*      riaggancio quando la rete cade: scan asincrono delle reti note, poi riavvio dopo 30 min
    ├── Uploader.*        POST HTTPS con Bearer token, spiegazione dei codici d'errore
    ├── StatusLed.*       codici LED per quando la seriale non la vedi
    ├── LocalClock.*      orologio in ora italiana: dal GPS, oppure da NTP se c'è internet
    ├── NetProbe.*        "c'è internet?" — task FreeRTOS separato, non blocca il loop
    ├── NetState.h        enum Online/Offline/Unknown condiviso fra NetProbe e Screens
    ├── Display.*         OLED: ricerca sul bus I2C, init, invio del frame
    └── Screens.*         COSA si disegna (boot, portale, stato) + UiState
```

`Screens` disegna e basta: non sa nulla di I2C, WiFi o GPS. `main.cpp` riempie una `UiState`
(fotografia dei dati), `Display` la passa a `Screens` e spedisce il frame. Per cambiare il
layout tocchi un file solo.

## Cablaggio

| GPS GY-NEO6MV2 | ESP32 DevKit        | Note                                         |

All'avvio il firmware cerca lo schermo a `0x3C`/`0x3D` e scrive in seriale `[OLED] Schermo trovato`.
Se non lo trova fa una scansione del bus e **continua senza schermo**: GPS e invio funzionano lo stesso.
Schermo con una colonna di pixel spazzatura o immagine spostata di 2 pixel → è un SH1106 (tipico
dei 1.3"): in `platformio.ini` togli il `;` davanti a `build_flags = -D OLED_SH1106`.

Alimentazione da batteria/boost: condensatore bulk 220–1000 µF vicino ai pin di alimentazione
dell'ESP32. Se in seriale vedi `Brownout detector was triggered`, è quello.

## Primo avvio

1. `cp .env.example .env` e riempi `URL_WEBSITE` (URL **completo**, `https://…/update-coordinates`),
   `DEVICE_TOKEN` (identico a quello su Railway) e `LINE_NUMBER` (la linea di quel token).
2. Flash — da dentro `esp32_test/`:
   ```bash
   pio run -t upload
   pio device monitor
   ```
   In VS Code: *PlatformIO → Open Project → `esp32_test`* (il repo contiene due progetti
   PlatformIO, apri questo come progetto a sé).
   Durante la build vedi `[load_env] ENV_URL_WEBSITE = …` — se non compare, il `.env` non è stato letto.
3. Hotspot del telefono **a 2,4 GHz** (iPhone: *Hotspot personale → Massimizza compatibilità*;
   Android: banda 2,4 GHz). L'ESP32 non vede il 5 GHz.
4. Il LED resta **acceso fisso** e lo schermo dice `CONFIGURAZIONE`: collegati col telefono alla
   rete `OnRoute-Setup`, si apre il portale (altrimenti vai su `192.168.4.1`), scegli l'hotspot,
   inserisci la password, salva. I campi URL / token / numero linea servono solo se vuoi
   sovrascrivere il `.env` senza riflashare: **lasciati vuoti restano invariati**.
   La rete scelta finisce nell'elenco delle reti note (vedi sotto): la prossima volta il portale
   non si apre, se quella rete è in vista.
   **Token e linea sono una coppia**: si salvano sempre insieme e, se il `.env` cambia, si azzerano
   insieme. Se cambi il token dal portale, controlla che la linea precompilata sia ancora quella
   giusta (in seriale compare un avviso se hai cambiato il token ma non la linea).
5. Porta tutto **all'aperto**. Primo fix del NEO-6M: da 1 a 10+ minuti; i successivi molto più rapidi.
   Il LED sul modulo GPS lampeggia 1 volta al secondo solo quando ha il fix.

## Reti WiFi note

L'ESP32 ricorda fino a **6 reti** (`KNOWN_NETWORKS_MAX` in `config.h`), non solo l'ultima.
Stanno in NVS, una partizione separata dal firmware: `pio run -t upload` **non le cancella**.
Le cancellano solo *Erase Flash* (`pio run -t erase`) e il reset col tasto BOOT.

- **All'avvio**: scan, poi tentativo con la rete nota dal segnale più forte, poi le altre.
  Fino a 2 scan (l'hotspot potrebbe essere ancora in accensione). Nessuna rete nota in vista →
  captive portal, per 3 minuti; scaduto il portale, riavvio e si ricomincia.
- **Nuova rete dal portale**: si aggiunge all'elenco. Elenco pieno → sostituisce quella salvata
  da più tempo. Una rete già nota con password nuova viene aggiornata.
- **Quando la rete cade**: per 15 s ci prova il driver da solo, poi ogni 15 s uno scan
  **asincrono** (il loop non si blocca) e un tentativo con le reti note in vista; ogni 4
  tentativi la radio viene spenta e riaccesa prima dello scan. Dopo **30 minuti** senza WiFi:
  riavvio → se nessuna rete nota è in vista si riapre il portale.
- **Primo avvio dopo l'aggiornamento** da un firmware precedente: la rete salvata da WiFiManager
  viene importata nell'elenco, così il device in campo non riapre il portale.

All'avvio la seriale elenca le reti note, dalla più recente alla più vecchia (l'ultima è la
prossima a essere sostituita). Le password non vengono mai stampate.

## Display

```
┌────────────────────────────┐
│ LINEA 2          14:03:21  │  intestazione: linea (dal portale/.env) + ora italiana
│ 04/10/2026        ora GPS  │  data + chi ha dato l'ora: GPS, rete (NTP) o "attesa ora"
│ Trasmesso  SI 3s fa        │  SI se l'ultimo POST accettato ha meno di 15 s
│ Internet   SI              │  SI / NO / verifico...
│ GPS        3D 8 sat        │  tipo di fix e satelliti
└────────────────────────────┘
```

I valori problematici sono **in negativo** (riquadro bianco, testo nero), così si vedono da un
metro. Quando `Trasmesso` è `NO`, accanto c'è il primo anello rotto della catena:

| Motivo      | Significa                                                        |
|-------------|------------------------------------------------------------------|
| `GPS muto`  | nessun byte dal GPS: cablaggio                                   |
| `no fix`    | GPS vivo ma senza posizione: cielo libero e pazienza             |
| `no WiFi`   | hotspot spento, lontano o a 5 GHz                                |
| `no rete`   | WiFi ok ma niente internet (hotspot senza dati mobili)           |
| `HTTP 401`  | (o altro codice) il server ha risposto con errore: vedi tabella sotto |
| `err rete`  | il POST è fallito a livello di rete (DNS, TLS, timeout)          |
| `URL?`      | URL vuoto o malformato                                           |
| `attesa`    | tutto pronto, il prossimo invio parte entro 5 s                  |

**Internet ≠ WiFi.** Un hotspot senza dati mobili dà WiFi pieno e internet zero. `NetProbe` lo
verifica aprendo una connessione TCP verso l'host del server ogni 10 s (in un task separato, così
un DNS che non risponde non congela schermo e GPS). Una risposta HTTP qualsiasi dal server vale
già come prova, e in quel caso la verifica non spreca dati. Con internet assente il firmware
**non tenta il POST**: riprova quando la verifica torna positiva.

**Ora:** il GPS dà l'ora UTC anche prima del fix (basta un satellite); senza GPS la prende da
NTP via internet. La conversione in ora italiana, compreso il cambio d'ora del 25 ottobre,
la fa la libc con la regola `CET-1CEST,M3.5.0,M10.5.0/3` in `config.h`.

## Codici LED (LED blu on-board, GPIO2)

| LED                      | Significato                              | Cosa fare                                   |
|--------------------------|------------------------------------------|---------------------------------------------|
| acceso fisso             | portale aperto                           | collegati a `OnRoute-Setup`                  |
| lampeggio veloce (5 Hz)  | il GPS non manda nessun byte             | cablaggio: TX→GPIO16, GND, alimentazione    |
| lampeggio lento (1 Hz)   | GPS vivo ma senza fix / primo invio      | all'aperto, cielo libero, aspetta           |
| 3 flash ogni 2 s         | WiFi perso o internet assente            | hotspot acceso? 2,4 GHz? dati mobili attivi? |
| 2 flash ogni 2 s         | il server risponde con errore            | monitor seriale o log Railway (vedi sotto)  |
| 1 flash ogni 2 s         | **tutto ok**, ultimo POST accettato      | niente: l'hardware funziona                 |

**Verifica senza Mac:** dal telefono apri `https://<tuo-servizio>.up.railway.app/coordinates`
e ricarica: `timestamp` deve avanzare ogni ~5 s. (Funziona anche fuori turno: cambia solo `is_active`.)

**Reset:** a firmware avviato tieni premuto **BOOT** per 5 s → cancella **tutte** le reti salvate e
gli override, riavvia, riapre il portale. (Non tenerlo premuto mentre dai corrente: quella è la modalità flash.)

## Leggere il monitor seriale

```
[GPS] chars=48213 ok=402 fail=0 sats=8 hdop=1.1 fix=3 lat=45.070812 lng=7.686234 age=412ms
[HTTP] POST {"longitude":7.686234,…} -> 200 (640 ms)
```

| Sintomo                     | Causa probabile                                                  |
|-----------------------------|------------------------------------------------------------------|
| `chars=0`                   | TX/RX invertiti, GND mancante, GPS non alimentato                |
| `fail` che cresce           | baud sbagliato o disturbi sul filo                               |
| `chars` ok, `sats=0`        | il GPS parla ma non vede il cielo                                |
| `-> 401`                    | token diverso da quello su Railway                               |
| `-> 404`                    | l'URL non finisce con `/update-coordinates`                      |
| `-> 422`                    | JSON non conforme a `PayloadReceived` (leggi il `detail`)        |
| `-> 301/308`                | URL con `http://` invece di `https://`                           |
| `-> -1` / errore di rete    | hotspot senza dati mobili, DNS, server spento                    |

## Test contro il server sul Mac (senza Railway)

Mac e ESP32 sullo stesso hotspot, poi da `Server/` lancia `uvicorn api:app --host 0.0.0.0 --port 8000`
(`0.0.0.0`, non il default `127.0.0.1`, altrimenti l'ESP32 non ti raggiunge) e nel portale
metti URL `http://<IP-del-Mac>:8000/update-coordinates`. L'uploader sceglie da solo HTTP o HTTPS
guardando l'URL.

## Perché il firmware in `../esp32/` non funzionava

Utile da tenere a mente quando lo riscrivi:

1. **`getenv()` su ESP32 ritorna sempre vuoto** — non c'è un sistema operativo. Qui il `.env`
   viene letto a compile time da `load_env.py`.
2. **`WiFiClient` su URL `https://`** — Railway è solo HTTPS; serve `WiFiClientSecure`.
3. **`delay(2000)` nel loop** — il buffer UART di default è 256 byte, il NEO-6M ne manda ~500/s:
   la maggior parte delle frasi NMEA andava persa. Qui: loop non bloccante + buffer 4 KB.
4. **Formato data `2026-09-26 14h:03m:21s`** — Pydantic vuole ISO 8601, il server rispondeva 422.
   Qui: `2026-09-26T14:03:21Z` (verificato contro una copia di `PayloadReceived`).
5. **`fix_status` e `track` mai assegnati** — memoria casuale nel JSON. Qui ogni campo ha un default.
6. **`speed` in km/h** — il server ragiona in m/s (soglia 0.3). Qui `gps.speed.mps()`.
7. **`json["api_token"] = api_token`** — direzione invertita (scriveva invece di leggere), e
   `SPIFFS.begin()` senza `true` fallisce su flash mai formattata. Qui: NVS con `Preferences`.

## Debito tecnico consapevole (va bene per un test, non per l'evento)

- `setInsecure()`: TLS cifrato ma senza verifica del certificato del server → nel definitivo
  `setCACert()` con la root CA.
- Token e URL sono dentro il binario: chi ha fisicamente il device può estrarli. Lo stesso vale
  per le password delle reti note: la NVS è in chiaro (esiste la NVS encryption, qui sarebbe eccessiva).
- CPU a 160 MHz invece di 240 (`CPU_FREQ_MHZ` in `config.h`) per consumare meno. 80 MHz consumerebbe
  ancora meno ma triplica l'handshake TLS, che blocca il loop: da provare solo dopo aver misurato
  i tempi `[HTTP] POST ... (xxx ms)`.
- Il POST è bloccante (fino a 8 s di timeout): il buffer da 4 KB lo assorbe, ma intanto display
  e LED restano fermi. Il caso peggiore (DNS morto) è già evitato da `NetProbe`. Nel definitivo
  valuta un task FreeRTOS dedicato all'upload con una coda da 1 elemento — `NetProbe` è già un
  esempio funzionante dello schema "task separato + variabili atomiche".
- La linea è inserita a mano: può non corrispondere al token. Con il modello a silo (un'istanza
  Railway per linea) il server conosce già la sua linea: un endpoint autenticato tipo
  `GET /device-info` che la restituisce eliminerebbe l'errore alla radice.
