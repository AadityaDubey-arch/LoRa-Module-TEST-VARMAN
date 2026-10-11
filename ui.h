// ui.h — e-ink screen renderer. Each function does one full-screen
// redraw for a given app state (see varman_node.ino for the state
// machine that calls these).
#pragma once
#include <stdint.h>
#include <stdbool.h>

void ui_init();

void ui_show_home(uint16_t nodeId, bool gpsFix, uint16_t sentCount, uint16_t meshMsgsSeen);
void ui_show_confirm(uint8_t msgType, bool gpsFix);
void ui_show_sending(uint8_t msgType);
void ui_show_sent(uint8_t msgType);
void ui_show_accepted(uint16_t byNode);
void ui_show_diag(uint16_t nodeId, bool gpsFix, double lat, double lon,
                   uint16_t sentCount, uint16_t meshMsgsSeen);
