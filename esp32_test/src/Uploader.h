#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "Coordinates.h"

// ============================================================================
//  Uploader: POST JSON delle coordinate verso il server FastAPI.
//
//  Railway è solo HTTPS -> serve WiFiClientSecure. Con un WiFiClient normale
//  l'ESP32 parla HTTP in chiaro sulla porta 443 e la richiesta muore.
//  Per test contro il server sul Mac (http://IP-del-mac:8000/...) usa invece
//  il client in chiaro: la scelta avviene guardando lo schema dell'URL.
// ============================================================================
class Uploader {
public:
  void begin();

  // Ritorna il codice HTTP (200, 401, 422...) oppure un errore negativo
  // di HTTPClient (-1 connessione rifiutata, -11 timeout, ...).
  int post(const String& url, const String& token, const Coordinates& coords);

  static constexpr int ERR_BAD_URL = -100;

private:
  static void explain(int code);

  WiFiClientSecure tlsClient_;
  WiFiClient plainClient_;
  HTTPClient http_;
};
