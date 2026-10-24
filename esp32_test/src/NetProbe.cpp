#include "NetProbe.h"
#include <WiFi.h>
#include "config.h"

bool NetProbe::parseHostPort(const String& url, char* host, size_t hostLen, uint16_t& port) {
  int start;
  if (url.startsWith("https://"))      { start = 8; port = 443; }
  else if (url.startsWith("http://"))  { start = 7; port = 80; }
  else return false;

  int end = start;
  while (end < (int)url.length() && url[end] != ':' && url[end] != '/') end++;
  if (end == start || (size_t)(end - start) >= hostLen) return false;

  String h = url.substring(start, end);
  strncpy(host, h.c_str(), hostLen - 1);
  host[hostLen - 1] = '\0';

  if (end < (int)url.length() && url[end] == ':') {
    long p = url.substring(end + 1).toInt();  // toInt si ferma al primo non-numero ("/")
    if (p <= 0 || p > 65535) return false;
    port = (uint16_t)p;
  }
  return true;
}

void NetProbe::begin(const String& serverUrl) {
  if (!parseHostPort(serverUrl, host_, sizeof(host_), port_)) {
    Serial.println("[NET] URL non interpretabile: niente verifica internet");
    return;
  }
  Serial.printf("[NET] Verifica internet verso %s:%u ogni %lu s\n",
                host_, port_, (unsigned long)(NET_PROBE_INTERVAL_MS / 1000));
  // 4 KB di stack bastano per DNS + connect. Priorità 1 = come loop().
  xTaskCreatePinnedToCore(taskEntry, "netprobe", 4096, this, 1, nullptr, 0);
}

void NetProbe::taskEntry(void* self) {
  static_cast<NetProbe*>(self)->run();  // non ritorna mai
}

void NetProbe::run() {
  bool lastReported = true;
  for (;;) {
    uint32_t now = millis();
    bool httpProofFresh = lastHttpProofMs_ != 0 && now - lastHttpProofMs_ < NET_PROBE_INTERVAL_MS;

    if (WiFi.status() != WL_CONNECTED) {
      probeOk_ = false;
      probeAtMs_ = now;
    } else if (!httpProofFresh) {  // se un POST ha appena risposto, non serve sprecare dati
      WiFiClient client;
      bool ok = client.connect(host_, port_, NET_PROBE_TIMEOUT_MS);
      client.stop();
      probeOk_ = ok;
      probeAtMs_ = millis();
      if (ok != lastReported) {
        Serial.printf("[NET] Internet %s\n", ok ? "raggiungibile" : "NON raggiungibile (hotspot senza dati?)");
        lastReported = ok;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(NET_PROBE_INTERVAL_MS));
  }
}

NetState NetProbe::state() const {
  if (WiFi.status() != WL_CONNECTED) return NetState::Offline;

  uint32_t now = millis();
  uint32_t httpProof = lastHttpProofMs_;
  if (httpProof != 0 && now - httpProof < NET_PROOF_FRESH_MS) return NetState::Online;

  if (probeAtMs_ == 0) return NetState::Unknown;
  return probeOk_ ? NetState::Online : NetState::Offline;
}
