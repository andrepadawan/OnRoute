
# GPS tracker and Web transmission

GPS tracker for events: sends GPS coordinates live to a web server, shows the position in an html frame complete with POI and tracker position, completed with a ui (gps side) and administration panel (server side).

## Stack hardware
Raspberry Pi Zero 2W or more powerful hardware, Python 3.9, GPS, cellular module, display, gpsd, flask, requests.

## Architecture
[RPi GPS module] -> Python(gps_module and networking.py) -> Server -> Web Map

### Server
La cartella Server contiene files con lo scopo di gestire le varie richieste get- e post-, cioé di gestire le coordinate
in arrivo da RPiGPS, fornirle agli user, controllare l'orario di operatività e permettere l'amministrazione. 
Per quest'ultima viene anche fornito un pannello Ui, /admin, a cui si accede tramite autenticazione e che permette, per via grafica
di aggiungere turni e di eliminarli.

L'implementazione dell'autenticazione è avvenuta tramite: <br>
<code>
import secrets <br>
from fastapi.security import HTTPBasic, HTTPBasicCredentials<br>
</code><br>
Mentre per garantire che **tutte** le operazioni di amministrazione avvenissero solo dietro autenticazione si è scelto APIRouter, 
che permette di impostare una dipendenza:
<code>router = APIRouter(dependencies=[Depends(authentication)])
</code>
su un **intero** gruppo di endpoint.
## Setup and installation

## Project status
In progress