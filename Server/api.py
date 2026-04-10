from fastapi import FastAPI
import os
from schedule_manager import ScheduleManager

app = FastAPI(
    docs_url="/docs" if os.getenv("ENV") == "development" else None,
    redoc_url="/redoc" if os.getenv("ENV") == "development" else None,
    openapi_url="/openapi.json" if os.getenv("ENV") == "development" else None
)

scheduleManager = ScheduleManager()

@app.get("/")
async def root():
    pass

#the endpoint to post coordinates to the main site
@app.get("/coordinates")
async def get_coordinates():
    #posts coordinates
    pass

@app.get("/admin")
async def admin_page():
    #displays admin page
    pass

@app.post("/update-coordinates")
async def update_coordinates():
    #receives from Raspberry
    pass


