// keypad_handler.h — thin wrapper around the "Keypad" library
// (Mark Stanley / Alexander Brevig).
#pragma once

void keypad_init();

// Returns the newly-pressed key ('0'-'9', '*', '#', 'A'-'D'), or 0 (NO_KEY)
// if nothing new was pressed since the last call. Debounced internally.
char keypad_get_key();
