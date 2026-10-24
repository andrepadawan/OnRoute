#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <functional>
#include "KnownNetworks.h"

// ============================================================================
//  WifiKeeper: riaggancia il WiFi quando l'hotspot sparisce e poi torna.
//
//  WiFi.setAutoReconnect(true) da solo non basta: nel core 2.0.x il driver
//  riprova solo per alcuni motivi di disconnessione, e dopo un'assenza lunga
//  dell'AP può smettere del tutto. Sintomo: hotspot riacceso, ESP32 che non
//  si riaggancia finché non gli togli corrente.
//
//  Scala di escalation, tutta guidata da update() nel loop:
//    1. primi WIFI_RETRY_INTERVAL_MS: lascio fare all'autoReconnect del core
//    2. ogni WIFI_RETRY_INTERVAL_MS:  scan delle reti note e tentativo con la
//                                     più forte in vista, poi le altre (morbido)
//    3. ogni WIFI_HARD_RETRY_EVERY tentativi: radio spenta e riaccesa prima
//                                     dello scan (duro)
//    4. dopo WIFI_GIVE_UP_MS senza WiFi: ESP.restart()        (per le brutte)
//       -> all'avvio, se nessuna rete nota è in vista, si riapre il portale
//
//  Lo scan è ASINCRONO (WiFi.scanNetworks(true)) e anche l'attesa della
//  connessione: update() controlla e torna subito, il loop non si blocca.
//  Se lo scan fallisce, ripiego sul vecchio metodo: WiFi.begin() sull'ultima
//  rete a cui ero collegato.
//
//  Copre solo "WiFi perso". Hotspot connesso ma senza dati è affare di
//  NetProbe: lì riavviare non servirebbe a niente.
// ============================================================================
class WifiKeeper {
public:
  // Da chiamare DOPO che connectWifi() è riuscita.
  //   networks:   le reti note fra cui scegliere quando quella attuale cade
  //   onGiveUp(): chiamata appena prima di ESP.restart() (es. per il display)
  void begin(KnownNetworks& networks, std::function<void()> onGiveUp);

  void update();  // a ogni giro di loop()

private:
  enum class Phase : uint8_t { Idle, Scanning, Connecting };

  static const char* reasonName(uint8_t reason);
  void startCandidate(uint32_t now);  // WiFi.begin() verso candidates_[candidateIdx_]
  void fallbackBegin();               // scan impossibile: riprova l'ultima rete usata

  KnownNetworks* networks_ = nullptr;
  String ssid_;  // ultima rete a cui ero collegato (per il ripiego)
  String psk_;
  std::function<void()> onGiveUp_;

  Phase phase_ = Phase::Idle;
  uint32_t phaseSinceMs_ = 0;
  WifiCandidate candidates_[KNOWN_NETWORKS_MAX];
  uint8_t candidateCount_ = 0;
  uint8_t candidateIdx_ = 0;

  bool down_ = false;
  uint32_t downSinceMs_ = 0;
  uint32_t lastAttemptMs_ = 0;
  uint16_t attempts_ = 0;

  // Scritto dal task degli eventi WiFi, letto dal loop: atomico.
  std::atomic<uint8_t> lastReason_{0};
};
