#include "WifiKeeper.h"
#include "config.h"

void WifiKeeper::begin(KnownNetworks& networks, std::function<void()> onGiveUp) {
  networks_ = &networks;
  onGiveUp_ = onGiveUp;
  ssid_ = WiFi.SSID();
  psk_ = WiFi.psk();

  // Le credenziali sono già in NVS (nel nostro elenco). Da qui in poi
  // ogni WiFi.begin() del riaggancio NON deve riscriverle in flash:
  // un tentativo al minuto per mezz'ora = flash consumata per niente.
  WiFi.persistent(false);

  // Motivo dell'ultima disconnessione: è l'indizio che dice PERCHÉ non si
  // riaggancia (AP sparito? password? handshake?). ASSOC_LEAVE lo ignoro:
  // è la disconnessione che provoco io prima di ogni scan.
  WiFi.onEvent(
      [this](WiFiEvent_t, WiFiEventInfo_t info) {
        uint8_t r = info.wifi_sta_disconnected.reason;
        if (r != WIFI_REASON_ASSOC_LEAVE) lastReason_ = r;
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Serial.printf("[WIFI] Watchdog attivo: riaggancio ogni %lu s fra %u reti note, riavvio dopo %lu min senza WiFi\n",
                (unsigned long)(WIFI_RETRY_INTERVAL_MS / 1000), networks_->count(),
                (unsigned long)(WIFI_GIVE_UP_MS / 60000));
}

void WifiKeeper::update() {
  uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (down_) {
      Serial.printf("[WIFI] Riagganciato a \"%s\" dopo %lu s (%u tentativi), IP %s\n",
                    WiFi.SSID().c_str(), (unsigned long)((now - downSinceMs_) / 1000),
                    attempts_, WiFi.localIP().toString().c_str());
      down_ = false;
      attempts_ = 0;
      // La rete potrebbe essere un'altra rispetto a prima: è lei il nuovo ripiego.
      ssid_ = WiFi.SSID();
      psk_ = WiFi.psk();
    }
    if (phase_ == Phase::Scanning) WiFi.scanDelete();
    phase_ = Phase::Idle;
    return;
  }

  if (!down_) {
    down_ = true;
    downSinceMs_ = now;
    lastAttemptMs_ = now;  // primo intervallo: prima prova l'autoReconnect del core
    Serial.println("[WIFI] Connessione persa, provo a riagganciarmi");
    return;
  }

  uint32_t downFor = now - downSinceMs_;
  if (downFor >= WIFI_GIVE_UP_MS) {
    Serial.printf("[WIFI] Nessun WiFi da %lu min: riavvio\n", (unsigned long)(downFor / 60000));
    if (onGiveUp_) onGiveUp_();
    delay(1000);  // tempo alla seriale di svuotarsi e al display di farsi leggere
    ESP.restart();
  }

  // ---- Tentativo in corso: controllo e torno subito al loop ----------------
  if (phase_ == Phase::Scanning) {
    int16_t found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) return;
    if (found < 0) {
      Serial.println("[WIFI] Scan fallito");
      phase_ = Phase::Idle;
      fallbackBegin();
      return;
    }
    candidateCount_ = networks_->matchScan(found, candidates_, KNOWN_NETWORKS_MAX);
    WiFi.scanDelete();
    Serial.printf("[WIFI] Scan: %d reti in vista, di cui %u note\n", found, candidateCount_);
    if (candidateCount_ == 0) {
      phase_ = Phase::Idle;  // niente di noto in giro: riprovo al prossimo intervallo
      return;
    }
    candidateIdx_ = 0;
    startCandidate(now);
    return;
  }

  if (phase_ == Phase::Connecting) {
    // Se si fosse collegato, ce ne saremmo accorti in cima a update().
    if (now - phaseSinceMs_ < WIFI_ATTEMPT_TIMEOUT_MS) return;
    Serial.printf("[WIFI] \"%s\" non risponde entro %lu s (ultimo motivo: %s)\n",
                  networks_->ssid(candidates_[candidateIdx_].slot),
                  (unsigned long)(WIFI_ATTEMPT_TIMEOUT_MS / 1000), reasonName(lastReason_));
    if (++candidateIdx_ < candidateCount_) {
      startCandidate(now);  // la prossima in ordine di segnale
    } else {
      phase_ = Phase::Idle;
    }
    return;
  }

  // ---- Nessun tentativo in corso: è ora di un altro? ------------------------
  if (now - lastAttemptMs_ < WIFI_RETRY_INTERVAL_MS) return;
  lastAttemptMs_ = now;
  attempts_++;

  bool hard = attempts_ % WIFI_HARD_RETRY_EVERY == 0;
  Serial.printf("[WIFI] Giù da %lu s, tentativo %u %s (ultimo motivo: %s)\n",
                (unsigned long)(downFor / 1000), attempts_,
                hard ? "DURO: radio off/on + scan" : "morbido: scan", reasonName(lastReason_));

  if (hard) {
    // Il driver può restare incastrato in uno stato da cui reconnect() non lo
    // tira fuori. Spegnere e riaccendere la radio lo riparte da zero, senza
    // perdere GPS, ora e stato del firmware come farebbe un restart.
    WiFi.mode(WIFI_OFF);
    delay(100);  // blocca il loop 0,1 s: il buffer UART del GPS regge
    WiFi.mode(WIFI_STA);
  } else {
    // Ferma eventuali tentativi del driver: con la STA "in connessione"
    // l'ESP32 rifiuta di avviare uno scan.
    WiFi.disconnect();
  }

  // Asincrono: ritorna subito, il risultato lo raccoglie scanComplete() qui sopra.
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    Serial.println("[WIFI] Impossibile avviare lo scan");
    fallbackBegin();
    return;
  }
  phase_ = Phase::Scanning;
  phaseSinceMs_ = now;
}

void WifiKeeper::startCandidate(uint32_t now) {
  const WifiCandidate& c = candidates_[candidateIdx_];
  Serial.printf("[WIFI] Provo \"%s\" (%ld dBm, canale %ld)\n", networks_->ssid(c.slot),
                (long)c.rssi, (long)c.channel);
  // Non bloccante: la connessione la porta avanti il task WiFi.
  WiFi.begin(networks_->ssid(c.slot), networks_->psk(c.slot), c.channel, c.bssid);
  phase_ = Phase::Connecting;
  phaseSinceMs_ = now;
}

void WifiKeeper::fallbackBegin() {
  Serial.printf("[WIFI] Ripiego: riprovo l'ultima rete \"%s\"\n", ssid_.c_str());
  if (ssid_.isEmpty()) WiFi.begin();
  else WiFi.begin(ssid_.c_str(), psk_.c_str());
}

// I codici più comuni con un hotspot da telefono. Gli altri: numero grezzo,
// da cercare in esp_wifi_types.h (wifi_err_reason_t).
const char* WifiKeeper::reasonName(uint8_t reason) {
  switch (reason) {
    case 0:                                   return "nessuno ancora";
    case WIFI_REASON_AUTH_EXPIRE:             return "2 auth scaduta";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:  return "15 handshake (password?)";
    case WIFI_REASON_BEACON_TIMEOUT:          return "200 AP sparito";
    case WIFI_REASON_NO_AP_FOUND:             return "201 AP non trovato";
    case WIFI_REASON_AUTH_FAIL:               return "202 auth fallita";
    case WIFI_REASON_ASSOC_FAIL:              return "203 associazione fallita";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:       return "204 handshake timeout";
    default: {
      static char buf[12];  // solo il loop la chiama: niente concorrenza
      snprintf(buf, sizeof(buf), "%u", reason);
      return buf;
    }
  }
}
