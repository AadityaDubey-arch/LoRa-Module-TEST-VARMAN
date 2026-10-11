// varman_node.ino — Varman LoRa mesh field unit.
// ESP32 + SX1262 (RadioLib) + e-ink (GxEPD2) + GPS (TinyGPSPlus) +
// 4x4 membrane keypad (Keypad).
//
// Flash the SAME firmware to every device — see config.h for the one
// setting (FIXED_NODE_ID) that's meant to differ between units, and
// README.md for wiring, library list, and protocol notes.
#include <Arduino.h>
#include "config.h"
#include "mesh.h"
#include "ui.h"
#include "gps_helper.h"
#include "keypad_handler.h"

enum AppState {
  STATE_HOME,
  STATE_CONFIRM,
  STATE_SENT,
  STATE_ACCEPTED_NOTICE,
  STATE_DIAG,
};

static AppState        s_state = STATE_HOME;
static mesh_msg_type_t s_pendingType;
static uint16_t        s_sentCount = 0;
static uint16_t        s_meshSeenCount = 0; // approximate activity counter, see README

static uint16_t deriveNodeId() {
#if FIXED_NODE_ID != 0
  return FIXED_NODE_ID;
#else
  uint64_t mac = ESP.getEfuseMac();
  uint16_t id = (uint16_t)(mac & 0xFFFF);
  if (id == 0) id = 1; // reserve 0 as "unknown/broadcast origin"
  return id;
#endif
}

static void goHome() {
  gps_fix_t f = gps_get();
  ui_show_home(mesh_node_id(), f.fix, s_sentCount, s_meshSeenCount);
  s_state = STATE_HOME;
}

static bool typeFromKey(char k, mesh_msg_type_t *out) {
  switch (k) {
    case '1': *out = MSG_TRAPPED;        return true;
    case '2': *out = MSG_MEDICAL;        return true;
    case '3': *out = MSG_FOOD;           return true;
    case '4': *out = MSG_SHELTER;        return true;
    case '5': *out = MSG_MISSING_PERSON; return true;
    default:  return false;
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  uint16_t nodeId = deriveNodeId();
  mesh_init(nodeId);
  gps_init();
  keypad_init();
  ui_init();

  Serial.printf("[varman] node %u ready\n", nodeId);
  goHome();
}

void loop() {
  gps_update();

  // --- incoming mesh traffic: serviced every loop, regardless of UI state ---
  mesh_msg_t m;
  if (mesh_poll(&m)) {
    s_meshSeenCount++;
    if (m.type == MSG_ACCEPT && mesh_is_my_pending(m.ref_id)) {
      ui_show_accepted(m.origin_node);
      s_state = STATE_ACCEPTED_NOTICE;
    }
    // Any other new mesh traffic (someone else's request, or an ACCEPT not
    // addressed to us) has already been relayed inside mesh_poll(); this
    // device's UI doesn't act on it further — that's the rescuer app's job.
  }

  // --- keypad / UI state machine ---
  char k = keypad_get_key();
  if (!k) return;

  switch (s_state) {
    case STATE_HOME: {
      mesh_msg_type_t t;
      if (typeFromKey(k, &t)) {
        s_pendingType = t;
        gps_fix_t f = gps_get();
        ui_show_confirm((uint8_t)t, f.fix);
        s_state = STATE_CONFIRM;
      } else if (k == '*') {
        gps_fix_t f = gps_get();
        ui_show_diag(mesh_node_id(), f.fix, f.lat, f.lon, s_sentCount, s_meshSeenCount);
        s_state = STATE_DIAG;
      }
      break;
    }

    case STATE_CONFIRM: {
      if (k == '#') {
        gps_fix_t f = gps_get();
        ui_show_sending((uint8_t)s_pendingType);
        uint32_t id = mesh_send_request(s_pendingType, (float)f.lat, (float)f.lon, f.epoch);
        if (id) {
          s_sentCount++;
          ui_show_sent((uint8_t)s_pendingType);
          s_state = STATE_SENT;
        } else {
          goHome(); // transmit failed - don't strand the user on a dead screen
        }
      } else if (k == '*') {
        goHome();
      }
      break;
    }

    case STATE_SENT:
    case STATE_ACCEPTED_NOTICE:
    case STATE_DIAG:
      goHome(); // any key returns home from these screens
      break;
  }
}
