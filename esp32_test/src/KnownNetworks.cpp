#include "KnownNetworks.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Namespace separato da quello di Settings ("onroute"): l'azzeramento degli
// override quando cambia il .env non deve toccare le reti salvate.
static const char* NVS_NAMESPACE = "wifinets";

static constexpr size_t SSID_MAX_LEN = 32;  // limiti dello standard 802.11 / WPA2
static constexpr size_t PSK_MAX_LEN = 64;

// "s0", "p0", "q0"... Le chiavi NVS possono essere lunghe al massimo 15 caratteri.
static void slotKey(char* out, size_t len, char kind, uint8_t slot) {
  snprintf(out, len, "%c%u", kind, (unsigned)slot);
}

void KnownNetworks::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);  // false: crea il namespace se non esiste ancora

  nextSeq_ = 1;
  char ks[6], kp[6], kq[6];
  for (uint8_t i = 0; i < KNOWN_NETWORKS_MAX; i++) {
    Entry& e = slots_[i];
    e = Entry();
    slotKey(ks, sizeof(ks), 's', i);
    if (!prefs.isKey(ks)) continue;
    slotKey(kp, sizeof(kp), 'p', i);
    slotKey(kq, sizeof(kq), 'q', i);
    e.used = true;
    e.ssid = prefs.getString(ks, "");
    e.psk = prefs.getString(kp, "");
    e.seq = prefs.getUInt(kq, 0);
    if (e.seq >= nextSeq_) nextSeq_ = e.seq + 1;
  }
  prefs.end();
}

uint8_t KnownNetworks::count() const {
  uint8_t n = 0;
  for (const Entry& e : slots_) n += e.used ? 1 : 0;
  return n;
}

int KnownNetworks::find(const String& ssid) const {
  for (uint8_t i = 0; i < KNOWN_NETWORKS_MAX; i++) {
    if (slots_[i].used && slots_[i].ssid == ssid) return i;
  }
  return -1;
}

int KnownNetworks::slotForNew() const {
  int oldest = 0;
  for (uint8_t i = 0; i < KNOWN_NETWORKS_MAX; i++) {
    if (!slots_[i].used) return i;
    if (slots_[i].seq < slots_[oldest].seq) oldest = i;
  }
  return oldest;
}

bool KnownNetworks::remember(const String& ssid, const String& psk) {
  if (ssid.isEmpty() || ssid.length() > SSID_MAX_LEN || psk.length() > PSK_MAX_LEN) {
    return false;
  }

  int slot = find(ssid);
  if (slot >= 0) {
    if (slots_[slot].psk == psk) return false;  // già nota, identica: niente da scrivere
    Serial.printf("[WIFI] Rete \"%s\" già nota: aggiorno la password\n", ssid.c_str());
  } else {
    slot = slotForNew();
    if (slots_[slot].used) {
      Serial.printf("[WIFI] Elenco pieno (%u reti): \"%s\" sostituisce la più vecchia, \"%s\"\n",
                    (unsigned)KNOWN_NETWORKS_MAX, ssid.c_str(), slots_[slot].ssid.c_str());
    }
  }

  char ks[6], kp[6], kq[6];
  slotKey(ks, sizeof(ks), 's', slot);
  slotKey(kp, sizeof(kp), 'p', slot);
  slotKey(kq, sizeof(kq), 'q', slot);

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  // L'SSID per ultimo: è lui che dice "slot occupato". Se la corrente salta a
  // metà scrittura, al massimo resta la rete vecchia, non uno slot mozzo.
  prefs.putString(kp, psk);
  prefs.putUInt(kq, nextSeq_);
  bool ok = prefs.putString(ks, ssid) > 0;
  prefs.end();

  if (!ok) {
    Serial.printf("[WIFI] !! impossibile salvare \"%s\" in NVS\n", ssid.c_str());
    return false;
  }

  Entry& e = slots_[slot];
  e.used = true;
  e.ssid = ssid;
  e.psk = psk;
  e.seq = nextSeq_++;
  Serial.printf("[WIFI] Rete \"%s\" salvata (%u/%u)\n", ssid.c_str(), count(),
                (unsigned)KNOWN_NETWORKS_MAX);
  return true;
}

bool KnownNetworks::importFromWifiStack() {
  if (count() > 0) return false;

  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return false;

  // I campi del driver sono array fissi, non sempre terminati da '\0'
  // (un SSID di esattamente 32 caratteri riempie tutto l'array).
  char ssid[SSID_MAX_LEN + 1] = {};
  char psk[PSK_MAX_LEN + 1] = {};
  memcpy(ssid, conf.sta.ssid, SSID_MAX_LEN);
  memcpy(psk, conf.sta.password, PSK_MAX_LEN);
  if (ssid[0] == '\0') return false;

  Serial.printf("[WIFI] Importo la rete salvata dal firmware precedente: \"%s\"\n", ssid);
  return remember(String(ssid), String(psk));
}

void KnownNetworks::clear() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  for (Entry& e : slots_) e = Entry();
  nextSeq_ = 1;
}

uint8_t KnownNetworks::matchScan(int16_t found, WifiCandidate* out, uint8_t maxOut) const {
  uint8_t n = 0;
  for (int16_t i = 0; i < found; i++) {
    int slot = find(WiFi.SSID(i));
    if (slot < 0) continue;

    int32_t rssi = WiFi.RSSI(i);

    // La stessa rete può comparire più volte (più access point, es. il WiFi
    // di una location): tengo solo l'AP che si sente meglio.
    int existing = -1;
    for (uint8_t j = 0; j < n; j++) {
      if (out[j].slot == slot) { existing = j; break; }
    }
    if (existing >= 0 && rssi <= out[existing].rssi) continue;
    if (existing < 0 && n >= maxOut) continue;

    WifiCandidate& c = out[existing >= 0 ? existing : n++];
    c.slot = (uint8_t)slot;
    c.rssi = rssi;
    c.channel = WiFi.channel(i);
    const uint8_t* bssid = WiFi.BSSID(i);
    if (bssid) memcpy(c.bssid, bssid, sizeof(c.bssid));
  }

  // Insertion sort, segnale più forte prima: sono al massimo 6 elementi.
  for (uint8_t i = 1; i < n; i++) {
    WifiCandidate key = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].rssi < key.rssi) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }
  return n;
}

void KnownNetworks::printSummary(Print& out) const {
  uint8_t n = count();
  out.printf("[WIFI] Reti note: %u/%u\n", n, (unsigned)KNOWN_NETWORKS_MAX);
  if (n == 0) return;

  // Dalla più recente alla più vecchia: l'ultima della lista è la prossima
  // a essere sostituita quando l'elenco è pieno. Le password non si stampano.
  bool printed[KNOWN_NETWORKS_MAX] = {};
  for (uint8_t k = 0; k < n; k++) {
    int best = -1;
    for (uint8_t i = 0; i < KNOWN_NETWORKS_MAX; i++) {
      if (!slots_[i].used || printed[i]) continue;
      if (best < 0 || slots_[i].seq > slots_[best].seq) best = i;
    }
    printed[best] = true;
    out.printf("[WIFI]   %u. \"%s\"%s\n", k + 1, slots_[best].ssid.c_str(),
               slots_[best].psk.isEmpty() ? "  (aperta)" : "");
  }
}
