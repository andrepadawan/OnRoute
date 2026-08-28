/**
 * Blink
 *
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */

#include "FS.h"
#include "Arduino.h"
#include "WiFiManager.h"
#define LED 2


#ifdef ESP32
  #include "SPIFFS.h"
#endif

#include "ArduinoJson.h"

// JSON configuration file
#define JSON_CONFIG_FILE "/test_config.json"
bool shouldSaveConfig = false;
//Se sono diversi nel config.json, verrà sovrascritto
char api_token[34] = "";

WiFiManager wm;

void saveConfigCallback(){//Notifica in caso si debba salvare config.json
  Serial.print("Should save config");
  shouldSaveConfig = true;
}


void setup()
{
  pinMode(LED, OUTPUT);
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.println("\n Starting");
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file esiste, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif

        Serial.println("\nparsed json");

          strcpy(api_token, json["api_token"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  
  WiFiManagerParameter custom_token("Codice Token", "Inserisci il token", api_token , 32);
  wm.setSaveConfigCallback(saveConfigCallback);

  wm.addParameter(&custom_token);

  bool res;
  res = wm.autoConnect("Tracker connect... ");

  if(!res) {
        Serial.println("Failed to connect");
        ESP.restart();
        delay(1000);
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected to {}:)");
    }

     Serial.println("connected...:)");

  //read updated parameters

  strcpy(api_token, custom_token.getValue());
  Serial.println("The values in the file are: ");
  Serial.println("\tapi_token : " + String(api_token));

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
 #if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["api_token"] = api_token;
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }
}

void loop()
{
    // put your main code here, to run repeatedly:
  digitalWrite(LED, HIGH);
  Serial.println("LED is on");
  delay(1000);
  digitalWrite(LED, LOW);
  Serial.println("LED is off");
  delay(1000);
}