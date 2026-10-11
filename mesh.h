// mesh.h — the flooding LoRa mesh protocol: packet format, de-dup cache,
// and the radio-facing API the rest of the firmware calls.
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#define MESH_MAX_TTL              6      // max hops a message may travel
#define DEDUPE_CACHE_SIZE         64     // how many recent message IDs we remember
#define DEDUPE_ENTRY_MAX_AGE_MS   (30UL * 60UL * 1000UL)  // 30 min
#define RELAY_JITTER_MIN_MS       50     // random delay before relaying, to reduce
#define RELAY_JITTER_MAX_MS       800    // collisions when several nodes relay at once
#define MY_PENDING_MAX            8      // how many of our own requests we track for ACCEPT matching

typedef enum : uint8_t {
  MSG_TRAPPED        = 0,
  MSG_MEDICAL        = 1,
  MSG_FOOD           = 2,
  MSG_SHELTER        = 3,
  MSG_MISSING_PERSON = 4,
  MSG_ACCEPT         = 5,  // acceptance, relayed back through the mesh
} mesh_msg_type_t;

// Fixed 24-byte wire format. Keep this small: shorter payload = shorter
// LoRa airtime = less airtime used, fewer collisions, lower power draw.
#pragma pack(push, 1)
typedef struct {
  uint32_t msg_id;         // unique ID of *this* message — origin_node:counter, see mesh.cpp
  uint32_t ref_id;         // MSG_ACCEPT only: msg_id being accepted. 0 for new requests.
  uint16_t origin_node;    // node that created the original request
  uint8_t  type;           // mesh_msg_type_t
  uint8_t  ttl;            // hops remaining; each relay decrements it, dropped at 0
  uint32_t created_epoch;  // GPS UTC unix time at creation, 0 if no fix was available
  float    lat;
  float    lon;
} mesh_msg_t; // 24 bytes
#pragma pack(pop)

// ---- pure, hardware-independent de-dup logic (unit-tested separately —
// see logic_test/test_mesh_logic.cpp) ----
typedef struct {
  uint32_t ids[DEDUPE_CACHE_SIZE];
  uint32_t seen_at_ms[DEDUPE_CACHE_SIZE];
  uint8_t  next_slot;
  uint8_t  count;
} dedupe_cache_t;

void dedupe_cache_init(dedupe_cache_t *c);
// Returns true and records msg_id if it's new (or its earlier entry has
// expired). Returns false — drop it — if it's a fresh duplicate.
bool dedupe_check_and_add(dedupe_cache_t *c, uint32_t msg_id, uint32_t now_ms);

// ---- radio-facing API ----
void     mesh_init(uint16_t nodeId);
uint16_t mesh_node_id();

// Build and broadcast a brand-new request originated by this node.
// Returns the assigned msg_id, or 0 if the radio failed to transmit.
uint32_t mesh_send_request(mesh_msg_type_t type, float lat, float lon, uint32_t epoch);

// Broadcast an ACCEPT for someone else's request (for a rescuer-role node).
bool mesh_send_accept(uint32_t ref_id, float lat, float lon, uint32_t epoch);

// Poll the radio — call every loop(). Internally this always handles
// de-dup and relaying regardless of return value. Returns true and fills
// *out when something is worth surfacing to the application layer: any
// new request seen on the mesh, or an ACCEPT that matches one of our own
// pending sends.
bool mesh_poll(mesh_msg_t *out);

// Was `ref_id` a request *we* originated and are still waiting on?
bool mesh_is_my_pending(uint32_t ref_id);
