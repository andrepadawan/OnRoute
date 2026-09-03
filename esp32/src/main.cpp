
/* class Coordinates(BaseModel):
    longitude: float
    latitude: float
    speed: float
    fix_status: int
    track: float
    time_of_acquisition: str
    model_config = ConfigDict(frozen = True)
    
    "
*/

#include "FS.h"
#include "Arduino.h"
#include "WiFiManager.h"
#include "WiFiClient.h"
#include "HTTPClient.h"
#include "TinyGPS++.h"
#include "string.h"
#include "stdlib.h"

#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

#ifdef ESP32
  #include "SPIFFS.h"
#endif

#include "ArduinoJson.h"
// JSON configuration file
#define JSON_CONFIG_FILE "/test_config.json"

bool shouldSaveConfig = false;
//Se sono diversi nel config.json, verrà sovrascritto
String api_token = getenv("DEVICE_TOKEN");
String http_post = getenv("URL_WEBSITE");

WiFiManager wm;
TinyGPSPlus gps;
HTTPClient http;
WiFiClient wfc;
JsonDocument doc;

HardwareSerial gpsSerial(2); //GPS instance

class Coordinates {
  public:
    double longitude;
    double latitude;
    double speed;
    uint32_t fix_status;
    double track;
    char time_of_acquisition[256];
  
    void toJson(JsonObject obj) const {
      obj["longitude"] = longitude;
      obj["latitude"] = latitude;
      obj["speed"] = speed;
      obj["fix_status"] = fix_status;
      obj["track"] = track;
      obj["time_of_acquisition"] = time_of_acquisition;
    }
};

void saveConfigCallback(){//Notifica in caso si debba salvare config.json
  Serial.print("Should save config");
  shouldSaveConfig = true;
}

void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.println("\n Starting");
  Serial.println("mounting FS...");
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

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

          json["api_token"] = api_token;
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

  
  WiFiManagerParameter custom_token("Codice Token", "Inserisci il token", api_token.c_str(), 32);
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

  api_token = custom_token.getValue();
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
  Coordinates coord;
    // put your main code here, to run repeatedly:
  while (gpsSerial.available() > 0){
    // get the byte data from the GPS
    gps.encode(gpsSerial.read());
    }

  delay(2000);
  if (gps.location.isUpdated()) {
      Serial.print("LAT: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("LONG: "); 
      Serial.println(gps.location.lng(), 6);
      Serial.print("SPEED (km/h) = "); 
      Serial.println(gps.speed.kmph()); 
      Serial.print("ALT (min)= "); 
      Serial.println(gps.altitude.meters());
      Serial.print("HDOP = "); 
      Serial.println(gps.hdop.value() / 100.0); 
      Serial.print("Satellites = "); 
      Serial.println(gps.satellites.value()); 
      Serial.print("Time in UTC: ");
      Serial.println(String(gps.date.year()) + "/" + String(gps.date.month()) + "/" + String(gps.date.day()) + "," + String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()));
      Serial.println("");

      coord.latitude = gps.location.lat();
      coord.longitude = gps.location.lng();
      coord.speed = gps.speed.kmph();
      snprintf(coord.time_of_acquisition, sizeof(coord.time_of_acquisition), "%04d-%02d-%02d %02dh:%02dm:%02ds", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour() , gps.time.minute(), gps.time.second());

      Serial.println("-------------------------------");
      http.begin(wfc, http_post);
      http.addHeader("Authorization", "Bearer "+ String(api_token));
      http.addHeader("Content-Type", "application/json");
      // Data to send with HTTP POST: JSON
      JsonObject obj = doc.to<JsonObject>();
      coord.toJson(obj);
      String payload;
      serializeJson(obj, payload);
      // Send HTTP POST request
      int httpResponseCode = http.POST(payload);
     
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // Free resources
      http.end();
  }
}