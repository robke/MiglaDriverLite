#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <LittleFS.h>
#include <string.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
extern "C" {
  #include <user_interface.h>
}

#include "NTP.h"
#include "RTC.h"

#include "HostConfig.h"

#define SLOW_CYCLE_PERIOD_MS 250

#define CONFIG_PORTAL_TRIGGER_PIN 0
#define CONFIG_PORTAL_TIMEOUT 120000

#define PIN_DRV1_A 12
#define PIN_DRV1_B 13
#define PIN_DRV2_A 14
#define PIN_DRV2_B 15

extern HostConfig hostConfig = HostConfig();


WiFiUDP udp;
extern Ntp ntp(udp);
extern Rtc rtc(ntp);

#define HOST_CFG_SIZE 512
#define DRV_CMD_SIZE 256
const char* hostConfigFile = "/host.json";

WiFiManager wm;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;



String corsHeaderName = "Access-Control-Allow-Origin";
String corsHeaderValue = "*";
String corsMethodsHeaderName = "Access-Control-Allow-Methods";
String corsMethodsHeaderValue = "GET, POST";
String corsHeadersHeaderName = "Access-Control-Allow-Headers";
String corsHeadersHeaderValue = "Content-Type";

boolean httpSetup = false;
boolean abnormalReset = false;

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
  MDNS.addService("migladrv", "tcp", 80);

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


  httpServer.on("/driver", []() {
    httpServer.sendHeader(corsHeaderName, corsHeaderValue);
    if (mt_handleOptions()) {
      return;
    }

    DynamicJsonDocument doc(DRV_CMD_SIZE);
    String output;
    if (httpServer.method() == HTTP_GET) {
        doc["drv1"] = statDrv1();
        doc["drv2"] = statDrv2();
        serializeJson(doc, output);
        httpServer.send(200, "application/json", output);
        return;
    } else if (httpServer.method() == HTTP_POST) {
      DeserializationError error = deserializeJson(doc, httpServer.arg("plain"));
      if (error) {
        httpServer.send(400, "application/json", String(F("{ \"error\": \"Invalid JSON: ")) + error.c_str() + F("\"}"));
      } else {
        DynamicJsonDocument outDoc(DRV_CMD_SIZE);
        uint8_t drvStat = doc["drv1"];
        if (drvStat) {
          ctrlDrv1(drvStat);
          outDoc["drv1"] = drvStat;
        }
        drvStat = doc["drv2"];
        if (drvStat) {
          ctrlDrv2(drvStat);
          outDoc["drv2"] = drvStat;
        }
        serializeJson(outDoc, output);
        httpServer.send(200, "application/json", output);
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

void setupDrivers() {

  pinMode(PIN_DRV1_A, OUTPUT);
  pinMode(PIN_DRV1_B, OUTPUT);
  pinMode(PIN_DRV2_A, OUTPUT);
  pinMode(PIN_DRV2_B, OUTPUT);

  digitalWrite(PIN_DRV1_A, LOW);
  digitalWrite(PIN_DRV1_B, LOW);
  digitalWrite(PIN_DRV2_A, LOW);
  digitalWrite(PIN_DRV2_B, LOW);
  
}

void setupWifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  

  wm.setHostname(hostConfig.host);
  wm.setConnectTimeout(30); // seconds
  wm.setConnectRetries(5);
  wm.setWiFiAutoReconnect(true);
  wm.autoConnect();
}

void setup() {
  Serial.begin(115200);

  setupDrivers();

  bool success = LittleFS.begin();
  Serial.print("FS begin: ");
  Serial.println(success);

  loadHostConf();

  abnormalReset = false;


  rst_info* resetInfo;
  resetInfo = ESP.getResetInfoPtr();
  if (resetInfo->reason != REASON_EXCEPTION_RST
      && resetInfo->reason != REASON_SOFT_WDT_RST
      && resetInfo->reason != REASON_WDT_RST) {
    abnormalReset = false;
    // TODO: configure
  } else {
    Serial.println("Not loading config - abnormal reset detected.");
    abnormalReset = true;
    // TODO: do not configure
  }

  

  setupWifi();

  // setupHttp();
} // setup


unsigned long lastSlowCycle = 0;
unsigned long lastTimePrintMs = 0;

boolean configPortalRunning = false;
unsigned long configPortalStartTime = 0;


void loopWifiManager() {
  // is auto timeout portal running
  if (configPortalRunning) {
    wm.process(); // do processing

    // check for timeout
    if((millis() - configPortalStartTime) > CONFIG_PORTAL_TIMEOUT) {
      Serial.println("portaltimeout");
      configPortalRunning = false;
      wm.stopConfigPortal();
    }
  }

  // is configuration portal requested?
  if (digitalRead(CONFIG_PORTAL_TRIGGER_PIN) == LOW && (!configPortalRunning)) {
    Serial.println("Button Pressed, Starting Config Portal");
    wm.setConfigPortalBlocking(false);
    wm.startConfigPortal();
    configPortalRunning = true;
    configPortalStartTime = millis();
  }
}

void loop() {
  unsigned long currentTime = millis();

  if (!httpSetup && WL_CONNECTED == WiFi.status() && !configPortalRunning) {
    setupHttp();
  }
  
  MDNS.update();

  loopWifiManager();

  if (httpSetup) {
    httpServer.handleClient();
    
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
        lastTimePrintMs = currentTime;
      }
    }
  }
  
}


void ctrlDrv1(uint8_t stat) {

  switch (stat) {
    case 2: {
      digitalWrite(PIN_DRV1_A, HIGH);
      digitalWrite(PIN_DRV1_B, LOW);
      break;
    }
    case 3: {
      digitalWrite(PIN_DRV1_A, LOW);
      digitalWrite(PIN_DRV1_B, HIGH);
      break;
    }
    default: {
      digitalWrite(PIN_DRV1_A, LOW);
      digitalWrite(PIN_DRV1_B, LOW);
    }
  }
  
}

void ctrlDrv2(uint8_t stat) {

  switch (stat) {
    case 2: {
      digitalWrite(PIN_DRV2_A, HIGH);
      digitalWrite(PIN_DRV2_B, LOW);
      break;
    }
    case 3: {
      digitalWrite(PIN_DRV2_A, LOW);
      digitalWrite(PIN_DRV2_B, HIGH);
      break;
    }
    default: {
      digitalWrite(PIN_DRV2_A, LOW);
      digitalWrite(PIN_DRV2_B, LOW);
    }
  }
  
}

uint8_t statDrv1() {
  uint8_t a = digitalRead(PIN_DRV1_A);
  uint8_t b = digitalRead(PIN_DRV1_B);
  if (a == b) {
    return 1;
  }
  return HIGH == a ? 2 : 3;
}

uint8_t statDrv2() {
  uint8_t a = digitalRead(PIN_DRV2_A);
  uint8_t b = digitalRead(PIN_DRV2_B);
  if (a == b) {
    return 1;
  }
  return HIGH == a ? 2 : 3;
}
