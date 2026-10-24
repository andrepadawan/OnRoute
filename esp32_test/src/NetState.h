#pragma once
#include <stdint.h>

// Stato di internet, condiviso fra NetProbe (che lo calcola) e Screens (che
// lo disegna). Sta in un header a parte così Screens non dipende dal WiFi.
enum class NetState : uint8_t { Unknown, Online, Offline };
