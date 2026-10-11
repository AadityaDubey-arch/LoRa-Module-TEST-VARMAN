// mesh.cpp — see mesh.h for the protocol description.
//
// Requires the "RadioLib" library (Jan Gromeš) — install via
// Arduino Library Manager or PlatformIO. Targets RadioLib's SX126x API
// (setDio1Action / startReceive / readData). If you're on a noticeably
// older or newer RadioLib release and it won't compile, check RadioLib's
// SX1262 examples for the current method names — the radio-facing API
// has shifted slightly across major versions.
#include "mesh.h"
#include "config.h"
#include <RadioLib.h>
#include <string.h>

static SX1262 radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

static dedupe_cache_t s_cache;
static uint16_t s_nodeId = 0;
static uint16_t s_counter = 0;   // per-boot counter — see "Known limitations" in README
static uint32_t s_myPending[MY_PENDING_MAX];
static uint8_t  s_myPendingNext = 0;

static volatile bool s_rxFlag = false;
static void IRAM_ATTR onRadioIRQ() { s_rxFlag = true; }

// ---------------------------------------------------------------------
// De-dup cache — pure logic, no hardware calls, so it can be unit tested
// on a desktop compiler (see logic_test/test_mesh_logic.cpp).
// ---------------------------------------------------------------------
void dedupe_cache_init(dedupe_cache_t *c) {
  memset(c, 0, sizeof(*c));
}

bool dedupe_check_and_add(dedupe_cache_t *c, uint32_t msg_id, uint32_t now_ms) {
  for (uint8_t i = 0; i < c->count; i++) {
    if (c->ids[i] == msg_id) {
      if (now_ms - c->seen_at_ms[i] < DEDUPE_ENTRY_MAX_AGE_MS) {
        return false; // genuine, still-fresh duplicate — drop it
      }
      // Same id resurfaced after its cache entry aged out (counter wrap,
      // or just a very long mission). Treat as new, refresh in place.
      c->seen_at_ms[i] = now_ms;
      return true;
    }
  }
  // Not seen before — insert. Ring buffer: once full, overwrite the
  // oldest slot (which is fine — DEDUPE_ENTRY_MAX_AGE_MS has usually
  // already made it stale by the time the buffer wraps this far).
  c->ids[c->next_slot] = msg_id;
  c->seen_at_ms[c->next_slot] = now_ms;
  c->next_slot = (c->next_slot + 1) % DEDUPE_CACHE_SIZE;
  if (c->count < DEDUPE_CACHE_SIZE) c->count++;
  return true;
}

// ---------------------------------------------------------------------
// "My pending requests" — small ring buffer so we can recognise an
// ACCEPT addressed back to one of our own sends.
// ---------------------------------------------------------------------
static void addMyPending(uint32_t id) {
  s_myPending[s_myPendingNext] = id;
  s_myPendingNext = (s_myPendingNext + 1) % MY_PENDING_MAX;
}

bool mesh_is_my_pending(uint32_t ref_id) {
  for (uint8_t i = 0; i < MY_PENDING_MAX; i++) {
    if (s_myPending[i] == ref_id) return true;
  }
  return false;
}

// ---------------------------------------------------------------------
// Radio setup
// ---------------------------------------------------------------------
void mesh_init(uint16_t nodeId) {
  s_nodeId = nodeId;
  dedupe_cache_init(&s_cache);
  memset(s_myPending, 0, sizeof(s_myPending));

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_LORA_CS);

  int state = radio.begin(LORA_FREQ_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR,
                           LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
                           LORA_PREAMBLE_LEN);
  // If your SX1262 module has a TCXO (most breakout boards do — e.g.
  // common Ebyte/Heltec/RAK modules), you likely need the extended form
  // instead, with your module's TCXO voltage from its datasheet:
  //   radio.begin(LORA_FREQ_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR,
  //               LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
  //               LORA_PREAMBLE_LEN, 1.6, false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[mesh] radio.begin() failed, code %d\n", state);
  }

  radio.setDio1Action(onRadioIRQ);
  radio.startReceive();
}

uint16_t mesh_node_id() { return s_nodeId; }

static uint32_t nextMsgId() {
  s_counter++; // wraps at 65535 — see README "Known limitations"
  return ((uint32_t)s_nodeId << 16) | s_counter;
}

static bool transmitRaw(const mesh_msg_t *m) {
  radio.standby();
  int state = radio.transmit((uint8_t *)m, sizeof(mesh_msg_t));
  radio.startReceive(); // always return to listening afterwards
  return state == RADIOLIB_ERR_NONE;
}

uint32_t mesh_send_request(mesh_msg_type_t type, float lat, float lon, uint32_t epoch) {
  mesh_msg_t m;
  m.msg_id        = nextMsgId();
  m.ref_id        = 0;
  m.origin_node   = s_nodeId;
  m.type          = (uint8_t)type;
  m.ttl           = MESH_MAX_TTL;
  m.created_epoch = epoch;
  m.lat           = lat;
  m.lon           = lon;

  // Remember our own id so we never relay our own echo, and so we can
  // recognise an ACCEPT addressed to us later.
  dedupe_check_and_add(&s_cache, m.msg_id, millis());
  addMyPending(m.msg_id);

  return transmitRaw(&m) ? m.msg_id : 0;
}

bool mesh_send_accept(uint32_t ref_id, float lat, float lon, uint32_t epoch) {
  mesh_msg_t m;
  m.msg_id        = nextMsgId();
  m.ref_id        = ref_id;
  m.origin_node   = s_nodeId;
  m.type          = (uint8_t)MSG_ACCEPT;
  m.ttl           = MESH_MAX_TTL;
  m.created_epoch = epoch;
  m.lat           = lat;
  m.lon           = lon;

  dedupe_check_and_add(&s_cache, m.msg_id, millis());
  return transmitRaw(&m);
}

bool mesh_poll(mesh_msg_t *out) {
  if (!s_rxFlag) return false;
  s_rxFlag = false;

  mesh_msg_t m;
  int state = radio.readData((uint8_t *)&m, sizeof(m));
  radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) return false;
  if (radio.getPacketLength() != sizeof(mesh_msg_t)) return false; // foreign/garbled packet, ignore

  uint32_t now = millis();
  bool isNew = dedupe_check_and_add(&s_cache, m.msg_id, now);
  if (!isNew) return false; // already seen and handled this message

  // Flood relay: every node forwards every new message once, regardless
  // of whether it's relevant to that node's own role — that's what makes
  // it a flood-fill mesh. Random jitter spreads out simultaneous relays
  // from nodes that all just heard the same packet.
  if (m.ttl > 0) {
    mesh_msg_t relay = m;
    relay.ttl -= 1;
    delay(random(RELAY_JITTER_MIN_MS, RELAY_JITTER_MAX_MS));
    transmitRaw(&relay);
  }

  // Only surface ACCEPTs that are actually addressed to one of our own
  // sends — an ACCEPT for someone else's request has already been
  // relayed above, but this node's UI has no reason to act on it.
  if (m.type == MSG_ACCEPT && !mesh_is_my_pending(m.ref_id)) {
    return false;
  }

  *out = m;
  return true;
}
