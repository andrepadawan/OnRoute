#pragma once
#include <Arduino.h>
#include <time.h>
#include <TinyGPS++.h>
#include "Coordinates.h"

// ============================================================================
//  GpsReader: legge l'UART del GPS e lo passa a TinyGPS++ SENZA mai bloccare.
//
//  Regola d'oro: poll() va chiamato a ogni giro di loop(). Niente delay()
//  lunghi nel loop, altrimenti il buffer UART trabocca e perdi frasi NMEA.
// ============================================================================
class GpsReader {
public:
  void begin(int rxPin, int txPin, uint32_t baud);

  // Consuma tutti i byte arrivati dall'ultima chiamata. Non bloccante.
  void poll();

  // true se il GPS non manda NULLA da un po' -> problema di cablaggio/baud.
  bool isSilent() const;

  // true se c'è una posizione valida e abbastanza recente.
  bool hasFreshFix(uint32_t maxAgeMs);

  // Copia la posizione corrente in `out`. Ritorna false se data/ora non sono
  // ancora affidabili (il server rifiuterebbe il payload).
  bool fillCoordinates(Coordinates& out);

  // Data/ora UTC dal GPS in una struct tm standard (tm_year = anni dal 1900,
  // tm_mon = 0-11). false se il dato manca, è vecchio o è palesemente sbagliato.
  // Il GPS conosce l'ora anche PRIMA del fix: basta un satellite.
  bool utcTime(struct tm& out);

  // Una riga di diagnostica sulla seriale: serve a capire DOVE sta il problema.
  void printDiagnostics(Print& out);

  int fixMode();     // 0 sconosciuto, 1 no fix, 2 = 2D, 3 = 3D
  int satellites();  // satelliti usati nel fix (0 se sconosciuto)

private:
  TinyGPSPlus gps_;
  // Campo 2 della frase GSA = modalità di fix. Il NEO-6M (solo GPS) manda
  // GPGSA; i moduli multi-costellazione (NEO-M8N/M9N) mandano GNGSA.
  TinyGPSCustom gpgsaMode_{gps_, "GPGSA", 2};
  TinyGPSCustom gngsaMode_{gps_, "GNGSA", 2};

  uint32_t lastByteMs_ = 0;
  bool anyByteEver_ = false;
};
