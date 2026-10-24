<p align="center">
  <img src="branding/onroute_logo_dark_svg.svg" alt="OnRoute" width="280">
</p>
# Tracker GPS end-to-end con trasmissione live della posizione 

Tracker GPS live per navette eventi. Un Raspberry con modulo GPS (o ESP32) manda le sue coordinate a un server con FastAPI, che la mostra
in una mappa Leaflet (standalone o embedded, localizzate in italiano e in inglese) comprensiva di POI (Punti di interesse). Un pannello di amministrazione (cui si accede dietro autenticazione)
imposta gli orari in cui la posizione viene esposta sulle mappe e i POI. Fuori dagli orari impostati, la chiamata all'endpoint `/coordinates` risponde con 
`off_service`, permettendo al dispositivo di rimanere acceso senza esporre pubblicamente la posizione.

Versione inglese [README.md](README.md)

## Stack hardware
Raspberry Pi (Zero 2 W) + gpsd, Python 3.9, FastAPI, Pydantic v2, Jinja2, Leaflet 1.9. 
Deployed su Railway. La versione ESP32 rimane identica lato server.
HTTP Basic auth per l'amministratore, bearer token per il dispositivo.

##Layout
```
RPiGPS/      # device-side: gpsd reader + HTTP poster
Server/      # FastAPI app, JSON persistence, admin templates + JS
branding/    # logos
```

## Setup e installazione

Server, in locale:

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp Server/.env.example Server/.env   # fill in values
cd Server && uvicorn api:app --reload
```

Compilare: `ADMIN_USERNAME`, `ADMIN_PASSWORD`, `DEVICE_TOKEN`, e `ENV` (`development` mostra `/docs`).
Il pannello admin si trova nell'endpoint `/admin`, la mappa pubblica in `/embed`.

Dispositivo:

```bash
cd RPiGPS
cp .env.example .env   # inserire URL_SITO_GPS + DEVICE_TOKEN
python networking.py
```

Su Mac non c'è `gpsd`, con `ENV=development` il codice legge `mock_gps_coordinates.txt` per questioni di debug.


## Project status
Ancora in corso. Vista la necessità di implementarlo durante un evento che si terrà a fine Ottobre 2025 a Torino, ho consapevolmente scelto di mantenere del debito tecnico a favore di un'implementazione più veloce e possibilmente già funzionale. L'endpoint '/embed', nonostante il nome, è una vera e propria landing page con tag, informazioni e stili hardcoded. Non è una soluzione elegante per un'applicazione a larga scala: questo è esattamente il debito tecnico menzionato prima.

Seguiranno: 
  - Una vera gestione multilinea. Attualmente, per avere più linee è necessario avere due servizi attivi in parallelo e quindi due link diversi (questo spiega anche perché le informazioni sulla landing page si riferiscono anche a una seconda linea che non è visualizzata nel link).
  - Revisione del firmware di ESP32: per rispettare le scadenze e avere un dispositivo user friendly pronto per il deploy a poche settimane dall'evento è stato utilizzato Claude Code in maniera generativa solamente per la cartella '/esp32_test'.
  - Schema KiKad della pcb. Attualmente il prototipo gira su una PCB millefori, con saldatura a stagno e cavi solid wire. Per ottenere un segnale migliore dal modulo gps, è stato aggiunto un condensatore di circa 220uF tra GND e Vin che agisce come filtro passa-basso.
  - File del case progettato con Autodesk Fusion e stampato in 3d fdm.

## Foto:
<img src="help/F08F08CD-6C05-4CDE-A9A9-9BA1D0234396_1_105_c.jpeg)" alt="FOto dell'implementazione su pcb millefori" width="280">


## License

MIT.

Fatto da [andrepadawan](https://github.com/andrepadawan), studente triennale di Computer Engineering al Politecnico di Torino.