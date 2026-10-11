// ui.cpp — requires the "GxEPD2" library (Jean-Marc Zingg) and
// Adafruit-GFX (pulled in automatically as a GxEPD2 dependency).
//
// *** The panel class below is a PLACEHOLDER. *** GxEPD2 has a different
// class for every panel revision (GxEPD2_213_B72, _B73, _B74, GxEPD2_290_T94,
// etc). Open GxEPD2's own "GxEPD2_display_selection_new_style.h" and pick
// the class that matches your exact module before this will draw
// correctly — wiring it up with the wrong class compiles fine but shows
// a scrambled or blank screen.
#include "ui.h"
#include "config.h"
#include "mesh.h"   // for mesh_msg_type_t labels
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// Example for a common ~250x122 2.13" panel — SWAP THIS for your module.
static GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(
    GxEPD2_213_B74(PIN_EINK_CS, PIN_EINK_DC, PIN_EINK_RST, PIN_EINK_BUSY));

void ui_init() {
  display.init(115200);
  display.setRotation(1); // landscape; Y-coordinates below assume ~250x122 — adjust for your panel
  display.setTextColor(GxEPD_BLACK);
}

static void frame(void (*draw)()) {
  // Full-screen refresh (slow but ghost-free, ~1-2s depending on panel).
  // GxEPD2 supports setPartialWindow() for faster partial updates if the
  // UI ever feels too sluggish — left out here for simplicity/reliability.
  display.firstPage();
  do { draw(); } while (display.nextPage());
}

static const char *typeLabel(uint8_t t) {
  switch (t) {
    case MSG_TRAPPED:        return "TRAPPED";
    case MSG_MEDICAL:        return "MEDICAL";
    case MSG_FOOD:            return "FOOD";
    case MSG_SHELTER:        return "SHELTER";
    case MSG_MISSING_PERSON: return "MISSING PERSON";
    default:                 return "?";
  }
}

// ---------------- HOME ----------------
static uint16_t s_homeNodeId; static bool s_homeFix; static uint16_t s_homeSent, s_homeMesh;
static void drawHome() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(4, 18);
  display.print("VARMAN "); display.print(s_homeNodeId);

  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 42);
  display.print("GPS: "); display.print(s_homeFix ? "FIX" : "NO FIX");
  display.setCursor(4, 58);
  display.print("Sent "); display.print(s_homeSent);
  display.print("  Seen "); display.print(s_homeMesh);

  display.setCursor(4, 84);  display.print("1 TRAPPED  2 MEDICAL");
  display.setCursor(4, 100); display.print("3 FOOD     4 SHELTER");
  display.setCursor(4, 116); display.print("5 MISSING  * DIAG");
}
void ui_show_home(uint16_t nodeId, bool gpsFix, uint16_t sentCount, uint16_t meshMsgsSeen) {
  s_homeNodeId = nodeId; s_homeFix = gpsFix; s_homeSent = sentCount; s_homeMesh = meshMsgsSeen;
  frame(drawHome);
}

// ---------------- CONFIRM ----------------
static uint8_t s_confirmType; static bool s_confirmFix;
static void drawConfirm() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(4, 24);
  display.print("SEND "); display.print(typeLabel(s_confirmType)); display.print("?");

  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 54);
  display.print(s_confirmFix ? "GPS fix OK" : "WARNING: no GPS fix");
  display.setCursor(4, 90);
  display.print("# = SEND    * = CANCEL");
}
void ui_show_confirm(uint8_t msgType, bool gpsFix) {
  s_confirmType = msgType; s_confirmFix = gpsFix;
  frame(drawConfirm);
}

// ---------------- SENDING / SENT ----------------
static uint8_t s_txType;
static void drawSending() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(4, 30); display.print(typeLabel(s_txType));
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 60); display.print("Sending...");
}
void ui_show_sending(uint8_t msgType) { s_txType = msgType; frame(drawSending); }

static void drawSent() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(4, 30); display.print(typeLabel(s_txType));
  display.setCursor(4, 56); display.print("SENT");
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 90); display.print("Broadcasting on mesh");
  display.setCursor(4, 108); display.print("Any key = home");
}
void ui_show_sent(uint8_t msgType) { s_txType = msgType; frame(drawSent); }

// ---------------- ACCEPTED ----------------
static uint16_t s_acceptBy;
static void drawAccepted() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(4, 30); display.print("ACCEPTED");
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 60); display.print("by node "); display.print(s_acceptBy);
  display.setCursor(4, 100); display.print("Any key = home");
}
void ui_show_accepted(uint16_t byNode) { s_acceptBy = byNode; frame(drawAccepted); }

// ---------------- DIAG ----------------
static uint16_t s_diagNode; static bool s_diagFix; static double s_diagLat, s_diagLon;
static uint16_t s_diagSent, s_diagMesh;
static void drawDiag() {
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(4, 16); display.print("DIAGNOSTICS");
  display.setCursor(4, 36); display.print("Node: "); display.print(s_diagNode);
  display.setCursor(4, 52);
  if (s_diagFix) { display.print(s_diagLat, 4); display.print(","); display.print(s_diagLon, 4); }
  else { display.print("No GPS fix"); }
  display.setCursor(4, 68); display.print("Sent: "); display.print(s_diagSent);
  display.setCursor(4, 84); display.print("Mesh seen: "); display.print(s_diagMesh);
  display.setCursor(4, 110); display.print("Any key = home");
}
void ui_show_diag(uint16_t nodeId, bool gpsFix, double lat, double lon,
                   uint16_t sentCount, uint16_t meshMsgsSeen) {
  s_diagNode = nodeId; s_diagFix = gpsFix; s_diagLat = lat; s_diagLon = lon;
  s_diagSent = sentCount; s_diagMesh = meshMsgsSeen;
  frame(drawDiag);
}
