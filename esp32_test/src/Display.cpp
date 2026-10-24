#include "Display.h"
#include <Wire.h>


static bool i2cAck(uint8_t address) {
  Wire.beginTransmission(address); //metodi di wire, ha bytes in trasmissione con write
  return Wire.endTransmission() == 0; //Se la trasmissione ha successo
}

bool Display::begin() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN); //inizializzo

  // Gli OLED 128x64 stanno a 0x3C (quasi sempre) o 0x3D (ponticello sul retro).
  uint8_t address = 0;
  if (i2cAck(0x3C)) address = 0x3C;
  else if (i2cAck(0x3D)) address = 0x3D;

  if (address == 0) { //Check
    Serial.println("[OLED] Nessuno schermo a 0x3C/0x3D. Scansione del bus:");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
      if (i2cAck(a)) {
        Serial.printf("[OLED]   dispositivo a 0x%02X\n", a);
        found++;
      }
    }
    if (found == 0) {
      Serial.println("[OLED]   niente di niente: SDA->GPIO21, SCL->GPIO22, VCC, GND? SDA/SCL invertiti?");
    }
    Serial.println("[OLED] Continuo senza schermo");
    return false;
  }

  oled_.setI2CAddress(address * 2);  // U8g2 vuole l'indirizzo a 8 bit (ne ho 7): shifto a sinistra di una posizione, *2 in base 2
  oled_.setBusClock(400000);         // fast: un frame in ~25 ms invece di ~100
  oled_.begin();
  present_ = true;
  Serial.printf("[OLED] Schermo trovato a 0x%02X\n", address);
  return true;
}

void Display::showBoot(const char* message, const char* detail) {
  if (!present_) return;
  Screens::boot(oled_, message, detail);
  oled_.sendBuffer();
}

void Display::showPortal(const char* apName, const char* apPassword) {
  if (!present_) return;
  Screens::portal(oled_, apName, apPassword);
  oled_.sendBuffer();
}

void Display::showStatus(const UiState& state) {
  if (!present_) return;
  Screens::status(oled_, state);
  oled_.sendBuffer();
}
