from time import time
import datetime
import threading
import uuid
from typing import List
from pydantic import BaseModel, Field, TypeAdapter

"""TO-DO:
    #insert json file for schedules -> timetable.json -> DONE
    #check timetable
"""

class Timeshifts(BaseModel):
    #generating unique id: useful for deleting/modifying shifts
    id: str = Field(default_factory = lambda : str(uuid.uuid4()))

    #actual information
    start: datetime.datetime
    end: datetime.datetime

_adapter = TypeAdapter(List[Timeshifts])


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


    def check_timetable(self) -> bool:
        #isolating time we are processing information
        self.now = datetime.datetime.now()
        table = self._read_json_file()

        #not very smart ik, will be updated
        for t in table:
            if (self.now > t.start and self.now < t.end):
                return True
        return False

    def _read_json_file(self) -> List[Timeshifts]:

        try:
            with open("timetable.json", "r") as f:
                raw = f.read()
                if not raw.strip():
                    return []
                return _adapter.validate_json(raw)
        except FileNotFoundError:
            return []


    def _write_json_file(self, data: List[Timeshifts]):
        with open("timetable.json", "wb") as f:
            f.write(_adapter.dump_json(data))

    def insert_shift(self, start: datetime, end: datetime):
        #creating a structure table to hold list of Timeshifts data type
        table = self._read_json_file()
        #using list methods to add a new shift to the file
        table.append(Timeshifts(id=str(uuid.uuid4()), start=start, end=end))
        #rewriting
        self._write_json_file(table)

    def delete_shift(self, id: str):
        raw_json_shifts = self._read_json_file()
        table = [Timeshifts.model_validate(t) for t in raw_json_shifts]
        table = [t for t in table if t.id != id]
        self._write_json_file(table)


    def retrieve_shifts(self):
        return self._read_json_file()



# For debug purposes only, for this module
if __name__ == "__main__":

    scheduleManager = ScheduleManager()
    print(scheduleManager.check_timetable())
