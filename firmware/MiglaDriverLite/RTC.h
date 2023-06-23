

#ifndef __RTC_H
#define __RTC_H

#include <Arduino.h>
#include "NTP.h"

#define RTC_STATUS_NOTIME 0x0
#define RTC_STATUS_HASTIME 0x1
#define RTC_STATUS_CALIBRATED 0x2

#define RTC_NTP_STATUS_NONE 0x0
#define RTC_NTP_STATUS_REQUESTED 0x1

#define RTC_NTP_UPDATE_TIMEOUT_SEC 14400 // 4 hours



class Rtc {
  public:
    Rtc(Ntp &ntp): ntp(ntp) {};
    void init();
    void update();
    inline uint32_t getCurrentNtpTimestamp() {
      unsigned long lastNtpUpdate = (millis() - this->lastUpdTimeMillis) / 1000;
      /*
      if (lastNtpUpdate > RTC_NTP_UPDATE_TIMEOUT_SEC) {
        this->status = RTC_STATUS_NOTIME;
      }
      */
      return this->status == RTC_STATUS_NOTIME ? 0 : lastUpdNtpTime + lastNtpUpdate;
    }
    inline NtpTime getCurrentTimeStruct() {
      if (this->status == RTC_STATUS_NOTIME) {
        return NtpTime();
      } else {
        return NtpTime(this->getCurrentNtpTimestamp());
      }
    }
  
  protected:
    Ntp &ntp;
    byte status = RTC_STATUS_NOTIME;
    byte ntpStatus = RTC_NTP_STATUS_NONE;
    uint32_t lastUpdNtpTime = 0;
    unsigned long lastUpdTimeMillis = 0;
   
};


#endif __RTC_H
