import os
import threading
import time
from dotenv import load_dotenv
import serial
import logging
from dataclasses import dataclass, fields
from typing import Optional, get_type_hints

from RPiGPS.backend.Coordinates import Coordinates

logger = logging.getLogger("gps_at")
logging.basicConfig(format="%(asctime)s — %(name)s — %(levelname)s — %(message)s",
    level=logging.INFO)

@dataclass(frozen=True, slots=True)
class AtGNGGA():
    #classe custom per fare un parsing corretto di AT, proprietario di SIMCOM
    '''@page 227 AT command manual ->
    +CGNSINF: <GNSS run status>,<Fix status>,<UTC date &
    Time>,<Latitude>,<Longitude>,<MSL Altitude>,<Speed Over
    Ground>,<Course Over Ground>,<Fix
    Mode>,<Reserved1>,<HDOP>,<PDOP>,<VDOP>,<Reserved2>,<GN
    SS Satellites in View>,<GNSS Satellites Used>,<GLONASS Satellites
    Used>,<Reserved3>,<C/N0 max>,<HPA>,<VPA>
    OK'''
    #Mi serve questa custom class in modo da poter fare parsing tranquillo quando leggo dalla seriale
    #con gli at commands. In futuro sarà semplice leggere un nuovo valore, perché c'è già struttura dati

    gnss_status: int
    fix_status: int
    utc_datetime: str
    latitude: float
    longitude: float
    msl_altitude: float
    speed_og: float
    course_og: float
    fix_mode: int
    reserved_1: str
    HDOP: float
    PDOP: float
    VDOP: float
    reserved_2: str
    gnss_sat_view: int
    gnss_sat_used: int
    glonass_sat_used: int
    reserved_3: str
    c_no_max: int
    HPA: int
    VPA: int


    @classmethod
    def csv_parts(cls, parts: list[str]) -> "AtGNGGA":
        names = [f.name for f in fields(cls)]
        types = get_type_hints(cls)
        if len(parts) != len(names):
            raise ValueError(f"Expected {len(names)} arguments, got {len(parts)}")

        def cast(t, v):
            return t(v) if v else t()  # cioé int() = 0, str()="", sono safe-defaults
        kwargs = {n: cast(types[n],(p)) for n,p in zip (names, parts)}
        return cls(**kwargs)


class AtReader():

    def __init__(self):
        self.coordinates : Coordinates | None = None

        self._lock = threading.Lock()
        self._thread = None
        self._stop_event = threading.Event()
        self._connected_to_gps = False

    def start(self):
        if self._thread is None or not self._stop_event.is_set():
            self._thread = threading.Thread(
                target = self.gps_loop, name="gps_at_thread", daemon=True)
            self._thread.start()
            logger.info("Gps thread started")

    def stop(self):
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=10)
        logger.info("Gps thread stopped")


    def get_coordinates(self) -> Coordinates:
        with self._lock:
            return self.coordinates


    def report(self, line):
        if(os.getenv("ENV")=="development"):
            print(line) #for debug purposes
        #checking first
        if not line.startswith(b'+CGNSINF:'):
            return None
        payload = line.removeprefix(b'+CGNSINF:').split(b",")
        parts = [p.strip().decode() for p in payload]
        with self._lock:
            self.coordinates = self.field_extractor(AtGNGGA.csv_parts(parts))

    def field_extractor(self, raw: AtGNGGA) -> Coordinates:
        return Coordinates(
            longitude = raw.longitude,
            latitude = raw.latitude,
            speed = raw.speed_og,
            fix_status = raw.fix_status,
            track = raw.course_og, #at equivalent of "track", float 0-360deg
            time_of_acquisition = raw.utc_datetime
        )


    def gps_loop(self):
        while not self._stop_event.is_set():
            #inside di external while so we can manage reconnections
            ser = serial.Serial("/dev/serial0", timeout=2, baudrate=115200) #potrei mettere 0 visto che sono in un thread separato, però
            logger.info(f"Serial port created")
            ser.write(b'AT\r\n')

            if (ser.read_until('OK\r\n')): #Prima risposta, perché il modulo potrebbe non esserci proprio
                logger.info(f"AT module opened and responding successfully")
                ser.write(b'AT+CGNSPWR=1\r\n')
                logger.info(f"Powering on GPS\r\n")
                if(ser.read_until(b'OK\r\n')):
                    try:
                        while ser.is_open and not self._stop_event.is_set():
                            ser.write(b'AT+CGNSINF\r')
                            logger.info(f"Sent coordinates request to AT module")
                            line = ser.readline()
                            print(repr(line))
                            self.report(line)
                            self._stop_event.wait(2) #asks every 2 seconds
                    except serial.SerialException as e:
                        logger.info(f"Errors reading serial port: {ser.name}, {e} ")

                    finally:
                        ser.write(b'AT+CGNSPWR==0')
                        logger.info(f"GPS powered off")
                        ser.close()  # Outside whilw loop, last thing done here
                        logger.info(f"Serial port with gps closed")
                else:
                    self._stop_event.wait(3)



