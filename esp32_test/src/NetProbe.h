#pragma once
#include <Arduino.h>
#include <atomic>
#include "NetState.h"

// ============================================================================
//  NetProbe: "c'è internet?" — che è diverso da "sono connesso al WiFi".
//  Un hotspot senza dati mobili ti dà WiFi pieno e internet zero.
//
//  Due prove valgono:
//    - una qualsiasi risposta HTTP dal server (anche un 401: il server l'hai raggiunto)
//    - una connessione TCP riuscita verso l'host del server (fatta da noi)
//
//  La prova TCP gira in un TASK FreeRTOS separato. Motivo: se il DNS non
//  risponde, la libreria resta bloccata fino a ~15 s. Nel loop principale
//  vorrebbe dire GPS, LED e display congelati; in un task a parte, chi se ne
//  accorge. Il task scrive solo variabili atomiche, il loop le legge.
// ============================================================================
class NetProbe {
public:
  // Estrae host e porta dall'URL e avvia il task. Da chiamare dopo il WiFi.
  void begin(const String& serverUrl);

  // Il loop la chiama quando riceve una risposta HTTP (codice > 0).
  void reportHttpResponse() { lastHttpProofMs_ = millis(); }

  // ...e quando il POST fallisce a livello di rete (codice < 0): indizio di
  // internet assente. Il task lo riverifica al giro successivo.
  void reportNetworkError() {
    lastHttpProofMs_ = 0;
    probeOk_ = false;
    probeAtMs_ = millis();
  }

  NetState state() const;

private:
  static void taskEntry(void* self);
  void run();
  static bool parseHostPort(const String& url, char* host, size_t hostLen, uint16_t& port);

  char host_[96] = "";
  uint16_t port_ = 443;
  std::atomic<bool> probeOk_{false};
  std::atomic<uint32_t> probeAtMs_{0};       // 0 = nessuna prova ancora
  std::atomic<uint32_t> lastHttpProofMs_{0};
};
