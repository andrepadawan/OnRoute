#pragma once
#include <stdio.h>
//For snprintf()
#include <ArduinoJson.h>

// ============================================================================
//  Coordinates: mirror di PayloadReceived in Server/api.py
//
//    class PayloadReceived(BaseModel):
//        longitude: float = 0.0
//        latitude: float = 0.0
//        speed: float = 0.0
//        fix_status: int = 0
//        track: float = 0.0
//        time_of_acquisition: datetime = None
//
//  Tutti i campi hanno un valore iniziale per evitare leak memoria casuale
// ============================================================================
struct Coordinates {
  double longitude = 0.0;      // gradi decimali, WGS84
  double latitude  = 0.0;      // gradi decimali
  double speed     = 0.0;      // m/s — stessa unità di gpsd il server usa soglia 0.3 
  int    fix_status = 0;       // semantica gpsd "mode": 0 sconosciuto, 1 no fix, 2 = 2D, 3 = 3D
  double track     = 0.0;      // rotta in gradi 0-360 rispetto N
  // ISO 8601 UTC "2026-09-26T14:03:21Z" = 20 char + '\0'. Il buffer è più
  // largo del minimo così snprintf non tronca nemmeno con valori assurdi.
  char   time_of_acquisition[32] = "";

  // Il server fa parsing con Pydantic -> vuole ISO 8601, non un formato inventato.
  void setTimeUtc(unsigned year, unsigned month, unsigned day,
                  unsigned hour, unsigned minute, unsigned second) {

/* 
Syntax
snprintf(str, size, format, ...);
Parameters
str: It is a pointer to the buffer.
size: It is the maximum number of bytes (characters) that will be written to the buffer.
format: C string that contains a format string that follows the same specifications 
as format in printf.
(...): The optional ( …) arguments are just the string 
formats like (“%d”, myint) as seen in printf. */

    snprintf(time_of_acquisition, sizeof(time_of_acquisition),
             "%04u-%02u-%02uT%02u:%02u:%02uZ",
             year, month, day, hour, minute, second);
  }

  // Riempie un oggetto JSON già esistente:
  void toJson(JsonObject obj) const {
    //const perché non cambia dati ma ricopia. È una "promessa" utile per eivitare bug
    obj["longitude"]  = longitude;
    obj["latitude"]   = latitude;
    obj["speed"]      = speed;
    obj["fix_status"] = fix_status;
    obj["track"]      = track;
    obj["time_of_acquisition"] = time_of_acquisition;
  }
};
