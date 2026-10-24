#pragma once
#include <U8g2lib.h>
#include "Screens.h"
#include "config.h"

// ============================================================================
//  Display: la parte "hardware" dello schermo — trovare l'OLED sul bus I2C,
//  inizializzarlo, spedire il buffer. Cosa disegnare lo decide Screens.
//
//  Se lo schermo non c'è (o è cablato male) il firmware continua a girare:
//  tutte le show*() diventano no-op. Un display rotto non deve fermare il GPS.
// ============================================================================
#if defined(OLED_SH1106)
using OledDriver = U8G2_SH1106_128X64_NONAME_F_HW_I2C;   // tipico dei 1.3"
#else
using OledDriver = U8G2_SSD1306_128X64_NONAME_F_HW_I2C;  // tipico dei 0.96"
#endif

class Display {
public:
  Display() : oled_(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN) {}

  bool begin();  // false = nessuno schermo trovato
  bool present() const { return present_; }

  void showBoot(const char* message, const char* detail = "");
  void showPortal(const char* apName, const char* apPassword);
  void showStatus(const UiState& state);

private:
  OledDriver oled_;  // "F" nel nome = frame buffer completo (1 KB di RAM)
  bool present_ = false;
};
