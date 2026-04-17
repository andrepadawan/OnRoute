import os
import datetime
import threading
import secrets

from fastapi import FastAPI, Request, Header, HTTPException, Form, Depends, APIRouter
from fastapi.responses import JSONResponse, HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from pydantic import BaseModel
from typing import Annotated
from pathlib import Path
from dotenv import load_dotenv

from schedule_manager import ScheduleManager
from mapManager import MapManager

load_dotenv()

#TO-DO
# -Add functionality to modify button
# -make it pretty
# - aggiungere status LED per vedere quando il servizio è attivo e quando no
# - possibility to add POI via admin page, using Leafleet(?) or bare coordinates


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
#And then we tell the app of a router obj
app.include_router(router)

#--------------------Configuring file location for HTML and CSS----------------
#telling FastAPI where the CSS at
app.mount("/static", StaticFiles(directory=Path(__file__).parent / "static"), name="static" )

#telling Jinja where the HTML at
templates = Jinja2Templates(Path(__file__).parent / "Templates")

#------------------- Custom data types------------------------
#Setting up custom classes for data validation
#coordinates must have the same field of the JSON received from rpi

class PayloadReceived(BaseModel):
    lon: float = 0.0
    lat: float = 0.0
    speed: float = 0.0
    fix_status: int = 0

class Coordinates(BaseModel):
    lon: float = 0.0
    lat: float = 0.0
    speed: float = 0.0
    info: str = "notactive"


scheduleManager = ScheduleManager()
mapManager = MapManager()
lastCoordinates = Coordinates()
_Lock = threading.Lock()


#--------------------- Public endpoints (Map) -------------
@app.get("/embed")
async def embed(request: Request):
    return templates.TemplateResponse(request=request,
                                      name='map.html')


#---------------------Updating coordinates endpoints
#the endpoint to post coordinates to the main site
@app.get("/coordinates")
async def get_coordinates():
    #posts coordinates
    #but only if we are in the authorized time window
    if scheduleManager.check_timetable():
        #sending the coordinates in JSON
        with _Lock:
            return lastCoordinates
    else:
        with _Lock:
            lastCoordinates.info = "notactive"
            return lastCoordinates

    #Not sending live location: outside of shift hours

@app.post("/update-coordinates")
async def update_coordinates(coords: PayloadReceived, authorization: str = Header(None)):
    #receives from Raspberry
    if authorization != f"Bearer {os.getenv('DEVICE_TOKEN')}":
        raise HTTPException(status_code=401, detail="Unauthorized")
    with _Lock:
        lastCoordinates.lon = coords.lon
        lastCoordinates.lat = coords.lat
        lastCoordinates.speed = coords.speed
        lastCoordinates.info = "active"

#------------------administration section -----------------
# Router works here: its dependency with auth allows modifications only to be
# performed by authorized personnel

@router.get("/admin", response_class=HTMLResponse)
async def admin_page(request: Request):
    #displays admin page
    data = scheduleManager.retrieve_shifts()
    #data è una lista di Timeshifts{id, start, end}
    #giving back a UI with Jinja, passing the shifts
    return templates.TemplateResponse(request=request,
                                      name="admin.html",
                                      context={"data":data})


@router.post("/admin/delete")
async def delete_shift(id: str = Form()):
    #Form is used to pass data from HTML to code here. Important though: fields must have
    #the same name! id
    scheduleManager.delete_shift(id)
    return RedirectResponse("/admin", status_code=303)


@router.post("/admin/add")
async def add_shift(start_date: str = Form(...),
                    start_time: str = Form(...),
                    end_date: str = Form(...),
                    end_time: str = Form(...)):
    start = datetime.datetime.fromisoformat(f"{start_date}T{start_time}")
    end = datetime.datetime.fromisoformat(f"{end_date}T{end_time}")
    scheduleManager.insert_shift(start,end)
    return RedirectResponse("/admin", status_code=303)

