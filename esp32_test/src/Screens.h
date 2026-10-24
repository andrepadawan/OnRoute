#pragma once
#include <U8g2lib.h>
#include "NetState.h"

// ============================================================================
//  Screens: COSA si disegna, separato da COME si parla con lo schermo
//  (quello sta in Display). Queste funzioni toccano solo l'oggetto U8G2:
//  niente I2C, niente WiFi, niente GPS. Per questo si possono anche provare
//  sul computer, disegnando in memoria invece che sull'OLED.
// ============================================================================

// Fotografia di tutto ciò che va a schermo. Il loop la riempie, lo schermo
// la disegna: chi disegna non deve sapere da dove arrivano i dati.
struct UiState {
  const char* line = "";       // numero linea, "" se non impostato

  char date[12] = "--/--/----";
  char time[10] = "--:--:--";
  const char* timeSource = "--";  // "GPS" / "rete" / "--"

  bool transmitted = false;
  uint32_t secondsSinceTx = 0;    // valido se transmitted
  char txReason[16] = "";         // perché NO: "no fix", "HTTP 401", ...

  NetState net = NetState::Unknown;

  bool gpsSilent = false;
  bool gpsFix = false;
  int fixMode = 0;                // 2 = 2D, 3 = 3D
  int satellites = 0;
};

namespace Screens {
  void boot(U8G2& g, const char* message, const char* detail);
  void portal(U8G2& g, const char* apName, const char* apPassword);
  void status(U8G2& g, const UiState& s);
}
