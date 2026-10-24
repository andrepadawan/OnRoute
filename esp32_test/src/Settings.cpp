#include "Settings.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"

static const char* NVS_NAMESPACE = "onroute";
static const char* KEY_URL = "url";
static const char* KEY_TOKEN = "token";
static const char* KEY_LINE = "line";
static const char* KEY_ENV_HASH = "env_hash";

// Impronta (FNV-1a) dei valori compilati dal .env: serve solo a capire se
// il firmware è stato ricompilato con un .env diverso.
static uint32_t fnv1a(const char* s, uint32_t h = 2166136261u) {
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

void Settings::load() {
  prefs_.begin(NVS_NAMESPACE, false);

  uint32_t envHash = fnv1a(ENV_LINE_NUMBER, fnv1a(ENV_DEVICE_TOKEN, fnv1a(ENV_URL_WEBSITE)));
  if (prefs_.getUInt(KEY_ENV_HASH, 0) != envHash) {
    if (prefs_.isKey(KEY_URL) || prefs_.isKey(KEY_TOKEN)) {
      Serial.println("[CFG] .env cambiato dall'ultima build: azzero gli override del portale");
    }
    prefs_.remove(KEY_URL);
    prefs_.remove(KEY_TOKEN);
    prefs_.remove(KEY_LINE);
    prefs_.putUInt(KEY_ENV_HASH, envHash);
  }

  urlFromPortal_ = prefs_.isKey(KEY_URL);
  serverUrl_ = urlFromPortal_ ? prefs_.getString(KEY_URL) : String(ENV_URL_WEBSITE);

  // Coppia token+linea: o entrambi dal portale, o entrambi dal .env.
  tokenFromPortal_ = prefs_.isKey(KEY_TOKEN);
  if (tokenFromPortal_) {
    deviceToken_ = prefs_.getString(KEY_TOKEN);
    lineNumber_ = prefs_.getString(KEY_LINE, "");
  } else {
    deviceToken_ = ENV_DEVICE_TOKEN;
    lineNumber_ = ENV_LINE_NUMBER;
  }

  prefs_.end();
}

void Settings::printSummary(Print& out) const {
  out.printf("[CFG] URL   = %s  (da %s)\n",
             serverUrl_.isEmpty() ? "(vuoto!)" : serverUrl_.c_str(),
             urlFromPortal_ ? "portale" : ".env");
  // Il token non si stampa mai per intero: la seriale finisce negli screenshot.
  out.printf("[CFG] TOKEN = %s  LINEA = %s  (da %s)\n",
             deviceToken_.isEmpty() ? "(vuoto!)"
                                    : (deviceToken_.substring(0, 3) + "***").c_str(),
             lineNumber_.isEmpty() ? "(non impostata)" : lineNumber_.c_str(),
             tokenFromPortal_ ? "portale" : ".env");
  if (!serverUrl_.startsWith("http://") && !serverUrl_.startsWith("https://")) {
    out.println("[CFG] !! l'URL deve iniziare con https:// (o http:// per un server locale)");
  }
}

void Settings::saveUrl() {
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putString(KEY_URL, serverUrl_);
  prefs_.end();
  urlFromPortal_ = true;
}

void Settings::saveTokenAndLine() {
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.putString(KEY_TOKEN, deviceToken_);
  prefs_.putString(KEY_LINE, lineNumber_);
  prefs_.end();
  tokenFromPortal_ = true;
}

// Fase 1 dell'avvio: scan e tentativo con le reti note, dalla più forte.
// Bloccante come lo era autoConnect(): il loop non è ancora partito.
bool Settings::connectToKnown(const std::function<void(const String&)>& onConnecting) {
  uint8_t known = networks_.count();
  if (known == 0) {
    Serial.println("[WIFI] Nessuna rete salvata");
    if (onConnecting) onConnecting("");
    return false;
  }

  char detail[32];
  for (uint8_t scan = 1; scan <= BOOT_SCAN_ATTEMPTS; scan++) {
    snprintf(detail, sizeof(detail), "cerco %u reti note", known);
    if (onConnecting) onConnecting(detail);
    Serial.printf("[WIFI] Scan %u/%u...\n", scan, BOOT_SCAN_ATTEMPTS);

    int16_t found = WiFi.scanNetworks();  // bloccante, 2-4 s
    if (found < 0) {
      Serial.println("[WIFI] Scan fallito");
      continue;
    }

    WifiCandidate candidates[KNOWN_NETWORKS_MAX];
    uint8_t n = networks_.matchScan(found, candidates, KNOWN_NETWORKS_MAX);
    WiFi.scanDelete();  // libera la RAM dei risultati
    Serial.printf("[WIFI] %d reti in vista, di cui %u note\n", found, n);
    if (n == 0) continue;  // magari l'hotspot si sta ancora accendendo: riprovo

    for (uint8_t i = 0; i < n; i++) {
      const WifiCandidate& c = candidates[i];
      const char* ssid = networks_.ssid(c.slot);
      Serial.printf("[WIFI] Connessione a \"%s\" (%ld dBm, canale %ld)...\n", ssid,
                    (long)c.rssi, (long)c.channel);
      if (onConnecting) onConnecting(ssid);

      WiFi.begin(ssid, networks_.psk(c.slot), c.channel, c.bssid);
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_ATTEMPT_TIMEOUT_MS) {
        delay(50);
      }
      if (WiFi.status() == WL_CONNECTED) return true;

      Serial.printf("[WIFI] \"%s\" non risponde entro %lu s (password cambiata?)\n", ssid,
                    (unsigned long)(WIFI_ATTEMPT_TIMEOUT_MS / 1000));
      WiFi.disconnect();  // ferma i tentativi del driver prima della prossima rete
    }
    return false;  // reti note in vista ma nessuna ha accettato: inutile ripetere lo scan
  }
  return false;
}

bool Settings::connectWifi(std::function<void(const String& ssid)> onConnecting,
                           std::function<void()> onPortalOpen) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // se l'hotspot sparisce e torna, si riaggancia da solo

  networks_.load();
  networks_.importFromWifiStack();  // primo avvio dopo l'aggiornamento: non perdere la rete
  networks_.printSummary(Serial);

  if (connectToKnown(onConnecting)) {
    Serial.printf("[WIFI] Connesso a \"%s\", IP %s, RSSI %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
  }

  // Fase 2: nessuna rete nota in vista -> captive portal, come prima.
  WiFiManager wm;
  wm.setTitle("OnRoute tracker");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wm.setAPCallback([onPortalOpen](WiFiManager*) {
    Serial.printf("[WIFI] Portale aperto: collegati alla rete \"%s\" e apri 192.168.4.1\n",
                  PORTAL_AP_NAME);
    if (onPortalOpen) onPortalOpen();
  });

  // Campi extra nella pagina del portale. Il token NON viene precompilato:
  // la pagina è servita in chiaro, chiunque sia collegato all'AP la vedrebbe.
  // URL e linea sì: non sono segreti, e vederli aiuta a non sbagliare coppia.
  // Per tutti e tre vale: campo lasciato vuoto = valore invariato.
  WiFiManagerParameter urlParam("server_url", "URL server (vuoto = invariato)",
                                serverUrl_.c_str(), 160);
  WiFiManagerParameter tokenParam("device_token", "Device token (vuoto = invariato)",
                                  "", 64, "type=\"password\"");
  WiFiManagerParameter lineParam("line_number", "Numero linea (vuoto = invariato)",
                                 lineNumber_.c_str(), LINE_NUMBER_MAX_LEN);
  wm.addParameter(&urlParam);
  wm.addParameter(&tokenParam);
  wm.addParameter(&lineParam);

  // Password dell'AP solo se valida (WPA2 vuole almeno 8 caratteri).
  const char* apPassword = strlen(ENV_PORTAL_PASSWORD) >= 8 ? ENV_PORTAL_PASSWORD : nullptr;

  // startConfigPortal() e non autoConnect(): autoConnect riproverebbe da solo
  // l'ultima rete dello stack, lavoro che connectToKnown() ha appena fatto meglio.
  if (!wm.startConfigPortal(PORTAL_AP_NAME, apPassword)) {
    return false;
  }
  WiFi.mode(WIFI_STA);  // spegne l'access point del portale, se fosse rimasto acceso

  Serial.printf("[WIFI] Connesso a \"%s\", IP %s, RSSI %d dBm\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());

  // WiFiManager ricorda solo l'ultima rete: la aggiungiamo al NOSTRO elenco.
  networks_.remember(WiFi.SSID(), WiFi.psk());

  // Si salvano solo cambiamenti veri: campo vuoto o identico = nessuna scrittura.
  String newUrl = String(urlParam.getValue());
  newUrl.trim();
  if (!newUrl.isEmpty() && newUrl != serverUrl_) {
    serverUrl_ = newUrl;
    saveUrl();
    Serial.println("[CFG] URL aggiornato dal portale");
  }

  String newToken = String(tokenParam.getValue());
  newToken.trim();
  String newLine = String(lineParam.getValue());
  newLine.trim();
  bool tokenChanged = !newToken.isEmpty() && newToken != deviceToken_;
  bool lineChanged = !newLine.isEmpty() && newLine != lineNumber_;
  if (tokenChanged || lineChanged) {
    if (tokenChanged) deviceToken_ = newToken;
    if (lineChanged) lineNumber_ = newLine;
    saveTokenAndLine();  // sempre in coppia
    Serial.printf("[CFG] Salvati dal portale: token %s, linea \"%s\"\n",
                  tokenChanged ? "nuovo" : "invariato", lineNumber_.c_str());
    if (tokenChanged && !lineChanged) {
      Serial.println("[CFG] !! token cambiato ma linea no: sicuro che sia la linea giusta?");
    }
  }
  return true;
}

void Settings::factoryReset() {
  Serial.println("[CFG] Reset: cancello reti salvate e override, poi riavvio");
  prefs_.begin(NVS_NAMESPACE, false);
  prefs_.clear();
  prefs_.end();

  // Anche l'elenco delle reti: altrimenti al riavvio l'ESP32 si ricollegherebbe
  // da solo a una rete nota e il portale non si riaprirebbe.
  networks_.clear();

  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}
