#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================================
//  KnownNetworks: l'elenco delle reti WiFi che l'ESP32 conosce, in NVS.
//
//  WiFiManager (e lo stack WiFi dell'ESP32) ricordano UNA sola rete: l'ultima.
//  Qui ne teniamo fino a KNOWN_NETWORKS_MAX, in un namespace NVS tutto nostro.
//  La NVS è una partizione separata dal firmware: un normale
//  `pio run -t upload` riscrive solo la partizione app, quindi le reti
//  sopravvivono agli aggiornamenti. Le cancellano solo "Erase Flash"
//  (pio run -t erase) e il reset col tasto BOOT.
//
//  Ogni rete occupa uno slot con tre chiavi:  s<i> = SSID,  p<i> = password,
//  q<i> = numero di sequenza (più alto = salvata più di recente).
//  Elenco pieno -> la nuova rete prende lo slot con la sequenza più bassa,
//  cioè la più vecchia. Una rete già nota con password nuova viene
//  aggiornata e conta come appena salvata.
//
//  Questa classe NON si collega a niente: sa solo cosa conosce e, dato uno
//  scan, quali reti note sono in vista e in che ordine provarle. A collegarsi
//  ci pensano Settings (all'avvio) e WifiKeeper (quando la rete cade).
// ============================================================================

// Una rete nota trovata dallo scan: quale slot, quanto bene si sente, e su
// quale canale/AP sta (passarli a WiFi.begin() risparmia una seconda ricerca).
struct WifiCandidate {
  uint8_t slot = 0;
  int32_t rssi = -127;
  int32_t channel = 0;
  uint8_t bssid[6] = {};
};

class KnownNetworks {
public:
  void load();  // legge l'elenco dalla NVS

  // Aggiunge la rete, o ne aggiorna la password. Ritorna true se ha scritto
  // in NVS (false se era già nota identica, o se l'SSID non è valido).
  bool remember(const String& ssid, const String& psk);

  // Primo avvio dopo l'aggiornamento del firmware: se l'elenco è vuoto,
  // importa la rete che il vecchio firmware (WiFiManager) aveva salvato
  // nello stack WiFi. Va chiamata DOPO WiFi.mode(WIFI_STA).
  bool importFromWifiStack();

  void clear();  // svuota l'elenco (reset col tasto BOOT)

  uint8_t count() const;
  const char* ssid(uint8_t slot) const { return slots_[slot].ssid.c_str(); }
  // nullptr per una rete aperta: è quello che si aspetta WiFi.begin()
  const char* psk(uint8_t slot) const {
    return slots_[slot].psk.isEmpty() ? nullptr : slots_[slot].psk.c_str();
  }

  // Da chiamare con il risultato di WiFi.scanNetworks() (o scanComplete()),
  // PRIMA di WiFi.scanDelete(). Riempie `out` con le reti note in vista,
  // dal segnale più forte al più debole. Ritorna quante sono.
  uint8_t matchScan(int16_t found, WifiCandidate* out, uint8_t maxOut) const;

  void printSummary(Print& out) const;

private:
  struct Entry {
    bool used = false;
    String ssid;
    String psk;
    uint32_t seq = 0;
  };

  int find(const String& ssid) const;  // slot della rete, -1 se sconosciuta
  int slotForNew() const;              // primo slot libero, altrimenti il più vecchio

  Entry slots_[KNOWN_NETWORKS_MAX];
  uint32_t nextSeq_ = 1;
};
