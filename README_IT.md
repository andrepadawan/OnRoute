
# Tracker GPS end-to-end con trasmissione live della posizione 

Tracker GPS versatile, con la possibilità di programmare gli orari in cui trasmettere la posizione e di mostrarla su una
mappa (standalone o embedded) comprensiva di POI, cursore
## Stack hardware
Raspberry Pi Zero 2W o hardware più performante, Python 3.9, GPS, modulo gps, display, gpsd, flask, requests, fastapi.

## Architettura
RPI(Modulo gps) -> Python(gps_module e networking.py) -> Server(api.py, mapManager.py, scheduleManager.py)
-> mappa Web 

### Server
La cartella Server contiene files con lo scopo di gestire le varie richieste get- e post-, cioé di gestire le coordinate
in arrivo da RPiGPS, fornirle agli user, controllare l'orario di operatività e permettere l'amministrazione. 
Per quest'ultima viene anche fornito un pannello Ui, /admin, a cui si accede tramite autenticazione e che permette, per via grafica
di aggiungere turni e di eliminarli.

L'implementazione dell'autenticazione è avvenuta tramite secrets, mentre per garantire che **tutte** le operazioni di amministrazione avvenissero solo dietro autenticazione si è scelto APIRouter, 
che permette di impostare una dipendenza: su un **intero** gruppo di endpoint.
## Setup and installation

## Project status
In progress