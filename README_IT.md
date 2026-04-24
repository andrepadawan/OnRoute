<p align="center">
  <img src="branding/onroute_logo_dark_svg.svg" alt="OnRoute" width="280">
</p>
# Tracker GPS end-to-end con trasmissione live della posizione 

Tracker GPS live per navette eventi. Un Raspberry con modulo GPS manda le sue coordinate a un server con FastAPI, che la mostra
in una mappa Leaflet (standalone o embedded) comprensiva di POI (Punti di interesse). Un pannello di amministrazione (cui si accede dietro autenticazione)
imposta gli orari in cui la posizione viene esposta sulle mappe e i POI. Fuori dagli orari impostati, la chiamata all'endpoint `/coordinates` risponde con 
`notactive`, permettendo al dispositivo di rimanere acceso senza esporre la posizione.

Versione inglese [README.md](README.md)

## Stack hardware
Raspberry Pi (Zero 2 W) + gpsd, Python 3.9, FastAPI, Pydantic v2, Jinja2, Leaflet 1.9. 
Deployed on PythonAnywhere.
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
Ancora in corso.
Seguiranno:

- Miglioramenti UI (il pannello admin sembra un blog del 1997)
- ruff + PEP 8 cleanup 
- LED su Pi (attivo / disattivato)
- Led su admin accanto al turno attivo
- Test per le classi python
- App android

## License

MIT.

Fatto da [andrepadawan](https://github.com/andrepadawan), studente triennale di Computer Engineering al Politecnico di Torino.