import os
from datetime import datetime, timezone, timedelta
import threading
import secrets
import babel

from fastapi import FastAPI, Request, Header, HTTPException, Form, Depends, APIRouter
from fastapi.responses import JSONResponse, HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from pydantic import BaseModel, AwareDatetime
from typing import Annotated, Optional
from pathlib import Path
from dotenv import load_dotenv
from babel.support import Translations
from enum import Enum


from schedule_manager import ScheduleManager
from mapManager import MapManager, PointOfInterest

load_dotenv()

#TO-DO
# -Add functionality to modify button
# -make it pretty
# - aggiungere status LED per vedere quando il servizio è attivo e quando no
# - possibility to add POI via admin page, using Leafleet(?) or bare coordinates
# - implementing modify-poi


#Documentation will be unavailable in production for safety reasons: reading the .env
app = FastAPI(
    docs_url="/docs" if os.getenv("ENV") == "development" else None,
    redoc_url="/redoc" if os.getenv("ENV") == "development" else None,
    openapi_url="/openapi.json" if os.getenv("ENV") == "development" else None
)

#Creating HTTBasic instance: will be used for authentication
security = HTTPBasic()

def authentication(credentials: Annotated[HTTPBasicCredentials, Depends(security)]):
    #Annotated links  metadata to credentials but does not change type of variable.
    #Basically asks to treat data with security
    _username = os.getenv("ADMIN_USERNAME")
    is_correct_username = secrets.compare_digest(_username, credentials.username)
    _password = os.getenv("ADMIN_PASSWORD")
    is_correct_password = secrets.compare_digest(_password, credentials.password)
    if not (is_correct_password and is_correct_username):
        raise HTTPException(status_code=401, detail="Unauthorized - Credentials not correct",
                            headers={"WWW-Authenticate": "Basic"})
    return credentials.username

#router will manage a single authentication across a group of endpoints (admin endpoints).
router = APIRouter(dependencies=[Depends(authentication)])

#--------------------Configuring file location for HTML and CSS----------------
#telling FastAPI where the CSS & JS at
app.mount("/static", StaticFiles(directory=Path(__file__).parent / "static"), name="static" )
app.mount("/branding", StaticFiles(directory=Path(__file__).parent.parent / "branding"), name="branding" )


#telling Jinja where the HTML at
templates = Jinja2Templates(Path(__file__).parent / "Templates", extensions=["jinja2.ext.i18n"])
translations = Translations.load(Path(__file__).parent / 'translations', ['it'])
templates.env.install_gettext_translations(translations)

#------------------- Custom data types------------------------
#Setting up custom classes for data validation
#coordinates must have the same field of the JSON received from rpi

class PayloadReceived(BaseModel):
    longitude: float = 0.0
    latitude: float = 0.0
    speed: float = 0.0
    fix_status: int = 0
    track: float = 0.0
    time_of_acquisition: AwareDatetime

class Coordinates(BaseModel):
    lon: float = 0.0
    lat: float = 0.0
    speed: float = 0.0
    track: Optional[float] = None
    is_active: bool = False
    timestamp: Optional[datetime] = None

#creating status enum to safely protect coordinates from being exposed online
#and to be more precise during diagnostic and debug

class Status(str, Enum):
    LIVE = "live"
    STALE = "stale"
    NO_DATA = "no_data"
    OFF_SERVICE = "off_service"

class PayloadSent(BaseModel):
    status: Status
    payload: Optional[Coordinates] = None

scheduleManager = ScheduleManager()
mapManager = MapManager()
lastCoordinates = Coordinates()
_Lock = threading.Lock()


#--------------------- Public endpoints (Map) -------------
@app.get("/embed")
async def embed(request: Request):
    poi_json = mapManager.returnPrimitiveTypeList()
    payload = get_payload_with_status_check()
    return templates.TemplateResponse(request=request,
                                      name='embed_map.html',
                                      context={"poi_json":poi_json, "coords":payload.model_dump(mode='json')})


#the endpoint to post coordinates to the main site
#only returns JSON object with coordinates
#(thinking about deprecating this branch)
@app.get("/coordinates", response_model=PayloadSent)
async def get_coordinates():
    #posts coordinates
    #but only if we are in the authorized time window
    return get_payload_with_status_check()

#---------------------Updating coordinates endpoints

@app.post("/update-coordinates")# function used by gps receiver to post coordinates 
async def update_coordinates(coords: PayloadReceived, authorization: str = Header(None)):
    #receives from Raspberry or ESP32
    if not secrets.compare_digest(authorization, f"Bearer {os.getenv('DEVICE_TOKEN')}"):
        raise HTTPException(status_code=401, detail="Unauthorized")
    with _Lock:
        lastCoordinates.lon = coords.longitude
        lastCoordinates.lat = coords.latitude
        lastCoordinates.speed = coords.speed
        """ Consideration: at low speeds or at a stop the orientation
         produces errors. Only registering the track if the speed is higher than 0.5km/h
         (0,14 m/s -> rounded and raised at 0,3 m/s for gps jitter), otherwise keep the previous data.
        """
        if coords.speed > 0.3 and coords.track is not None:#m/s
            lastCoordinates.track = coords.track
        lastCoordinates.timestamp = coords.time_of_acquisition

#------------------administration section -----------------
# Router works here: its dependency with auth allows modifications only to be
# performed by authorized personnel

@router.get("/admin", response_class=HTMLResponse)
async def admin_page(request: Request):
    #displays admin page
    shifts = scheduleManager.retrieve_shifts()
    poi = mapManager.readPointsOfInterest()
    #shifts is list of Timeshifts{id, start, end}
    #giving back a UI with Jinja, passing the shifts
    poi_json = mapManager.returnPrimitiveTypeList()
    #necessary because the js in adminhtml uses, with Jinja, {{ | tojson }}, that relies on python's base
    #model, not pydantic (PointOfInterests), giving errors
    is_active: Bool = scheduleManager.check_timetable()
    return templates.TemplateResponse(request=request,
                                      name="admin.html",
                                      context={"shifts":shifts, "poi":poi, "poi_json":poi_json, "is_active":is_active})


@router.post("/admin/delete-shift")
async def delete_shift(id: str = Form()):
    #Form is used to pass data from HTML to code here. Important though: fields must have
    #the same name! id
    scheduleManager.delete_shift(id)
    return RedirectResponse("/admin", status_code=303)


@router.post("/admin/add-shift")
async def add_shift(start_date: str = Form(...),
                    start_time: str = Form(...),
                    end_date: str = Form(...),
                    end_time: str = Form(...)):
    #def getIsoFormat(self, start_date: str, start_time: str, end_date: str, end_time: str) -> (datetime.datetime, datetime.datetime):
    scheduleManager.insert_shift(*scheduleManager.getIsoFormat(start_date, start_time, end_date, end_time))
    return RedirectResponse("/admin", status_code=303)

@router.post("/admin/modify-shift")
async def modify_shift(start_date: str = Form(...),
                       start_time: str = Form(...),
                       end_date: str = Form(...),
                       end_time: str = Form(...),
                       id: str = Form(...)):
        start, end = scheduleManager.getIsoFormat(start_date, start_time, end_date, end_time)
        scheduleManager.modify_shift(id, start, end)
        return RedirectResponse("/admin", status_code=303)


@router.post("/admin/add-poi")
async def add_poi(name: str = Form(...),
                  lat: float = Form(...),
                  lon: float = Form(...)):
    poi = PointOfInterest(name=name, lat=lat, lon=lon)
    mapManager.addPointsOfInterest(poi)
    return RedirectResponse("/admin", status_code=303)

@router.post("/admin/modify-poi")
async def modify_poi(name: str = Form(...),
                  lat: float = Form(...),
                  lon: float = Form(...),
                     id: str = Form(...)):
    #Implement
    mapManager.modifyPointsOfInterest(id, name, lat, lon)
    return RedirectResponse("/admin", status_code=303)

@router.post("/admin/delete-poi")
async def delete_poi(id: str = Form(...)):
    mapManager.deletePointsOfInterest(id)
    return RedirectResponse("/admin", status_code=303)

#And then we tell the app of a router obj
app.include_router(router)
#

def is_service_available() -> bool:
    return scheduleManager.check_timetable()

def check_for_stale_data(serverTime: datetime, coordinatesTime: datetime) -> bool:
    time_diff: timedelta = serverTime-coordinatesTime
    FIX_EXPIRATION = timedelta(minutes=5)
    return time_diff > FIX_EXPIRATION

def get_payload_with_status_check() -> PayloadSent:
   #Am I live? Should I be offline?
   if(is_service_available()):
       #I should be online, so:
       #Am i with no data?
       if(lastCoordinates.timestamp == None):
           return PayloadSent(status=Status.NO_DATA, payload=None)
           #Am i with an old gps fix?
       if(not check_for_stale_data(datetime.now(timezone.utc), lastCoordinates.timestamp)):
           #No: I should be live and I have fresh coordinates
           return PayloadSent(status=Status.LIVE, payload=lastCoordinates)
       else:
            #I should be live, but my gps_fix is too old!
            return PayloadSent(status = Status.STALE, payload=None)
   else:
       return PayloadSent(status=Status.OFF_SERVICE, payload=None)
