from time import time
import datetime
import threading

"""TO-DO:
    #insert json file for schedules -> timetable.json
"""


class ScheduleManager:
    def __init__(self):
        #coordinates
        self.lat = 0.0
        self.lon = 0.0
        self.speed = 0.0
        self._Lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None

    def start(self):

        if self._thread is not None and self._thread.is_alive():
            return

        #complete



