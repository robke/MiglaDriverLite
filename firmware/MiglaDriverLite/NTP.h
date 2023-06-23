

#ifndef __NTP_H
#define __NTP_H

#include <WiFiUdp.h>
#include <Arduino.h>

#define NTP_PACKET_SIZE 48  // NTP time stamp is in the first 48 bytes of the message

#define NTP_STATUS_NOT_INIT 0
#define NTP_STATUS_INIT_OK 1
#define NTP_STATUS_HOST_RES_START 2
#define NTP_STATUS_HOST_RESOLVED 3
#define NTP_STATUS_PACK_SENT 4
#define NTP_STATUS_TIME_UPDATED 5

class NtpTime {
  public:
    NtpTime(): ntpTime(-1) {}
    NtpTime(uint32_t ntpTimestamp) : ntpTime(ntpTimestamp % 86400) { }
    const int32_t ntpTime;
    inline bool isDefined() {
      return ntpTime >= 0;
    }
    inline byte getHours() {
      return isDefined() ? (this->ntpTime % 86400) / 3600 : 255;
    }
    inline byte getMinutes() {
      return isDefined() ? (this->ntpTime % 3600) / 60 : 255;
    }
    inline byte getSeconds() {
      return isDefined() ? this->ntpTime % 60 : 255;
    }
    inline bool operator==(const NtpTime& other) const {
      return this->ntpTime == other.ntpTime;
    }
    inline bool operator!=(const NtpTime& other) const {
      return this->ntpTime != other.ntpTime;
    }
    inline bool operator<(const NtpTime& other) const {
      return this->ntpTime < other.ntpTime;
    }
    inline bool operator<=(const NtpTime& other) const {
      return this->ntpTime <= other.ntpTime;
    }
    inline bool operator>(const NtpTime& other) const {
      return this->ntpTime > other.ntpTime;
    }
    inline bool operator>=(const NtpTime& other) const {
      return this->ntpTime >= other.ntpTime;
    }
};

class NtpTimePeriod {
  public:
    NtpTime start;
    NtpTime end;
    NtpTimePeriod(NtpTime start, NtpTime end): start(start), end(end) { }
    inline bool isDefined() {
      return start.isDefined() && end.isDefined();
    }
    inline bool timeInPeriod(NtpTime time) {
      if (start < end) {
        return time >= start && time < end;
      } else {
        return time >= start || time < end;
      }
    }
};

class Ntp {
  public:
    Ntp(WiFiUDP& udp) : udp(udp) {}
    void init();
    bool startTimeAcquisition();
    uint32_t checkTimeAcquisition();
  
  protected:
    WiFiUDP& udp;

    byte status = NTP_STATUS_NOT_INIT;
    ip_addr_t ntpIp;
    const char* ntpServerName = "europe.pool.ntp.org";
    byte ntpBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
    long lastOpStartTime = 0;

    static void onHostResolve(const char*, const ip4_addr*, void *obj);
};


#endif __NTP_H
