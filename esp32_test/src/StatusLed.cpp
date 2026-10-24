#include "StatusLed.h"

void StatusLed::begin(int pin) {
  pin_ = pin;
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
}

void StatusLed::on() {
  digitalWrite(pin_, HIGH);
  lastLevel_ = true;
}

// N flash brevi all'inizio di un ciclo di 2 secondi, poi buio.
// Nessuno stato interno: il pattern si calcola da millis(), quindi non importa
// quanto spesso (o irregolarmente) update() viene chiamato.
bool StatusLed::flashes(uint32_t now, int count) {
  const uint32_t cycleMs = 2000, slotMs = 300, onMs = 120;
  uint32_t phase = now % cycleMs;
  uint32_t slot = phase / slotMs;
  return slot < (uint32_t)count && (phase % slotMs) < onMs;
}

void StatusLed::update() {
  uint32_t now = millis();
  bool level = false;

  switch (pattern_) {
    case LedPattern::Portal:      level = true;                     break;
    case LedPattern::GpsSilent:   level = (now / 100) % 2 == 0;     break;
    case LedPattern::Waiting:     level = (now / 500) % 2 == 0;     break;
    case LedPattern::WifiDown:    level = flashes(now, 3);          break;
    case LedPattern::ServerError: level = flashes(now, 2);          break;
    case LedPattern::AllGood:     level = flashes(now, 1);          break;
  }

  if (level != lastLevel_) {  // scrivi sul pin solo quando cambia
    digitalWrite(pin_, level ? HIGH : LOW);
    lastLevel_ = level;
  }
}
