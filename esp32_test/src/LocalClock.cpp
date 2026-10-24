#include "LocalClock.h"
#include <Arduino.h>
#include <atomic>
#include <sys/time.h>
#include <esp_sntp.h>
#include "config.h"

// Prima di questa data l'orologio di sistema non è mai stato impostato
// (all'accensione l'ESP32 parte dal 1970).
static const time_t VALID_AFTER = 1704067200;  // 2024-01-01 00:00:00 UTC

// Ultima fonte che ha confermato l'ora. È atomica perché la scrive anche la
// callback NTP, che gira in un altro task (quello dello stack di rete).
enum : uint8_t { SRC_NONE, SRC_GPS, SRC_NTP };
static std::atomic<uint8_t> lastSource{SRC_NONE};

static void onNtpSync(struct timeval*) {
  lastSource = SRC_NTP;
}

void LocalClock::begin(const char* posixTz) {
  tz_ = posixTz;
  setenv("TZ", tz_, 1);
  tzset();
}

void LocalClock::startNtp() {
  sntp_set_time_sync_notification_cb(onNtpSync);
  // Non bloccante: SNTP gira in background dentro lo stack di rete
  // e si riallinea da solo ogni ora.
  configTzTime(tz_, NTP_SERVER_1, NTP_SERVER_2);
}

void LocalClock::syncFromGps(const struct tm& utc) {
  int64_t gpsEpoch = utcToEpoch(utc);
  time_t now = time(nullptr);

  // Il GPS ha risoluzione di 1 s e la frase arriva qualche centinaio di ms
  // dopo lo scatto del secondo: correggiamo solo scarti di almeno 2 s,
  // altrimenti l'orologio "balla" avanti e indietro di un secondo.
  int64_t drift = gpsEpoch - (int64_t)now;
  if (drift < 0) drift = -drift;

  if (!syncedFromGps_ || drift >= 2) {
    struct timeval tv = {};
    tv.tv_sec = (time_t)gpsEpoch;
    settimeofday(&tv, nullptr);
    if (!syncedFromGps_) Serial.println("[TIME] Orologio impostato dal GPS");
    syncedFromGps_ = true;
  }
  lastSource = SRC_GPS;
}

bool LocalClock::isValid() const {
  return time(nullptr) > VALID_AFTER;
}

bool LocalClock::format(char* date, size_t dateLen, char* timeStr, size_t timeLen) const {
  if (!isValid()) {
    snprintf(date, dateLen, "--/--/----");
    snprintf(timeStr, timeLen, "--:--:--");
    return false;
  }
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);  // UTC -> ora italiana, ora legale inclusa
  strftime(date, dateLen, "%d/%m/%Y", &local);
  strftime(timeStr, timeLen, "%H:%M:%S", &local);
  return true;
}

const char* LocalClock::source() const {
  if (!isValid()) return "--";
  switch (lastSource.load()) {
    case SRC_GPS: return "GPS";
    case SRC_NTP: return "rete";
    default:      return "--";
  }
}
