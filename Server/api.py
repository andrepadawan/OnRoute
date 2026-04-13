import threading
from fastapi import FastAPI
from fastapi.responses import JSONResponse
from pydantic import BaseModel
import os
from dotenv import load_dotenv
from schedule_manager import ScheduleManager

load_dotenv()

app = FastAPI(
    docs_url="/docs" if os.getenv("ENV") == "development" else None,
    redoc_url="/redoc" if os.getenv("ENV") == "development" else None,
    openapi_url="/openapi.json" if os.getenv("ENV") == "development" else None
)

#coordinates must have the same field of json received from rpi
class PayloadReceived(BaseModel):
    lon: 0.0
    lat: 0.0
    speed: 0.0
    fix_status: int

class Coordinates(BaseModel):
    lon: 0.0
    lat: 0.0
    speed: 0.0
    info: "notactive"

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

@app.get("/admin")
async def admin_page():
    #displays admin page
    pass

@app.post("/update-coordinates")
async def update_coordinates(coords: PayloadReceived):
    #receives from Raspberry
    with _Lock:
        lastCoordinates.lon = coords.lon
        lastCoordinates.lat = coords.lat
        lastCoordinates.speed = coords.speed


