# Varman field node — ESP32 + SX1262 + e-ink + GPS + membrane keypad

Firmware for one mesh node. Flash the identical code to every device —
each board picks up its own node ID automatically (see "Node IDs" below).

**I wrote and reasoned through this code carefully, but could not compile
it here** — the four hardware libraries it depends on need to be
installed from the Arduino/PlatformIO registry, and this environment has
no internet access to fetch them. I *did* independently verify the two
trickiest pieces of pure logic (the de-dup/TTL cache and the GPS-to-epoch
time conversion) by compiling and running them standalone on a desktop
compiler — see `logic_test/`. Flash one board first and sanity-check it
on the bench before building a whole fleet.

## 1. What this is

A flooding LoRa mesh: press a preset on the keypad, the request radios
out, every node that hears it relays it once (hop limit + duplicate
suppression keep it from looping forever), and if a rescuer's device
sends an ACCEPT referencing your request, your screen tells you who
accepted it. This firmware is the **field/requester node** — the
"rescuer coordinator" role described in the wider Varman project lives
on the phone app, not this device.

## 2. Parts and wiring

| Peripheral | Pins used (ESP32) | Notes |
|---|---|---|
| SX1262 LoRa | CS=5, RST=14, BUSY=27, DIO1=26, SCK=18, MISO=19, MOSI=23 | SPI bus shared with the e-ink display |
| E-ink display | CS=15, DC=17, RST=16, BUSY=4, SCK=18, MISO=19, MOSI=23 | Same shared SPI bus |
| GPS (UART2) | RX=33 (<- GPS TX), TX=32 (-> GPS RX) | Most NEO-6M/NEO-M8N boards are 3.3V-logic safe — check yours |
| 4x4 membrane keypad | Rows: 34, 35, 36, 39 — Cols: 13, 21, 22, 25 | **Rows need an external 10k pull-up to 3.3V each** — GPIO34-39 are input-only with no internal pull resistor |

All pin numbers are set in `config.h` — change them there if your wiring
differs. Avoided on purpose: GPIO0/2/12 (boot-strapping pins) and
GPIO6-11 (wired internally to flash) — don't reassign anything onto
those.

## 3. Libraries (install via Arduino Library Manager, or see `platformio.ini`)

- **RadioLib** (Jan Gromeš) — SX1262 driver
- **GxEPD2** (Jean-Marc Zingg) — e-ink driver (pulls in Adafruit-GFX)
- **TinyGPSPlus** (Mikal Hart) — NMEA parsing
- **Keypad** (Mark Stanley / Alexander Brevig) — matrix keypad scanning

**Before your first flash, open `ui.cpp` and change the `GxEPD2_213_B74`
panel class** to whatever matches your exact e-ink module — GxEPD2 has a
distinct class per panel revision (its own
`GxEPD2_display_selection_new_style.h` lists them). The wrong class
compiles fine but draws a scrambled or blank screen.

If your SX1262 module has a TCXO (common on most breakout boards), you
may need to pass its TCXO voltage to `radio.begin()` — there's a
commented-out example for this in `mesh.cpp`.

## 4. The mesh protocol

24-byte fixed packet:

| Field | Size | Meaning |
|---|---|---|
| `msg_id` | 4B | Unique ID of this message: `(origin_node << 16) \| counter` |
| `ref_id` | 4B | For ACCEPT only: the `msg_id` being accepted |
| `origin_node` | 2B | Node that created the original request |
| `type` | 1B | TRAPPED / MEDICAL / FOOD / SHELTER / MISSING_PERSON / ACCEPT |
| `ttl` | 1B | Hops remaining — starts at 6, decremented each relay, dropped at 0 |
| `created_epoch` | 4B | GPS UTC unix time at creation (0 if no fix yet) |
| `lat`, `lon` | 4B each | Location at creation |

**Flow:** every node that hears a new (not-already-seen) message relays
it once, after a random 50–800ms jitter delay (spreads out nodes that
all heard the same packet at once, cutting collisions). A 64-slot cache
of recently-seen `msg_id`s (aged out after 30 minutes) is what makes
"new" mean anything — this is the part I unit-tested separately.

**LoRa airtime, at the default settings (SF10, BW125kHz, 24-byte
payload):** worked out from the standard Semtech airtime formula, that's
~133ms preamble + ~270ms payload ≈ **400ms per transmission**. That's
why the relay jitter window (50–800ms) is sized the way it is — too
narrow and simultaneous relays collide more; too wide and a request
takes longer to cross many hops.

## 5. Node IDs

`config.h` → `FIXED_NODE_ID`: leave at `0` and each board auto-derives a
short ID from its own MAC address (nothing to configure, guaranteed
unique per board). Set it to `1`, `2`, `3`... per device instead if you
want predictable IDs while bench-testing with a few units side by side.

## 6. Bench-testing the mesh

1. Flash two boards. Power both on near each other.
2. On board A, press `1` (TRAPPED) then `#` to send. Board A's screen
   should show "SENT".
3. Watch board B's `Seen` counter on the home screen tick up — board B
   heard and relayed the packet.
4. To see multi-hop relaying specifically (not just one hop), add a
   third board out of direct radio range of board A, with board B
   sitting in between — board C should still see the message, relayed
   through B.
5. To test the ACCEPT path, you'd need a second firmware role (a
   rescuer-side sender) calling `mesh_send_accept(ref_id, ...)` with the
   `msg_id` board A's request used — that role isn't implemented in this
   field-node firmware; wire it up in whatever runs the rescuer side.

## 7. Known limitations / next steps

- **No authentication or encryption.** Any compatible LoRa radio on the
  same frequency and sync word can inject or spoof messages, or flood
  the channel. I deliberately didn't bolt on a quick crypto scheme here
  — getting authenticated encryption subtly wrong is worse than no
  encryption with a clear warning. For a real deployment, look at
  ESP32's built-in **mbedTLS** (AES-GCM) to encrypt+authenticate the
  payload with a pre-shared key before `transmit()`, and decrypt+verify
  in `mesh_poll()` before trusting a packet.
- **Message counter resets on reboot.** `s_counter` in `mesh.cpp` starts
  at 0 each boot, so immediately after a reboot it's theoretically
  possible (though unlikely within the 30-minute cache window) to reuse
  a `msg_id` from just before the reboot. For production, persist the
  counter in flash with the `Preferences` library and bump it on boot.
- **`meshMsgsSeen` is an approximate activity counter**, not a precise
  protocol statistic — it increments on every new message this node
  processes (relays or ACCEPTs-for-me alike).
- **Regulatory check needed before field use.** India's 865–867 MHz
  license-exempt band has its own WPC power and duty-cycle rules —
  confirm the current limits rather than assuming EU868 (1% duty cycle)
  or US915 rules apply. `LORA_TX_POWER_DBM` in `config.h` is a starting
  point, not a verified-legal value for your exact setup.
- **Partial e-ink refresh** isn't implemented — every screen does a full
  (slower, ~1-2s) refresh for reliability. GxEPD2 supports
  `setPartialWindow()` if you want snappier updates later, at the cost
  of occasional ghosting.

## 8. Files

```
varman_node.ino        setup()/loop(), the UI state machine
config.h               all pins and tunables — edit this first
mesh.h / mesh.cpp       packet format, de-dup cache, radio I/O, flood relay
gps_helper.h / .cpp     GPS wrapper + UTC epoch conversion
keypad_handler.h / .cpp keypad wrapper
ui.h / ui.cpp           e-ink screens for each app state
platformio.ini          optional, for PlatformIO/VSCode users
```

`logic_test/test_mesh_logic.cpp` (one level up, not part of the Arduino
sketch) is a standalone, desktop-compilable check of the de-dup/TTL cache
and the civil-date-to-epoch conversion — see that folder's own notes.
