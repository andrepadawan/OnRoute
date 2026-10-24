#include "Uploader.h"
#include "config.h"

void Uploader::begin() {
  // DEBITO CONSAPEVOLE (solo firmware di test): la connessione è cifrata ma
  // l'identità del server non viene verificata.
  
  tlsClient_.setInsecure();

  http_.setReuse(true);  // keep-alive: evita un handshake TLS completo a ogni POST
  http_.setTimeout(HTTP_TIMEOUT_MS);
  http_.setConnectTimeout(HTTP_TIMEOUT_MS);
}

int Uploader::post(const String& url, const String& token, const Coordinates& coords) {
  JsonDocument doc;
  coords.toJson(doc.to<JsonObject>());
  String body;
  serializeJson(doc, body);

  WiFiClient& client = url.startsWith("https://") ? tlsClient_ : plainClient_;
  if (!http_.begin(client, url)) {
    Serial.printf("[HTTP] URL non valido: \"%s\"\n", url.c_str());
    return ERR_BAD_URL;
  }
  http_.addHeader("Content-Type", "application/json");
  http_.addHeader("Authorization", "Bearer " + token);

  uint32_t t0 = millis();
  int code = http_.POST(body);
  uint32_t elapsed = millis() - t0;

  String response = code > 0 ? http_.getString() : String();
  http_.end();

  Serial.printf("[HTTP] POST %s -> %d (%lu ms)\n", body.c_str(), code, (unsigned long)elapsed);
  if (code < 200 || code >= 300) {
    if (response.length() > 0) {
      Serial.printf("[HTTP] risposta: %s\n", response.substring(0, 300).c_str());
    }
    explain(code);
  }
  return code;
}

// Traduce il codice in "cosa controllare". È la parte che fa risparmiare ore.
void Uploader::explain(int code) {
  const char* hint;
  switch (code) {
    case 301: case 302: case 307: case 308:
      hint = "redirect: usa https:// nell'URL"; break;
    case 401:
      hint = "token rifiutato: DEVICE_TOKEN nel .env != DEVICE_TOKEN su Railway"; break;
    case 404:
      hint = "endpoint inesistente: l'URL deve finire con /update-coordinates"; break;
    case 405:
      hint = "metodo non ammesso: stai colpendo un endpoint GET"; break;
    case 422:
      hint = "payload rifiutato da Pydantic: il JSON non combacia con PayloadReceived (leggi 'detail')"; break;
    case 500: case 502: case 503: case 504:
      hint = "errore lato server: guarda i log di Railway"; break;
    case ERR_BAD_URL:
      hint = "URL vuoto o malformato: controlla .env o reinseriscilo dal portale"; break;
    default:
      if (code < 0) {
        // errorToString() restituisce una String temporanea: la usiamo subito,
        // senza conservarne il puntatore.
        Serial.printf("[HTTP] -> errore di rete: %s (hotspot senza dati? DNS? server spento?)\n",
                      HTTPClient::errorToString(code).c_str());
        return;
      }
      hint = "codice inatteso";
  }
  Serial.printf("[HTTP] -> %s\n", hint);
}
