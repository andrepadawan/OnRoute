// ============================================================================
//  OnRoute — esp32_test
//  Firmware di TEST: GPS NEO-6M -> ESP32 -> hotspot del telefono -> Railway,
//  con LED di stato e display OLED 128x64.
//
//  Flusso:
//    setup(): LED, display, GPS, config (.env + NVS), WiFi/captive portal,
//             poi NTP e il task che verifica internet
//    loop():  legge il GPS senza bloccare; ogni SEND_INTERVAL_MS, se c'è un
//             fix fresco e internet, manda un POST; aggiorna LED e display.
//
//  Il loop non contiene delay(): ogni compito controlla da solo se "è ora"
//  confrontando millis() con l'ultima volta che è stato eseguito.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "Coordinates.h"
#include "Display.h"
#include "GpsReader.h"
#include "LocalClock.h"
#include "NetProbe.h"
#include "Settings.h"
#include "StatusLed.h"
#include "Uploader.h"
#include "WifiKeeper.h"

GpsReader gps;
Settings settings;
StatusLed led;
Uploader uploader;
Display display;
LocalClock localClock;  // non "clock": clock() esiste già nella libreria C
NetProbe net;
WifiKeeper wifiKeeper;

static int lastHttpCode = 0;     // 0 = nessun POST ancora tentato
static uint32_t lastTxOkMs = 0;  // millis() dell'ultimo POST accettato (2xx)
static uint32_t lastSendMs = 0;
static uint32_t lastDiagMs = 0;
static uint32_t lastUiMs = 0;

static bool isSuccess(int code) { return code >= 200 && code < 300; }

static void trySend() {
  if (!gps.hasFreshFix(FIX_MAX_AGE_MS)) return;  // niente fix = niente POST

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NET] WiFi non connesso, salto l'invio");
    // Senza internet il POST resterebbe bloccato sul DNS fino a ~15 s,
    return;
  }
  // congelando display e GPS: meglio non provarci finché NetProbe non dà l'ok.
  if (net.state() == NetState::Offline) {
    Serial.println("[NET] Internet assente, salto l'invio");
    return;
  }

  Coordinates coords;
  if (!gps.fillCoordinates(coords)) {
    Serial.println("[GPS] posizione ok ma data/ora non ancora valide, aspetto");
    return;
  }

  lastHttpCode = uploader.post(settings.serverUrl(), settings.deviceToken(), coords);

  if (lastHttpCode > 0) {
    net.reportHttpResponse();  // il server ha risposto: internet c'è
  } else if (lastHttpCode != Uploader::ERR_BAD_URL) {
    net.reportNetworkError();
  }
  if (isSuccess(lastHttpCode)) lastTxOkMs = millis();
}

// Perché "Trasmesso: NO"? Massimo 8 caratteri, se no non sta nello schermo.
// L'ordine conta: si riporta il primo anello rotto della catena.
static void describeTxFailure(char* out, size_t len) {
  if (gps.isSilent())                             snprintf(out, len, "GPS muto");
  else if (!gps.hasFreshFix(FIX_MAX_AGE_MS))      snprintf(out, len, "no fix");
  else if (WiFi.status() != WL_CONNECTED)         snprintf(out, len, "no WiFi");
  else if (net.state() == NetState::Offline)      snprintf(out, len, "no rete");
  else if (lastHttpCode == Uploader::ERR_BAD_URL) snprintf(out, len, "URL?");
  else if (lastHttpCode < 0)                      snprintf(out, len, "err rete");
  else if (lastHttpCode > 0 && !isSuccess(lastHttpCode)) snprintf(out, len, "HTTP %d", lastHttpCode);
  else                                            snprintf(out, len, "attesa");
}

static void refreshDisplay() {
  UiState s;
  s.line = settings.lineNumber().c_str();

  localClock.format(s.date, sizeof(s.date), s.time, sizeof(s.time));
  s.timeSource = localClock.source();

  uint32_t now = millis();
  s.transmitted = lastTxOkMs != 0 && now - lastTxOkMs < TX_FRESH_MS;
  if (s.transmitted) {
    s.secondsSinceTx = (now - lastTxOkMs) / 1000;
  } else {
    describeTxFailure(s.txReason, sizeof(s.txReason));
  }

  s.net = net.state();
  s.gpsSilent = gps.isSilent();
  s.gpsFix = gps.hasFreshFix(FIX_MAX_AGE_MS);
  s.fixMode = gps.fixMode();
  s.satellites = gps.satellites();

  display.showStatus(s);
}

static LedPattern currentPattern() {
  if (gps.isSilent())                    return LedPattern::GpsSilent;
  if (!gps.hasFreshFix(FIX_MAX_AGE_MS))  return LedPattern::Waiting;
  if (WiFi.status() != WL_CONNECTED || net.state() == NetState::Offline) return LedPattern::WifiDown;
  if (lastHttpCode == 0)                 return LedPattern::Waiting;
  if (!isSuccess(lastHttpCode))          return LedPattern::ServerError;
  return LedPattern::AllGood;
}

// Tasto BOOT tenuto premuto per RESET_HOLD_MS a firmware avviato -> reset
// e riapertura del portale. (NON tenerlo premuto mentre dai corrente: in quel
// caso l'ESP32 entra in modalità flash, è una cosa diversa.)
static void handleResetButton() {
  static bool pressed = false;
  static uint32_t pressedSince = 0;

  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (!pressed) {
      pressed = true;
      pressedSince = millis();
    } else if (millis() - pressedSince > RESET_HOLD_MS) {
      display.showBoot("Reset...", "riapro il portale");
      settings.factoryReset();  // non ritorna: riavvia
    }
  } else {
    pressed = false;
  }
}

void setup() {
  // Prima di tutto, prima che parta il WiFi: vedi CPU_FREQ_MHZ in config.h.
  setCpuFrequencyMhz(CPU_FREQ_MHZ);

  Serial.begin(115200);
  delay(300);  // tempo al monitor seriale di agganciarsi
  Serial.println("\n===== OnRoute esp32_test =====");
  Serial.printf("[BOOT] CPU a %lu MHz (bus APB a %lu MHz)\n",
                (unsigned long)getCpuFrequencyMhz(), (unsigned long)(getApbFrequency() / 1000000));

  led.begin(STATUS_LED_PIN);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  display.begin();
  display.showBoot("Avvio...");

  gps.begin(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
  localClock.begin(TIMEZONE);

  settings.load();
  settings.printSummary(Serial);

  bool connected = settings.connectWifi(
      [](const String& ssid) {
        display.showBoot("Connessione WiFi", ssid.isEmpty() ? "(nessuna rete salvata)" : ssid.c_str());
      },
      []() {
        led.on();  // il portale blocca il loop: LED acceso fisso
        bool hasPassword = strlen(ENV_PORTAL_PASSWORD) >= 8;
        display.showPortal(PORTAL_AP_NAME, hasPassword ? ENV_PORTAL_PASSWORD : "");
      });

  if (!connected) {
    Serial.println("[WIFI] Portale scaduto senza configurazione: riavvio e riprovo");
    display.showBoot("Portale scaduto", "riavvio...");
    delay(1000);
    ESP.restart();
  }

  uploader.begin();
  localClock.startNtp();          // ora da internet, se il GPS non l'ha ancora data
  net.begin(settings.serverUrl());
  wifiKeeper.begin(settings.networks(), []() { display.showBoot("WiFi perso", "riavvio..."); });
  Serial.println("[BOOT] Pronto. Diagnostica GPS ogni 5 s.");
}

void loop() {
  gps.poll();
  handleResetButton();
  wifiKeeper.update();

  uint32_t now = millis();

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    trySend();
    gps.poll();  // il POST può durare un secondo: svuota subito il buffer UART
  }

  if (now - lastDiagMs >= DIAG_INTERVAL_MS) {
    lastDiagMs = now;
    gps.printDiagnostics(Serial);
  }

  if (now - lastUiMs >= DISPLAY_REFRESH_MS) {
    lastUiMs = now;
    struct tm utc;
    if (gps.utcTime(utc)) localClock.syncFromGps(utc);
    refreshDisplay();
  }

  led.set(currentPattern());
  led.update();
}
