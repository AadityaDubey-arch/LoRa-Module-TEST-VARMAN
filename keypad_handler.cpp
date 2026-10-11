// keypad_handler.cpp — requires the "Keypad" library (Mark Stanley /
// Alexander Brevig). 4x4 layout: 1-5 are the emergency presets, '#'
// confirms, '*' cancels/opens diagnostics. Re-map KEYS[][] below if your
// physical pad's legend differs (e.g. a 3x4 phone-style pad).
//
// IMPORTANT: row pins 34/35/36/39 (see config.h) are ESP32 input-only
// pins with no internal pull resistor. This library normally enables an
// internal pull-up on row pins — on these four pins that silently does
// nothing, so you MUST add an external 10k pull-up to 3.3V on each row
// line, or key reads will be unreliable.
#include "keypad_handler.h"
#include "config.h"
#include <Keypad.h>
#include <string.h>

static char KEYS[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

static uint8_t rowPins[KEYPAD_ROWS];
static uint8_t colPins[KEYPAD_COLS];
static Keypad *s_kp = nullptr;

void keypad_init() {
  memcpy(rowPins, PIN_KEYPAD_ROWS, sizeof(rowPins));
  memcpy(colPins, PIN_KEYPAD_COLS, sizeof(colPins));
  s_kp = new Keypad(makeKeymap(KEYS), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);
  s_kp->setDebounceTime(25);
}

char keypad_get_key() {
  return s_kp->getKey(); // 0 (NO_KEY) if nothing pressed
}
