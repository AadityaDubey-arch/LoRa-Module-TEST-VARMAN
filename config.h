// config.h — pin map and tunables for one Varman field node.
// Every device in the mesh flashes the SAME firmware; only FIXED_NODE_ID
// (or the auto-derived MAC-based ID) differs between units.
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------
// Node identity
// ---------------------------------------------------------------------
// Leave at 0 to auto-derive a short ID from the ESP32's MAC address
// (convenient, unique per board, nothing to configure).
// Set to a fixed value 1-65535 for predictable node numbers while bench
// testing (e.g. 1, 2, 3 for three units sitting on your desk).
#define FIXED_NODE_ID   0

// ---------------------------------------------------------------------
// LoRa radio (SX1262)
// ---------------------------------------------------------------------
// India's license-exempt ISM band is 865-867 MHz (WPC de-licensing
// notification), NOT 868 MHz (EU) or 915 MHz (US) — using the wrong
// region plan is a real, common mistake. Confirm the current WPC power
// and duty-cycle limits for this band before field deployment; don't
// assume EU868 or US915 rules apply.
#define LORA_FREQ_MHZ           865.0
#define LORA_BANDWIDTH_KHZ      125.0
#define LORA_SPREADING_FACTOR   10     // 7=fastest/shortest range .. 12=slowest/longest range
#define LORA_CODING_RATE        5      // 5 = 4/5
#define LORA_SYNC_WORD          0x12   // "private network" sync word (0x34 is LoRaWAN's public one — don't use it)
#define LORA_TX_POWER_DBM       20     // many SX1262 modules top out near +22 dBm — check your module's datasheet AND the legal limit for your band
#define LORA_PREAMBLE_LEN       12

// SX1262 control pins (change to match your wiring)
#define PIN_LORA_CS     5
#define PIN_LORA_RST    14
#define PIN_LORA_BUSY   27
#define PIN_LORA_DIO1   26

// Shared hardware SPI bus (LoRa radio + e-ink display both sit on this bus,
// each with its own CS line)
#define PIN_SPI_SCK     18
#define PIN_SPI_MISO    19
#define PIN_SPI_MOSI    23

// ---------------------------------------------------------------------
// E-ink display
// ---------------------------------------------------------------------
// GxEPD2 needs a *panel-specific* class selected in ui.cpp — the class
// used there is a placeholder. Match it to your exact panel from GxEPD2's
// own display-selection list before flashing.
#define PIN_EINK_CS     15
#define PIN_EINK_DC     17
#define PIN_EINK_RST    16
#define PIN_EINK_BUSY   4

// ---------------------------------------------------------------------
// GPS (NMEA over UART2)
// ---------------------------------------------------------------------
#define GPS_BAUD        9600
#define PIN_GPS_RX      33   // ESP32 RX2  <- GPS module TX
#define PIN_GPS_TX      32   // ESP32 TX2  -> GPS module RX

// ---------------------------------------------------------------------
// Membrane keypad (4x4 matrix)
// ---------------------------------------------------------------------
// ROW pins (34,35,36,39) are ESP32 input-only pins: they have NO internal
// pull resistors. Wire a 10k resistor from each row pin to 3.3V, or the
// keypad reads will float and misfire. Re-map the KEYS[][] layout in
// keypad_handler.cpp if your pad's rows/cols are wired differently (e.g.
// a 3x4 phone-style pad instead of 4x4).
#define KEYPAD_ROWS     4
#define KEYPAD_COLS     4
static const uint8_t PIN_KEYPAD_ROWS[KEYPAD_ROWS] = {34, 35, 36, 39}; // inputs, need external 10k pull-ups to 3.3V
static const uint8_t PIN_KEYPAD_COLS[KEYPAD_COLS] = {13, 21, 22, 25}; // outputs
