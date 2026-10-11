// gps_helper.cpp — requires the "TinyGPSPlus" library (Mikal Hart).
//
// Most NEO-6M / NEO-M8N breakout boards already level-shift to 3.3V
// logic and are safe to wire directly to the ESP32's UART — double check
// your specific module before assuming that, though.
#include "gps_helper.h"
#include "config.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>

static TinyGPSPlus   s_gps;
static HardwareSerial s_gpsSerial(2); // ESP32 UART2

void gps_init() {
  s_gpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

void gps_update() {
  while (s_gpsSerial.available()) {
    s_gps.encode(s_gpsSerial.read());
  }
}

// Howard Hinnant's well-known "days_from_civil" algorithm, extended with
// a time-of-day term. Proleptic Gregorian, UTC, integer-only — correct
// regardless of the host's libc timezone setting, which is why this is
// used instead of mktime().
uint32_t civil_to_epoch(int y, int m, int d, int hh, int mm, int ss) {
  y -= (m <= 2) ? 1 : 0;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;                                      // [0, 399]
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;      // [0, 365]
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;               // [0, 146096]
  int64_t days = era * 146097 + doe - 719468;                       // days since 1970-01-01
  return (uint32_t)(days * 86400LL + hh * 3600LL + mm * 60LL + ss);
}

gps_fix_t gps_get() {
  gps_fix_t f;
  f.fix = s_gps.location.isValid() && s_gps.location.age() < 5000;
  f.lat = f.fix ? s_gps.location.lat() : 0.0;
  f.lon = f.fix ? s_gps.location.lng() : 0.0;
  f.epoch = 0;
  if (s_gps.date.isValid() && s_gps.time.isValid()) {
    f.epoch = civil_to_epoch(s_gps.date.year(), s_gps.date.month(), s_gps.date.day(),
                              s_gps.time.hour(), s_gps.time.minute(), s_gps.time.second());
  }
  return f;
}
