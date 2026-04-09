from time import time
import datetime
import threading
from pydantic import BaseModel
"""TO-DO:
    #insert json file for schedules -> timetable.json
"""

class Timeshifts(BaseModel):
    #generating unique id: useful for deleting/modifying shifts
    id: str = Field(default_factory = lambda : str(uuid.uuid4()))

    #actual information
    start: datetime.datetime
    end: datetime.datetime

class ScheduleManager:
    def __init__(self):
        #coordinates
        self.lat = 0.0
        self.lon = 0.0
        self.speed = 0.0
        self._Lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None
        self.now = datetime.datetime.now()

    def start(self):
        #usual check if not already running
        if self._thread is not None and self._thread.is_alive():
            return

        self._stop.clear()
        self._thread = threading.Thread(target = self.check_timetable(),
            name = "ScheduleManager_thread",
            daemon = True)


    def stop(self):
        self._stop.set()

        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout = 5)


    def check_timetable(self):
        self.now = datetime.datetime.now()
        pass


    def insert_shift(self, start: datetime, end: datetime):

        shift = Timeshifts(start = start, end = end)

        with open("timetable.json", "w") as f:
            f.write(shift.model_dump_json())



    def delete_shift(self, id: str):
        with open("timetable.json", "r") as f:
            raw_json_shifts = f.read()

            table = [Timeshifts.model_validate(t) for t in raw_json_shifts]
            table = [t for t in table if t.id != id]

        with open("timetable.json", "w") as f:
            f.write(shift.model_dump_json())