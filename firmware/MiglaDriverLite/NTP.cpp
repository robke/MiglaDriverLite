
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <lwip/dns.h>

extern "C" {
  #include <user_interface.h>
}

#include "NTP.h"

#define PACK_SEND_TIMEOUT_MS 10000 
#define SEVENTY_YEARS 2208988800UL

void Ntp::init() {
  unsigned long currTime = millis();

  Serial.println("NTP init.");
  

  this->lastOpStartTime = currTime;
/*
  if (WiFi.hostByName(ntpServerName, ntpIp)) {
    Serial.print("NTP IP:\t");
    Serial.println(ntpIp);
  }
  
  this->status = NTP_STATUS_INIT_OK;
  this->lastOpStartTime = 0;
  */

  this->status = NTP_STATUS_INIT_OK;
}

bool Ntp::startTimeAcquisition() {
  if (this->status == NTP_STATUS_NOT_INIT) {
    this->init();
    return false;
  }

  unsigned long currTime = millis();

  if (this->status == NTP_STATUS_HOST_RES_START && currTime - this->lastOpStartTime > PACK_SEND_TIMEOUT_MS) {
    // packet send timeout
    Serial.println("Resolve request timeout");
    this->status = NTP_STATUS_INIT_OK;
    this->lastOpStartTime = 0;
    return false;
  }

  if (this->status == NTP_STATUS_PACK_SENT && currTime - this->lastOpStartTime > PACK_SEND_TIMEOUT_MS) {
    // packet send timeout
    Serial.println("NTP request timeout");
    this->status = NTP_STATUS_HOST_RESOLVED;
    this->lastOpStartTime = 0;
  } else if (this->status == NTP_STATUS_PACK_SENT) {
    // packet send in progress
    return false;
  }

  if (this->status == NTP_STATUS_INIT_OK || this->status == NTP_STATUS_TIME_UPDATED) {
    dns_gethostbyname(ntpServerName, &ntpIp, Ntp::onHostResolve, this);
    this->status = NTP_STATUS_HOST_RES_START;
    this->lastOpStartTime = currTime;
    Serial.println("Host resolve init.");
    return true;
  }

  return false;
}

void Ntp::onHostResolve(const char* host, const ip4_addr* addr, void *obj) {
  Serial.println("On host resolve");
  Serial.print("NTP IP:\t");
  IPAddress ipAddr(addr->addr);
  Serial.println(ipAddr);
  ((Ntp*)obj)->status = NTP_STATUS_HOST_RESOLVED;
  ((Ntp*)obj)->lastOpStartTime = 0;
  ((Ntp*)obj)->ntpIp = *addr;
}

uint32_t Ntp::checkTimeAcquisition() {
  if (this->status == NTP_STATUS_NOT_INIT || this->status == NTP_STATUS_INIT_OK || this->status == NTP_STATUS_TIME_UPDATED) {
    return 0;
  }

  unsigned long currTime = millis();

  if (this->status == NTP_STATUS_HOST_RES_START) {
    if (currTime - this->lastOpStartTime > PACK_SEND_TIMEOUT_MS) {
      Serial.println("Resolve request timeout");
      this->status = NTP_STATUS_INIT_OK;
      this->lastOpStartTime = 0;
      this->startTimeAcquisition();
      return 0;
    }
  }

  if (this->status == NTP_STATUS_HOST_RESOLVED) {
    Serial.println("NTP pack send.");
    memset(this->ntpBuffer, 0, NTP_PACKET_SIZE);
    this->ntpBuffer[0] = 0b11100011;   // LI, Version, Mode
    this->udp.beginPacket(ntpIp, 123);
    this->udp.write(ntpBuffer, NTP_PACKET_SIZE);
    this->udp.endPacket();
    this->status = NTP_STATUS_PACK_SENT;
    this->lastOpStartTime = currTime;
    return 0;
  }
  
  if (this->status == NTP_STATUS_PACK_SENT) {
    if (currTime - this->lastOpStartTime > PACK_SEND_TIMEOUT_MS) {
      // packet send timeout
      Serial.println("NTP request timeout");
      this->status = NTP_STATUS_INIT_OK;
      this->lastOpStartTime = 0;
      this->startTimeAcquisition();
      return 0;
    }
    
    if (udp.parsePacket() == 0) { // If there's no response (yet)
      return 0;
    }

    this->udp.read(this->ntpBuffer, NTP_PACKET_SIZE);
    // Combine the 4 timestamp bytes into one 32-bit number
    uint32_t ntpTime = (this->ntpBuffer[40] << 24) | (this->ntpBuffer[41] << 16) | (this->ntpBuffer[42] << 8) | this->ntpBuffer[43];
    this->status = NTP_STATUS_TIME_UPDATED;
    this->lastOpStartTime = currTime;
    Serial.print("NTP time: ");
    Serial.println(ntpTime);
    return ntpTime;
    
  }
  
  return 0;
  
}
