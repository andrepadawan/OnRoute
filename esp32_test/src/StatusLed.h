#pragma once
#include <Arduino.h>

// ============================================================================
//  StatusLed: il "monitor seriale" di quando il Mac non c'è.
//
//  Pattern (in ordine di priorità, vince il primo problema trovato):
//    acceso fisso       -> portale aperto: collegati al WiFi "OnRoute-Setup"
//    lampeggio veloce   -> il GPS non manda byte: cablaggio
//    lampeggio lento    -> GPS ok ma niente fix (o primo invio non ancora fatto)
//    3 flash ogni 2 s   -> WiFi/hotspot perso
//    2 flash ogni 2 s   -> il server risponde con errore (token? URL?)
//    1 flash ogni 2 s   -> tutto ok: ultimo POST accettato
// ============================================================================
enum class LedPattern {
  Portal,
  GpsSilent,
  Waiting,
  WifiDown,
  ServerError,
  AllGood,
};

class StatusLed {
public:
  void begin(int pin);
  void set(LedPattern pattern) { pattern_ = pattern; }
  void on();      // per quando il loop è bloccato (es. portale)
  void update();  // da chiamare a ogni giro di loop()

private:
  static bool flashes(uint32_t now, int count);

  int pin_ = -1;
  LedPattern pattern_ = LedPattern::Waiting;
  bool lastLevel_ = false;
};
