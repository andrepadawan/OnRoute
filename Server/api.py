import datetime
import threading
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse, HTMLResponse, RedirectResponse
from fastapi import Form
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
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

app.mount("/static", StaticFiles(directory=Path(__file__).parent / "static"), name="static" )


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
        with _Lock:
            return lastCoordinates

    #Not sending live location: outside of shift hours

@app.get("/admin", response_class=HTMLResponse)
async def admin_page(request: Request):
    #displays admin page
    data = scheduleManager.retrieve_shifts()
    return templates.TemplateResponse(request=request,
                                      name="admin.html",
                                      context={"data":data})

@app.post("/update-coordinates")
async def update_coordinates(coords: PayloadReceived):
    #receives from Raspberry
    with _Lock:
        lastCoordinates.lon = coords.lon
        lastCoordinates.lat = coords.lat
        lastCoordinates.speed = coords.speed


@app.post("/admin/delete")
async def delete_shift(id: str = Form()):
    scheduleManager.delete_shift(id)
    return RedirectResponse("/admin", status_code=303)

@app.post("/admin/add")
async def add_shift(start_date: str = Form(...),
                    start_time: str = Form(...),
                    end_date: str = Form(...),
                    end_time: str = Form(...)):
    start = datetime.datetime.fromisoformat(f"{start_date}T{start_time}")
    end = datetime.datetime.fromisoformat(f"{end_date}T{end_time}")
    scheduleManager.insert_shift(start,end)
    return RedirectResponse("/admin", status_code=303)
