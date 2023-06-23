
#ifndef __HOST_CONFIG_H
#define __HOST_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

class HostConfig {
  public:
    char name[32];
    char host[32];

    void loadFromJson(JsonDocument& doc) {
      if (doc["name"])
        strlcpy(this->name, doc["name"], sizeof(this->name));
      if (doc["host"])
        strlcpy(this->host, doc["host"], sizeof(this->host));
      
    }
    
    void saveToJson(JsonDocument& doc) {
      doc["name"] = this->name;
      doc["host"] = this->host;
    }

    void resetToDefault() {
      strlcpy(this->name, "New MDRV device", sizeof(this->name));
      strlcpy(this->host, "MDRV-NEW", sizeof(this->host));
    }
};


#endif __HOST_CONFIG_H
