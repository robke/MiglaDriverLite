
extern "C" {
  #include <user_interface.h>
}

#include "RTC.h"

#define RTC_NTP_UPDATE_INTERVAL_MS 3600000
#define RTC_NTP_UPDATE_TIMEOUT_MS 1000

void Rtc::init() {
  Serial.println("RTC init.");
}

void Rtc::update() {
  unsigned long currentTimeMillis = millis();
  if (this->status == RTC_STATUS_NOTIME || currentTimeMillis - this->lastUpdTimeMillis > RTC_NTP_UPDATE_INTERVAL_MS) {
    if (this->ntpStatus == RTC_NTP_STATUS_NONE) {
      if (this->ntp.startTimeAcquisition()) {
        this->ntpStatus = RTC_NTP_STATUS_REQUESTED;
      }
    } else if (this->ntpStatus == RTC_NTP_STATUS_REQUESTED) {
      uint32_t ntpTime = this->ntp.checkTimeAcquisition();
      if (ntpTime) {
        this->lastUpdNtpTime = ntpTime;
        this->lastUpdTimeMillis = currentTimeMillis;
        this->status = RTC_STATUS_HASTIME;
        this->ntpStatus = RTC_NTP_STATUS_NONE;
      }
    }
  }
}
