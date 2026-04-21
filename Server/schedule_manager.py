from time import time
import datetime
import threading
import uuid
from typing import List
from pathlib import Path
from pydantic import BaseModel, Field, TypeAdapter

"""
    TO-DO:
    #insert json file for schedules -> timetable.json -> DONE
    #check timetable -> DONE
    #right now thread and lock are not necessary, 
        function check is not yet adapted to a thread loop
    #return shift in chronological order
"""
TIMETABLE_FILE = Path(__file__).parent / "timetable.json"

class Timeshifts(BaseModel):
    #generating unique id: useful for deleting/modifying shifts
    id: str = Field(default_factory = lambda : str(uuid.uuid4()))

    #actual information
    start: datetime.datetime
    end: datetime.datetime

_adapter = TypeAdapter(List[Timeshifts])


class ScheduleManager:
    def __init__(self):
        self._Lock = threading.Lock()
        self.now = datetime.datetime.now()


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
            with open(TIMETABLE_FILE, "r") as f:
                raw = f.read()
                if not raw.strip() or raw == "null":
                    return []
                return _adapter.validate_json(raw)
        except FileNotFoundError:
            return []


    def _write_json_file(self, data: List[Timeshifts]):
        with open(TIMETABLE_FILE, "wb") as f:
                f.write(_adapter.dump_json(data))


    def insert_shift(self, start: datetime, end: datetime):
        #creating a structure table to hold list of Timeshifts data type
        #check condition: start<end otherwise swap
        start, end = self._normalize_order(start, end)
        table = self._read_json_file()
        #using list methods to add a new shift to the file
        table.append(Timeshifts(start=start, end=end))
        table = self.order_shifts(table)
        #rewriting
        self._write_json_file(table)

    def modify_shift(self, id: str, start: datetime, end: datetime):
        start, end = self._normalize_order(start, end)

        table = self._read_json_file()
        for elem in table:
            if elem.id == id:
                elem.start = start; elem.end = end;
                break
        self._write_json_file(self.order_shifts(table))

    def _normalize_order(self, start: datetime.datetime, end: datetime.datetime) -> (datetime.datetime, datetime.datetime):
        if start>=end:
            temp = end
            end = start
            start = temp
        return start, end


    def delete_shift(self, id: str):
        table = self._read_json_file()
        table = [t for t in table if t.id != id]
        table = self.order_shifts(table)
        self._write_json_file(table)


    def order_shifts(self, data: List[Timeshifts]) -> List[Timeshifts]:
        #Ordering items from most recent to last
        table = data.copy()
        table.sort(key = lambda t: t.start, reverse = True)
        return table


    def retrieve_shifts(self) -> List[Timeshifts]:
        return self._read_json_file()


    def getIsoFormat(self, start_date: str, start_time: str, end_date: str, end_time: str)\
            -> (datetime.datetime, datetime.datetime):
        start = datetime.datetime.fromisoformat(f"{start_date}T{start_time}")
        end = datetime.datetime.fromisoformat(f"{end_date}T{end_time}")
        return start, end

# For debug purposes only, for this module
if __name__ == "__main__":

    scheduleManager = ScheduleManager()
    scheduleManager.insert_shift(datetime.datetime(2021, 5, 5), datetime.datetime(2021, 5, 9))
    print(scheduleManager.retrieve_shifts())
