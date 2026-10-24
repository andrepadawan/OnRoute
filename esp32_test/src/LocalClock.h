#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// ============================================================================
//  LocalClock: l'orologio di sistema dell'ESP32, in ora italiana.
//
//  Due fonti, in ordine di preferenza:
//    1. GPS  — funziona anche senza internet, basta vedere un satellite
//    2. NTP  — funziona anche al chiuso, basta internet
//  Entrambe scrivono nello stesso orologio di sistema (UTC). La conversione
//  in ora locale (con il cambio ora legale/solare) la fa la libc tramite TZ.
// ============================================================================
class LocalClock {
public:
  void begin(const char* posixTz);
  void startNtp();                        // dopo che il WiFi è connesso
  void syncFromGps(const struct tm& utc); // chiamala spesso: corregge solo se serve

  bool isValid() const;
  // "04/10/2026" e "14:03:21". false (e trattini) se l'ora non è ancora nota.
  bool format(char* date, size_t dateLen, char* time, size_t timeLen) const;
  const char* source() const;             // ultima fonte che ha confermato l'ora: "GPS", "rete" o "--"

  // UTC "a pezzi" -> secondi dal 1970. Non usa mktime(), che lavorerebbe
  // in ora LOCALE: qui serve UTC puro. Algoritmo days_from_civil
  // di Howard Hinnant.
  static int64_t utcToEpoch(const struct tm& t) {
    int y = t.tm_year + 1900;
    int m = t.tm_mon + 1;
    int d = t.tm_mday;
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;                                    // [0, 399]
    const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;  // [0, 365]
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0, 146096]
    const int64_t days = (int64_t)era * 146097 + doe - 719468;
    return days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  }

private:
  bool syncedFromGps_ = false;
  const char* tz_ = "UTC0";
};
