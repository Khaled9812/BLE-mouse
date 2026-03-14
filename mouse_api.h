#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "btn.h"

// ---------------------------------------------------------------------------
// Status codes
// ---------------------------------------------------------------------------
typedef enum {
    MOUSE_OK = 0,
    MOUSE_ERR_INVALID_ARG,
    MOUSE_ERR_NOT_INIT,
    MOUSE_ERR_NOT_CONNECTED,
    MOUSE_ERR_INTERNAL
} status_mouse;

// ---------------------------------------------------------------------------
// Mouse button identifiers
// ---------------------------------------------------------------------------
typedef enum {
    MOUSE_BTN_LEFT = 0,
    MOUSE_BTN_RIGHT,
    MOUSE_BTN_MIDDLE
} mouse_button_t;

// ---------------------------------------------------------------------------
// Initialisation
// Calls esp_ble_init() and esp_ble_start_advertising() internally.
// ---------------------------------------------------------------------------
status_mouse mouse_init(void);
status_mouse mouse_deinit(void);

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------
status_mouse mouse_move(int8_t dx, int8_t dy);
status_mouse mouse_scroll(int8_t wheel);

// ---------------------------------------------------------------------------
// Button actions
//   mouse_press()        — hold button down (sends report with bit set)
//   mouse_release()      — release button   (sends report with bit cleared)
//   mouse_click()        — press + release
//   mouse_double_click() — two clicks with a short gap
//   mouse_long_click()   — press, hold 600 ms, release
// ---------------------------------------------------------------------------
status_mouse mouse_press(mouse_button_t btn);
status_mouse mouse_release(mouse_button_t btn);
status_mouse mouse_click(mouse_button_t btn);
status_mouse mouse_double_click(mouse_button_t btn);
status_mouse mouse_long_click(mouse_button_t btn);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
bool mouse_is_ready(void);
bool mouse_is_connected(void);

// ---------------------------------------------------------------------------
// Button-to-mouse mapping
//
// Maps the button's logical function and the detected event to the correct
// mouse action.  Call this after btn_get_event() returns a non-NONE value.
//
// Mapping table:
//   BTN_FUNC_RIGHT_CLICK / BTN_FUNC_LEFT_CLICK / BTN_FUNC_MIDDLE_CLICK
//     BTN_EVENT_SHORT_CLICK  → mouse_click()
//     BTN_EVENT_DOUBLE_CLICK → mouse_double_click()
//     BTN_EVENT_LONG_CLICK   → mouse_long_click()
//   PRESS / RELEASE / NONE are silently ignored.
// ---------------------------------------------------------------------------
status_mouse mouse_handle_button_event(button_t *btn, button_event_t event);
