#pragma once
#include <stddef.h>
#include <stdint.h>

// ============================================================================
//  Configurazione di compilazione: pin, tempi, default dal .env
//  Tutto ciò che vuoi "girare" durante i test sta qui, non sparso nel codice.
// ============================================================================

// ---- Pin (ESP32 DevKit v1 / ELEGOO ESP32 WROOM-32) -------------------------
constexpr int GPS_RX_PIN = 16;   // RX2 dell'ESP32  <-  TX del GPS  (obbligatorio)
constexpr int GPS_TX_PIN = 17;   // TX2 dell'ESP32  ->  RX del GPS  (facoltativo)
constexpr uint32_t GPS_BAUD = 9600;  // default del NEO-6M

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2         // LED blu on-board sulle DevKit v1
#endif

constexpr int RESET_BUTTON_PIN = 0;  // tasto BOOT: tenuto premuto 5 s a firmware avviato = reset

// OLED 128x64 I2C. 21/22 sono i pin I2C standard dell'ESP32.
// Controller: SSD1306 di default (i classici 0.96"). Se lo schermo mostra
// una colonna di pixel spazzatura sul bordo o è spostato di 2 pixel, è un
// SH1106 (tipico dei 1.3"): aggiungi -D OLED_SH1106 ai build_flags.
constexpr int OLED_SDA_PIN = 21;
constexpr int OLED_SCL_PIN = 22;

// ---- Tempi ------------------------------------------------------------------
constexpr uint32_t SEND_INTERVAL_MS    = 5000;  // ogni quanto si prova un POST
constexpr uint32_t FIX_MAX_AGE_MS      = 3000;  // posizione più vecchia di così = fix perso
constexpr uint32_t GPS_SILENT_AFTER_MS = 3000;  // nessun byte dal GPS per così = cablaggio
constexpr uint32_t DIAG_INTERVAL_MS    = 5000;  // ogni quanto stampare la diagnostica
constexpr uint32_t RESET_HOLD_MS       = 5000;
constexpr uint32_t HTTP_TIMEOUT_MS     = 8000;
constexpr uint32_t DISPLAY_REFRESH_MS  = 500;
constexpr uint32_t TX_FRESH_MS         = 15000; // "Trasmesso: SI" se l'ultimo POST ok è più recente di così
constexpr uint32_t NET_PROBE_INTERVAL_MS = 10000; // ogni quanto verificare internet (se nessun POST lo ha già fatto)
constexpr uint32_t NET_PROBE_TIMEOUT_MS  = 3000;
constexpr uint32_t NET_PROOF_FRESH_MS    = 20000; // una risposta HTTP vale come "internet ok" per così

// ---- Riaggancio WiFi (WifiKeeper) ------------------------------------------
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;                // ogni quanto un tentativo di riaggancio
constexpr uint16_t WIFI_HARD_RETRY_EVERY  = 4;                    // ogni N tentativi: radio off/on (~1 al minuto)
constexpr uint32_t WIFI_GIVE_UP_MS        = 30UL * 60UL * 1000UL; // 30 min senza WiFi -> ESP.restart()

// ---- Reti WiFi note (KnownNetworks) ------------------------------------------
// Elenco in NVS: sopravvive al flash del firmware. Pieno -> si sovrascrive la
// rete salvata da più tempo.
constexpr uint8_t  KNOWN_NETWORKS_MAX      = 6;
constexpr uint8_t  BOOT_SCAN_ATTEMPTS      = 2;      // scan all'avvio prima di aprire il portale
constexpr uint32_t WIFI_ATTEMPT_TIMEOUT_MS = 12000;  // tempo concesso a ogni rete candidata (DHCP compreso)

// ---- Clock della CPU ------------------------------------------------------------
// Default dell'ESP32: 240 MHz. A 160 MHz il loop (che gira senza pause) consuma
// meno, e niente di quello che fa il firmware ne risente:
//   - UART del GPS e I2C dell'OLED vanno con il bus APB, che resta a 80 MHz
//     per qualsiasi clock CPU >= 80 MHz;
//   - il WiFi richiede almeno 80 MHz;
//   - l'unica cosa che rallenta è la parte di calcolo dell'handshake TLS,
//     di circa 1,5x, e con keep-alive avviene solo quando la connessione si
//     riapre.
// 80 MHz (il minimo col WiFi) consumerebbe ancora meno, ma triplicherebbe
// l'handshake TLS, che blocca il loop e quindi il buffer del GPS: da provare
// solo dopo aver misurato i tempi "[HTTP] POST ... (xxx ms)" a 160.
constexpr uint32_t CPU_FREQ_MHZ = 160;

// ---- Ora locale ---------------------------------------------------------------
// Regola POSIX per l'Italia: CET (UTC+1) d'inverno, CEST (UTC+2) dall'ultima
// domenica di marzo all'ultima di ottobre. Il cambio ora lo gestisce la libc.
constexpr const char* TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";

constexpr size_t LINE_NUMBER_MAX_LEN = 3;  // "LINEA 123" sta ancora nell'intestazione

// Buffer di ricezione UART per il GPS. Il default del core è 256 byte, ma il
// NEO-6M ne manda ~500 al secondo: se il loop si blocca (es. handshake TLS)
// il buffer trabocca e le frasi NMEA arrivano mozzate.
constexpr size_t GPS_RX_BUFFER_BYTES = 4096;

// ---- Portale di configurazione (WiFiManager) --------------------------------
constexpr const char* PORTAL_AP_NAME    = "OnRoute-Setup";
constexpr uint16_t    PORTAL_TIMEOUT_S  = 180;  // poi riavvia e riprova
constexpr uint16_t    WIFI_CONNECT_TIMEOUT_S = 20;

// ---- Default dal .env (iniettati da load_env.py come -D ENV_<CHIAVE>) -------
// Se una chiave manca nel .env, il default è stringa vuota e il valore andrà
// inserito dal portale.
#ifndef ENV_URL_WEBSITE
#define ENV_URL_WEBSITE ""
#endif
#ifndef ENV_DEVICE_TOKEN
#define ENV_DEVICE_TOKEN ""
#endif
#ifndef ENV_PORTAL_PASSWORD
#define ENV_PORTAL_PASSWORD ""
#endif
#ifndef ENV_LINE_NUMBER
#define ENV_LINE_NUMBER ""
#endif
