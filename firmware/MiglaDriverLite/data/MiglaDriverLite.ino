#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>
#include <string.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
extern "C" {
  #include <user_interface.h>
}

#include "NTP.h"
#include "RTC.h"

// Using library https://github.com/Makuna/NeoPixelBus/wiki
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>

#include "HostConfig.h"
#include "HwConfig.h"
#include "Generator.h"

#define LIGHTS_LOOP_INTERVAL 25

extern HostConfig hostConfig = HostConfig();
extern HwConfig hwConfig = HwConfig();


WiFiUDP udp;
extern Ntp ntp(udp);
extern Rtc rtc(ntp);

/*
extern const uint16_t pixelCount = 50;
//will use GPIO2 pint (UTX1)
extern NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod> strip(pixelCount, -1);
NeoPixelAnimator animations(1);
*/
Generator *pGenerator = NULL;
// Decorator *pDecorator = NULL;

#define SLOW_CYCLE_PERIOD_MS 250

#define HOST_CFG_SIZE 512
#define HW_CFG_SIZE 512
#define LIGHTS_CFG_SIZE 4096
//#define HOST_CFG_SIZE 256
//#define LIGHTS_CFG_SIZE 1024
const char* hostConfigFile = "/host.json";
const char* hwConfigFile = "/hw.json";
const char* lightsConfigFile = "/lights.json";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


String corsHeaderName = "Access-Control-Allow-Origin";
String corsHeaderValue = "*";
String corsMethodsHeaderName = "Access-Control-Allow-Methods";
String corsMethodsHeaderValue = "GET, POST";
String corsHeadersHeaderName = "Access-Control-Allow-Headers";
String corsHeadersHeaderValue = "Content-Type";

unsigned long lastSlowCycle = 0;

unsigned long wifiLastChecked = 0;
unsigned long lightsLastUpdated = 0;
boolean httpSetup = false;
boolean abnormalReset = false;
boolean stripInitOk = false;

// ScheduledDecorator* pdsd = new ScheduledDecorator();


void loadHostConf() {
  File file = LittleFS.open(hostConfigFile, "r");
  StaticJsonDocument<HOST_CFG_SIZE> doc;

  hostConfig.resetToDefault();
  
  ReadBufferingStream bufferedFile(file, 64);
  DeserializationError error = deserializeJson(doc, bufferedFile);
  file.close();
  if (error) {
    Serial.print(F("Failed to read host conf file: "));
    Serial.println(error.c_str());
    saveHostConf();
  } else {
    Serial.println(F("Host config file read OK."));
    hostConfig.loadFromJson(doc);
  }
  
}

void saveHostConf() {
  File file = LittleFS.open(hostConfigFile, "w");
  if (!file) {
    Serial.println(F("Failed to create host config file"));
    return;
  }
  
  StaticJsonDocument<HOST_CFG_SIZE> doc;
  
  hostConfig.saveToJson(doc);

  WriteBufferingStream bufferedStream(file, 64);
  if (serializeJson(doc, bufferedStream) == 0) {
    Serial.println(F("Failed to write HOST config to file"));
  }
  bufferedStream.flush();
  file.close();
  Serial.println(F("Host config file written OK."));
}

void loadHwConf() {
  File file = LittleFS.open(hwConfigFile, "r");
  StaticJsonDocument<HW_CFG_SIZE> doc;

  hwConfig.resetToDefault();
  
  ReadBufferingStream bufferedFile(file, 64);
  DeserializationError error = deserializeJson(doc, bufferedFile);
  file.close();
  if (error) {
    Serial.print(F("Failed to read HW conf file: "));
    Serial.println(error.c_str());
    saveHwConf();
  } else {
    Serial.println(F("HW config file read OK."));
    hwConfig.loadFromJson(doc);
  }
  
}

void saveHwConf() {
  File file = LittleFS.open(hwConfigFile, "w");
  if (!file) {
    Serial.println(F("Failed to create HW config file"));
    return;
  }
  
  StaticJsonDocument<HW_CFG_SIZE> doc;
  
  hwConfig.saveToJson(doc);

  WriteBufferingStream bufferedStream(file, 64);
  if (serializeJson(doc, bufferedStream) == 0) {
    Serial.println(F("Failed to write HW config to file"));
  }
  bufferedStream.flush();
  file.close();
  Serial.println(F("HW config file written OK."));
}

void loadLightsConf() {
  /*
  File ff = LittleFS.open(lightsConfigFile, "r");
  Serial.println("--");
  while (ff.available()) {
    Serial.write(ff.read());  
  }
  Serial.println("--");
  
  ff.close();
  */
  // Open file for reading
  File file = LittleFS.open(lightsConfigFile, "r");
  DynamicJsonDocument doc(LIGHTS_CFG_SIZE);

  ReadBufferingStream bufferedFile(file, 64);
  // ReadLoggingStream loggingStream(bufferedFile, Serial);
  DeserializationError error = deserializeJson(doc, bufferedFile);
  file.close();
  if (error) {
    Serial.print(F("Failed to read lights conf file: "));
    Serial.println(error.c_str());
  } else {
    Serial.println(F("Loaded lights config, applying..."));
    
    if (pGenerator != NULL) {
      Generator* plGen = pGenerator;
      pGenerator = NULL;
      Generator::unload(plGen);
    }
    
    pGenerator = Generator::load(doc);
    
    if (pGenerator) {
      Serial.println(F("Lights config applied."));
    } else {
      Serial.println(F("No suitable lights config."));
    }
  }
  
}

boolean saveLightsConf(JsonDocument& doc) {
  File file = LittleFS.open(lightsConfigFile, "w");
  if (!file) {
    Serial.println(F("Failed to create lights config file"));
    return false;
  }

  WriteBufferingStream bufferedStream(file, 64);
  if (serializeJson(doc, bufferedStream) == 0) {
    file.close();
    Serial.println(F("Failed to write to file"));
    return false;
  }
  bufferedStream.flush();

  file.close();
  Serial.println(F("Lights config file written OK."));
  return true;
}




void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(hostConfig.host);
  WiFi.begin(hostConfig.ssid, hostConfig.passwd);
}

boolean mt_handleOptions() {
    if (httpServer.method() == HTTP_OPTIONS) {
        httpServer.sendHeader(corsMethodsHeaderName, corsMethodsHeaderValue);
        httpServer.sendHeader(corsHeadersHeaderName, corsHeadersHeaderValue);
        httpServer.send(200, "text/plain", "");
        return true;
    } else {
        return false;
    }
}

void setupHttp() {
  MDNS.begin(hostConfig.host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("maglight", "tcp", 80);

  httpServer.on("/host", []() {
    httpServer.sendHeader(corsHeaderName, corsHeaderValue);
    if (mt_handleOptions()) {
      return;
    }

    DynamicJsonDocument doc(HOST_CFG_SIZE);
    String output;
    if (httpServer.method() == HTTP_GET) {
        hostConfig.saveToJson(doc);
        serializeJson(doc, output);
        httpServer.send(200, "application/json", output);
        return;
    } else if (httpServer.method() == HTTP_POST) {
      DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));
      if (error) {
        httpServer.send(400, "application/json", String("{ \"error\": \"Invalid JSON: ") + error.c_str() + "\"}");
      } else {
        hostConfig.loadFromJson(doc);
        saveHostConf();
        serializeJson(doc, output);
        httpServer.send(200, "application/json", output);
      }
      return;
    }

    httpServer.send(400, "application/json", F("{ \"error\": \"Invalid request\"}"));
    // httpServer.send(200, "text/plain", "POST body was:\n" + httpServer.arg("plain"));

    return;
  });

  httpServer.on("/hw", []() {
    httpServer.sendHeader(corsHeaderName, corsHeaderValue);
    if (mt_handleOptions()) {
      return;
    }

    DynamicJsonDocument doc(HW_CFG_SIZE);
    String output;
    if (httpServer.method() == HTTP_GET) {
        hwConfig.saveToJson(doc);
        serializeJson(doc, output);
        httpServer.send(200, "application/json", output);
        return;
    } else if (httpServer.method() == HTTP_POST) {
      DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));
      if (error) {
        httpServer.send(400, "application/json", String("{ \"error\": \"Invalid JSON: ") + error.c_str() + "\"}");
      } else {
        hwConfig.loadFromJson(doc);
        saveHwConf();
        serializeJson(doc, output);
        httpServer.send(200, "application/json", output);
      }
      return;
    }

    httpServer.send(400, "application/json", F("{ \"error\": \"Invalid request\"}"));
    // httpServer.send(200, "text/plain", "POST body was:\n" + httpServer.arg("plain"));

    return;
  });

  httpServer.on("/lights", []() {
    httpServer.sendHeader(corsHeaderName, corsHeaderValue);
    if (mt_handleOptions()) {
      return;
    }

    DynamicJsonDocument doc(LIGHTS_CFG_SIZE);
    String output;
    if (httpServer.method() == HTTP_GET) {
        File file = LittleFS.open(lightsConfigFile, "r");
        if (!file) {
            httpServer.send(200, "application/json", F("{}"));
            return;
        }
        
        ReadBufferingStream bufferedFile(file, 64);
        DeserializationError error = deserializeJson(doc, bufferedFile);
        file.close();
        if (error) {
          httpServer.send(500, "application/json", F("{ \"error\": \"Failed to read stored configuration.\"}"));
          return;
        } else {
          serializeJson(doc, output);
          httpServer.send(200, "application/json", output);
          return;
        }
    } else if (httpServer.method() == HTTP_POST) {
      DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));
      if (error) {
        httpServer.send(400, "application/json", String("{ \"error\": \"Invalid JSON: ") + error.c_str() + "\"}");
      } else {
        if (!saveLightsConf(doc)) {
          httpServer.send(500, "application/json", F("{ \"error\": \"Failed to write config.\"}"));
        } else {
          loadLightsConf();
          String output;
          serializeJson(doc, output);
          httpServer.send(200, "application/json", output);
        }
      }
      return;
    }

    httpServer.send(400, "application/json", F("{ \"error\": \"Invalid request\"}"));
    // httpServer.send(200, "text/plain", "POST body was:\n" + httpServer.arg("plain"));

    return;
  });
  
  
  httpSetup = true;

  udp.begin(123);
  ntp.init();
  rtc.init();
}

/*
void setupNtp() {
  if (!httpSetup) {
    return;
  }

  
  
}

void updateNtpTime() {
  unsigned long currTime = millis();
  if (ntpSetUp && (!timeUpdatedMs || currTime - timeUpdatedMs > 3600000)) {
    // TODO:
  }
}

void sendNTPpacket(IPAddress& address) {
  memset(ntpBuffer, 0, NTP_PACKET_SIZE);
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  UDP.beginPacket(ntpIp, 123);
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}
*/

void setup() {
  Serial.begin(115200);

  // bool success = LittleFS.format();
  // Serial.print("FS format: ");
  // Serial.println(success);
  bool success = LittleFS.begin();
  Serial.print("FS begin: ");
  Serial.println(success);
  
  // SPIFFS.begin();

  loadHwConf();
  loadHostConf();

  abnormalReset = false;
  initStrip();
  // setupLights();


  rst_info* resetInfo;
  resetInfo = ESP.getResetInfoPtr();
  if (resetInfo->reason != REASON_EXCEPTION_RST
      && resetInfo->reason != REASON_SOFT_WDT_RST
      && resetInfo->reason != REASON_WDT_RST) {
    abnormalReset = false;
    initStrip();
    setupLights();
  } else {
    Serial.println("Not loading lights config - abnormal reset detected.");
    abnormalReset = true;
    disableStrip();
  }

  

  setupWifi();

  // setupHttp();
}


void initStrip() {
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  RgbColor black(0, 0, 0);
  stripInitOk = hwConfig.initStrip();
  if (stripInitOk) {
    for (int i = 0; i < hwConfig.pixelCount; i++) {
      hwConfig.SetPixelColor(i, black);
    }
    
    delay(100);
    hwConfig.Show();
  }
}

void disableStrip() {
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
}

void setupLights() {
  loadLightsConf();
}


unsigned long lastTimePrintMs = 0;

void loop() {

  /*
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    // Serial.println("WiFi failed, retrying.");
    WiFi.begin(ssid, password);
    delay(100);
  }
  */
  unsigned long currentTime = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (currentTime - wifiLastChecked > 60000) {
      setupWifi();
      wifiLastChecked = currentTime;
    }
  } else if (!httpSetup) {
    setupHttp();
  }

  if (httpSetup) {
    httpServer.handleClient();
    MDNS.update();
    if (currentTime - lastSlowCycle > SLOW_CYCLE_PERIOD_MS && !abnormalReset) {
      lastSlowCycle = currentTime;
      rtc.update();
      if (currentTime - lastTimePrintMs > 10000) {
        NtpTime time = rtc.getCurrentTimeStruct();
        Serial.print("Time: ");
        Serial.print(time.getHours());
        Serial.print(":");
        Serial.print(time.getMinutes());
        Serial.print(":");
        Serial.println(time.getSeconds());
        /*NtpTimePeriod period(75180, 75240);
        Serial.println(period.timeInPeriod(time));*/
        lastTimePrintMs = currentTime;
      }
    }
  }

  if (!abnormalReset && stripInitOk) {
    if (currentTime - lightsLastUpdated > LIGHTS_LOOP_INTERVAL && pGenerator) {
      if (pGenerator->loop()) {
        yield();
        hwConfig.Show();
        lightsLastUpdated = currentTime;
      }
    }
  }
  
}
