import requests
import os
import json
import threading
import logging
import time
from dotenv import load_dotenv
from gps_module import GpsReader

load_dotenv()

logger = logging.getLogger()
logging.basicConfig(format="%(asctime)s — %(name)s — %(levelname)s — %(message)s",
    level=logging.INFO)

class Networking:
    def __init__(self):
        self.url_site = os.getenv("URL_SITO_GPS")
        self.device_token = os.getenv("DEVICE_TOKEN")
        if os.getenv("ENV") != "development":
            self.gps_module = GpsReader()
        else:
            self.gps_module = None
        self._thread = None
        self._lock = threading.Lock()
        self._stop_event = threading.Event()

    def start(self):

        if self._thread is not None:
            logger.info("Thread networking già in esecuzione")
            return
        self._stop_event.clear()

        self._thread = threading.Thread(
            target=self.send_coord_loop,
            name = "Networking thread",
            daemon=True)
        
        self._thread.start()
        logger.info("Networking thread creato")

    def stop(self):
        self._stop_event.set()

        if self._thread:
            self._thread.join(timeout=10)

    def get_payload(self) -> dict:
        if os.getenv("ENV") != "development":
            #So I'm in production
            with self._lock:
                coord = {
                    "lon":self.gps_module.longitude,
                    "lat":self.gps_module.latitude,
                    "speed":self.gps_module.speed,
                    "fix_status":self.gps_module.fix_status,
                    "track":self.gps_module.track
                }
            return coord
        else:
            with open("mock_gps_coordinates.txt", "r") as f:
                return json.loads(f.read())


    def send_coord_loop(self):
        
        #main loop to manage sessions and sending data
        session = requests.session()

        session.headers.update(
            #Standard sentence
            {"Authorization": f"Bearer {self.device_token}",
             #Content type ->   automatically set as app.../json if json=dict in requests.post
              "Content-Type": "application/json"})
        
        while not self._stop_event.is_set():
            try:
                #requests (session) automatically serializes a dict into json string
                session.post(self.url_site, json = self.get_payload(), timeout=5)
            
            except requests.exceptions.ConnectionError:
                
                logger.info("Connection lost")

            except requests.exceptions.Timeout:

                logger.info("Timeout error")

            except Exception as e:

                logger.info(f"Errore imprevisto: {e}")
            
            #Waiting using .wait so our function still responds to stop events
            self._stop_event.wait(10)

        session.close()


#DEBUG only
if __name__ == "__main__":
    sender = Networking()
    sender.start()

    try:
        while True:
            time.sleep(2)

    except KeyboardInterrupt:
        print("\nStop.")

    finally:
        sender.stop()