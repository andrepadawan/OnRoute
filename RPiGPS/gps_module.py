#Modules
import time
import gps
import threading
import logging


logger = logging.getLogger("gps_module")
logging.basicConfig(format="%(asctime)s — %(name)s — %(levelname)s — %(message)s",
    level=logging.INFO)
class GpsReader: 

    def __init__(self):
        #Here I have several variables:
        #The most important in order to acquire the position are:
        self.latitude = 0.0
        self.longitude = 0.0
        self.speed = 0.0
        self.track = None #track described from documentation as drift from true North
        #Can be none if module cannot calculate it -> 0.0 as std would be wrong
        #Quality of the signal:
        self.fix_status = gps.STATUS_FIX

        #Management variable: lock, threading and event management
        self._lock = threading.Lock()
        self._thread = None
        self._stop_event = threading.Event()
        self._connected_to_gps = False
    
    def start(self):

        #Checking no other gps thread is alive, if positive return -> no effect
        if self._thread is not None and self._thread.is_alive():
            return
        
        #Clearing the way for a new thread 
        self._stop_event.clear()

        self._thread = threading.Thread(
            target=self.gps_loop, 
            name = "gps_thread",
            daemon = True)
        
        self._thread.start()
        logger.info("Thread GPS avviato")
        

    def stop(self):
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=10)
        
        logger.info("Thread GPS terminato")

    def write_coordinates(self, report):
        #Acquiring lock in order to write correctly without concurrency issues
        with self._lock:
            #For the sake of this project no other information is necessary
            self.latitude = report["lat"] 
            self.longitude = report["lon"]
            self.speed = report["speed"]
            self.fix_status = report["mode"]
            self.track = report["track"]
            #showing coordinates for debug purposes
            logger.info(f"Coordinates: lat: {self.latitude}, lon: {self.longitude}, fix:{self.fix_status}, track:{self.track}")

    def gps_loop(self):

        """
        What should I do here?
        - study code in order to manage dropped connections (try: catch blocks)
        - thread should continuously cycle to get the position (inner cycle perhaps)
        - 
        """

        #Continuing connecting until stop event (external)
        while not self._stop_event.is_set():

            #establishing a connection with gpsd daemon
            session = gps.gps(mode=gps.WATCH_ENABLE)
            self._connected_to_gps = True

            #trying to connect
            try:
                #while connected, update as many times TPV report
                while 0 == session.read():
                    
                    #generating my report
                    report = session.next()
                    self.evaluate_report(report)

            #except falls here in case of dropped connection
            except StopIteration:
                self._connected_to_gps = False
                logger.warning("Connessione persa. Riconnetto...")
            
            except ConnectionRefusedError:
                logger.warning("Connessione rifiutata")

            except Exception as e:
                self._connected_to_gps = False
                logger.warning(f"Errore generico: {e}")

            self._stop_event.wait(3)
                    
    def evaluate_report(self, report):

        #Checking if we have a valid TPV
        if report["class"] == "TPV":

            #Valid TPV report! Now let's see if we have enough satellites
            if report["mode"] >= gps.MODE_2D:
                
                self.write_coordinates(report)
        else:
            return
