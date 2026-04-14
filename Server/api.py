import threading
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel
import os
from pathlib import Path
from dotenv import load_dotenv

from schedule_manager import ScheduleManager

load_dotenv()

templates = Jinja2Templates(Path(__file__).parent / "Templates")

app = FastAPI(
    docs_url="/docs" if os.getenv("ENV") == "development" else None,
    redoc_url="/redoc" if os.getenv("ENV") == "development" else None,
    openapi_url="/openapi.json" if os.getenv("ENV") == "development" else None
)

#coordinates must have the same field of json received from rpi
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

payloadRpi = PayloadReceived()
lastCoordinates = Coordinates()
_Lock = threading.Lock()

#the endpoint to post coordinates to the main site
@app.get("/coordinates")
async def get_coordinates():
    #posts coordinates
    if scheduleManager.check_timetable():
        #sending the coordinates in JSON
        return lastCoordinates

    #Not sending live location: outside of shift hours

@app.get("/admin", response_class=HTMLResponse)
async def admin_page(request: Request):
    #displays admin page
    return templates.TemplateResponse(request=request, name="admin.html")

@app.post("/update-coordinates")
async def update_coordinates(coords: PayloadReceived):
    #receives from Raspberry
    with _Lock:
        lastCoordinates.lon = coords.lon
        lastCoordinates.lat = coords.lat
        lastCoordinates.speed = coords.speed


