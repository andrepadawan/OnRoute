#include "Screens.h"
#include <stdio.h>
#include <string.h>

// Layout 128x64:
//   y  0-13  intestazione in negativo (bianco pieno, testo nero)
//   y 25     riga 1   data + fonte dell'ora
//   y 37     riga 2   Trasmesso
//   y 49     riga 3   Internet
//   y 61     riga 4   GPS
// Font 6x10: 21 caratteri per riga. Font 7x13B (grassetto): 18 caratteri.

static const int W = 128;
static const int VALUE_X = 60;  // colonna dei valori: 11 caratteri disponibili

static void header(U8G2& g, const char* left, const char* right) { //Static perchè 
  //nessun altro oltre a questo file dovrebbe chiamarla
  g.setDrawColor(1);
  g.drawBox(0, 0, W, 14); //Rettangolo largo W, alto 14, acceso (1)
  g.setDrawColor(0);  // testo "in negativo, 0 spento" dentro il box sopra
  g.setFont(u8g2_font_7x13B_tf);
  g.drawStr(2, 11, left); //Scrive left: 2 px di leading, 11 di altezza (x,y)
  //Titolo a destra è opzionale
  if (right && *right) { //se c'è, quindi right non è nullpointer e nemmeno "\0", cioé stringa vuota

    g.drawStr(W - 2 - g.getStrWidth(right), 11, right); //messo a destra ad altezza 11, con distanza da left
  }
  g.setDrawColor(1);
}

// Etichetta a sinistra, valore nella colonna fissa. Se alert, il valore è
// in negativo: un "NO" deve saltare all'occhio anche da un metro.
static void field(U8G2& g, int y, const char* label, const char* value, bool alert) {
  g.setFont(u8g2_font_6x10_tf);
  g.drawStr(0, y, label);
  if (alert) {
    g.drawBox(VALUE_X - 2, y - 8, g.getStrWidth(value) + 3, 10);
    //Lo disegno in negativo
    g.setDrawColor(0);
  }
  g.drawStr(VALUE_X, y, value);
  g.setDrawColor(1);
}

static void rightStr(U8G2& g, int y, const char* s) {
  g.drawStr(W - 1 - g.getStrWidth(s), y, s);
}

void Screens::boot(U8G2& g, const char* message, const char* detail) {
  g.clearBuffer(); //Giustamente
  g.setFontMode(1);  // trasparente: serve per il testo in negativo
  header(g, "OnRoute test", nullptr);
  g.setFont(u8g2_font_7x13B_tf);
  g.drawStr(0, 34, message);
  g.setFont(u8g2_font_6x10_tf);
  if (detail && *detail) g.drawStr(0, 50, detail);
}

void Screens::portal(U8G2& g, const char* apName, const char* apPassword) {
  g.clearBuffer();
  g.setFontMode(1);
  header(g, "CONFIGURAZIONE", nullptr);
  g.setFont(u8g2_font_6x10_tf);
  g.drawStr(0, 26, "Collegati al WiFi:");
  g.setFont(u8g2_font_7x13B_tf);
  g.drawStr(0, 40, apName);
  g.setFont(u8g2_font_6x10_tf);
  char pass[32];
  if (apPassword && *apPassword) {
    snprintf(pass, sizeof(pass), "pass: %s", apPassword);
  } else {
    snprintf(pass, sizeof(pass), "(rete aperta)");
  }
  g.drawStr(0, 51, pass);
  g.drawStr(0, 62, "poi apri 192.168.4.1");
}

void Screens::status(U8G2& g, const UiState& s) {
  g.clearBuffer();
  g.setFontMode(1);

  // Intestazione: linea a sinistra, ora a destra
  char left[32];
  snprintf(left, sizeof(left), "LINEA %s", (s.line && *s.line) ? s.line : "--");
  header(g, left, s.time);

  // Riga 1: data + da dove arriva l'ora
  g.setFont(u8g2_font_6x10_tf);
  g.drawStr(0, 25, s.date);
  char src[32];
  if (strcmp(s.timeSource, "--") == 0) {
    snprintf(src, sizeof(src), "attesa ora");
  } else {
    snprintf(src, sizeof(src), "ora %s", s.timeSource);
  }
  rightStr(g, 25, src);

  // Riga 2: trasmesso
  char tx[32];
  if (s.transmitted) {
    snprintf(tx, sizeof(tx), "SI %lus fa", (unsigned long)s.secondsSinceTx);
  } else {
    snprintf(tx, sizeof(tx), "NO %s", s.txReason);
  }
  field(g, 37, "Trasmesso", tx, !s.transmitted);

  // Riga 3: internet
  const char* net = s.net == NetState::Online  ? "SI"
                  : s.net == NetState::Offline ? "NO"
                                               : "verifico...";
  field(g, 49, "Internet", net, s.net == NetState::Offline);

  // Riga 4: GPS
  char gps[32];
  if (s.gpsSilent) {
    snprintf(gps, sizeof(gps), "NESSUN DATO");
  } else if (s.gpsFix && s.fixMode >= 2) {
    snprintf(gps, sizeof(gps), "%dD %d sat", s.fixMode, s.satellites);
  } else if (s.gpsFix) {
    snprintf(gps, sizeof(gps), "fix %d sat", s.satellites);
  } else {
    snprintf(gps, sizeof(gps), "no fix %dsat", s.satellites);
    if (strlen(gps) > 11) snprintf(gps, sizeof(gps), "no fix %d", s.satellites);  // 10+ satelliti
  }
  field(g, 61, "GPS", gps, s.gpsSilent || !s.gpsFix);
}
