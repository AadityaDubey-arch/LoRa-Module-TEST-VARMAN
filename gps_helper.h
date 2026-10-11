// gps_helper.h — thin wrapper around TinyGPSPlus exposing just what the
// rest of the firmware needs.
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  bool     fix;   // true if we have a recent, valid fix
  double   lat;
  double   lon;
  uint32_t epoch; // UTC unix seconds, 0 if date/time not yet known
} gps_fix_t;

void      gps_init();
void      gps_update();  // call every loop(); feeds any available NMEA bytes to the parser
gps_fix_t gps_get();

// Exposed for unit testing (see logic_test/test_mesh_logic.cpp) — pure
// integer math, no timezone/libc dependency. UTC, proleptic Gregorian,
// no leap seconds (fine for logging/display, not for precision timing).
uint32_t civil_to_epoch(int year, int month, int day, int hh, int mm, int ss);
