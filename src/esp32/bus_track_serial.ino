#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <string>
#include <vector>
#include <time.h>
#include <TM1637Display.h>

#define CLK 12
#define DIO 14
TM1637Display display = TM1637Display(CLK, DIO);

const char* ssid = "";
const char* password = "";
const char* endpoint = "https://mbus.ltp.umich.edu/bustime/api/v3/getpredictions?key=";
const char* key = "";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const long daylightOffset_sec = 0;

struct StopData {
  String routeId;
  String dir;
  String stopId;
  String vehicleId;
  String arrivalTime;
};

std::vector<std::tuple<String, String, String>> northboundFromCCTC = {
    {"BB", "NORTHBOUND", "C250"},
    {"NW", "NORTHBOUND", "C251"},
    {"CN", "NORTHBOUND", "C250"}
};

// get Json data from https GET
void get_data(const String& url, DynamicJsonDocument& doc) {
    HTTPClient http;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    int httpResponseCode = http.GET();
    // Serial.println("HTTP Response code: " + String(httpResponseCode));

    if (httpResponseCode == 302) {
        String redirectUrl = http.header("Location");
        Serial.println("Redirected to: " + redirectUrl);
    }

    if (httpResponseCode == 200) {
        String payload = http.getString();
        deserializeJson(doc, payload);
    }

    http.end();
    return;
}

// get current time
void get_current_time() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  else{
    Serial.println(&timeinfo, "Current Time - %H:%M:%S");
  }

  int curTime = timeinfo.tm_hour * 100 + timeinfo.tm_min;
  display.showNumberDecEx(curTime, 0b01000000);
}

// connect to wifi
void setup() {
  Serial.begin(115200); 

  WiFi.begin(ssid, password);
  Serial.println("Connecting");

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("... ");
  }
  Serial.println("");

  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  get_current_time();

  display.clear();
  display.setBrightness(7);
}

void loop() {
    if ((WiFi.status() == WL_CONNECTED)) { 
      std::vector<StopData> result;

      get_current_time();

      Serial.println("Calling info");
      DynamicJsonDocument doc(2048);

      for (const auto& entry : northboundFromCCTC) {
        String routeId, dir, stopId;
        std::tie(routeId, dir, stopId) = entry;

        String url = String(endpoint) + key + "&format=json&rt=" + routeId + "&stpid=" + stopId + "&tmres=s";
        get_data(url, doc);

        if (doc.containsKey("bustime-response")) {
          JsonObject response = doc["bustime-response"];
          
          if(response.containsKey("prd")){
              JsonArray predictions = response["prd"].as<JsonArray>();
              
              for (JsonObject prediction : predictions) {
                // Serial.println(prediction["vid"].as<String>());
                if (prediction["rtdir"].as<String>() == dir) {
                  StopData stopData = {
                    routeId,
                    dir,
                    stopId,
                    prediction["vid"].as<String>(),
                    prediction["prdctdn"].as<String>()
                  };
                  result.push_back(stopData);
                }
              }
          }
        }
      }

      for (const auto& stopData : result) {
        Serial.println("Route ID: " + stopData.routeId);
        Serial.println("Direction: " + stopData.dir);
        Serial.println("Stop ID: " + stopData.stopId);
        Serial.println("Vehicle ID: " + stopData.vehicleId);
        Serial.println("Arrival Time: " + stopData.arrivalTime);
        Serial.println();
      }
    
      delay(5000);
    }
}