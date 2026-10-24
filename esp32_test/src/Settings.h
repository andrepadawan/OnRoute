#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <functional>
#include "KnownNetworks.h"

// ============================================================================
//  Settings: da dove vengono URL, token e numero linea, e come ci si connette
//  al WiFi.
//
//  Precedenza dei valori:
//    1. override salvati dal portale (memoria NVS, sopravvivono al riavvio)
//    2. default compilati dal .env
//  Se ricompili con un .env DIVERSO, gli override vecchi vengono azzerati:
//  così "ho cambiato il .env ma usa ancora il vecchio URL" non può succedere.
//
//  Token e linea sono una COPPIA: si salvano e si caricano sempre insieme,
//  dalla stessa fonte. Non può capitare un token del portale con la linea
//  del .env o viceversa. La linea serve solo al display: al server non viene
//  mandata, perché l'identità della linea sta nel token (decisione di agosto).
//
//  NVS (Preferences) invece di SPIFFS + config.json: è un archivio
//  chiave/valore già pronto nella flash, non va formattato né parsato.
// ============================================================================
class Settings {
public:
  void load();
  void printSummary(Print& out) const;

  // Cerca con uno scan le reti note (KnownNetworks) e si collega alla più
  // forte; se nessuna è in vista apre il captive portal, e la rete scelta
  // lì viene aggiunta all'elenco.
  // Ritorna false se il portale scade senza che nessuno l'abbia configurato.
  //   onConnecting(detail): chiamata a ogni fase ("cerco N reti note", poi
  //                         l'SSID che sta provando; "" = nessuna rete salvata)
  //   onPortalOpen():       chiamata quando si apre il portale (il loop si ferma)
  bool connectWifi(std::function<void(const String& ssid)> onConnecting,
                   std::function<void()> onPortalOpen);

  // Cancella reti salvate + override NVS e riavvia (-> riapre il portale).
  void factoryReset();

  const String& serverUrl() const { return serverUrl_; }
  const String& deviceToken() const { return deviceToken_; }
  const String& lineNumber() const { return lineNumber_; }

  // L'elenco delle reti note: serve anche a WifiKeeper per riagganciarsi.
  KnownNetworks& networks() { return networks_; }

private:
  void saveUrl();
  void saveTokenAndLine();
  bool connectToKnown(const std::function<void(const String&)>& onConnecting);

  KnownNetworks networks_;
  Preferences prefs_;
  String serverUrl_;
  String deviceToken_;
  String lineNumber_;
  bool urlFromPortal_ = false;
  bool tokenFromPortal_ = false;  // vale anche per la linea: viaggiano in coppia
};
