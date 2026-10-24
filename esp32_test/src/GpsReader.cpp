#include "GpsReader.h"
#include "config.h"

void GpsReader::begin(int rxPin, int txPin, uint32_t baud) {
  // Va chiamato PRIMA di begin(): dopo, il buffer è già allocato.
  Serial2.setRxBufferSize(GPS_RX_BUFFER_BYTES);
  Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
  Serial.printf("[GPS] UART2 avviata: RX=GPIO%d TX=GPIO%d @ %lu baud\n",
                rxPin, txPin, (unsigned long)baud);
}

void GpsReader::poll() {
  while (Serial2.available() > 0) {
    gps_.encode(Serial2.read());
    lastByteMs_ = millis();
    anyByteEver_ = true;
  }
}

bool GpsReader::isSilent() const {
  if (!anyByteEver_) {
    return millis() > GPS_SILENT_AFTER_MS;  // diamo al modulo il tempo di accendersi
  }
  return millis() - lastByteMs_ > GPS_SILENT_AFTER_MS;
}

bool GpsReader::hasFreshFix(uint32_t maxAgeMs) {
  // location.isValid() resta true per sempre dopo il primo fix:
  // è age() che dice se il dato è ancora attuale.
  return gps_.location.isValid() && gps_.location.age() < maxAgeMs;
}

int GpsReader::fixMode() {
  TinyGPSCustom& src = gngsaMode_.isValid() ? gngsaMode_ : gpgsaMode_;
  if (!src.isValid()) return 0;
  return atoi(src.value());
}

int GpsReader::satellites() {
  return gps_.satellites.isValid() ? (int)gps_.satellites.value() : 0;
}

bool GpsReader::utcTime(struct tm& out) {
  if (!gps_.date.isValid() || !gps_.time.isValid()) return false;
  if (gps_.time.age() > 1500) return false;  // dato non più fresco
  int year = gps_.date.year();
  if (year < 2024 || year > 2070) return false;  // RTC del modulo non ancora allineato

  out = {};
  out.tm_year = year - 1900;
  out.tm_mon  = gps_.date.month() - 1;
  out.tm_mday = gps_.date.day();
  out.tm_hour = gps_.time.hour();
  out.tm_min  = gps_.time.minute();
  out.tm_sec  = gps_.time.second();
  return true;
}

bool GpsReader::fillCoordinates(Coordinates& out) {
  // Prima del fix alcuni moduli mandano date assurde (1980, 2080...).
  if (!gps_.date.isValid() || !gps_.time.isValid() || gps_.date.year() < 2024) {
    return false;
  }

  out.latitude   = gps_.location.lat();
  out.longitude  = gps_.location.lng();
  out.speed      = gps_.speed.isValid()  ? gps_.speed.mps()  : 0.0;
  out.track      = gps_.course.isValid() ? gps_.course.deg() : 0.0;
  out.fix_status = fixMode();
  out.setTimeUtc(gps_.date.year(), gps_.date.month(), gps_.date.day(),
                 gps_.time.hour(), gps_.time.minute(), gps_.time.second());
  return true;
}

void GpsReader::printDiagnostics(Print& out) {
  // Come leggerla:
  //   chars=0            -> il GPS non parla: TX/RX invertiti, GND mancante, baud sbagliato
  //   fail cresce        -> byte corrotti: baud sbagliato o disturbi sul filo
  //   chars ok, sats=0   -> il GPS parla ma non vede il cielo: vai all'aperto e aspetta
  //   fix=2D/3D          -> tutto ok lato GPS
  out.printf("[GPS] chars=%lu ok=%lu fail=%lu sats=%lu hdop=%.1f fix=%d",
             (unsigned long)gps_.charsProcessed(),
             (unsigned long)gps_.passedChecksum(),
             (unsigned long)gps_.failedChecksum(),
             (unsigned long)(gps_.satellites.isValid() ? gps_.satellites.value() : 0),
             gps_.hdop.isValid() ? gps_.hdop.hdop() : 99.9,
             fixMode());

  if (gps_.location.isValid()) {
    out.printf(" lat=%.6f lng=%.6f age=%lums",
               gps_.location.lat(), gps_.location.lng(),
               (unsigned long)gps_.location.age());
  } else {
    out.print(" (nessuna posizione)");
  }
  out.println();

  if (isSilent()) {
    out.println("[GPS] !! nessun byte dal modulo: controlla TX GPS -> GPIO16, GND comune, alimentazione");
  }
}
