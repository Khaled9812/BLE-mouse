#pragma once

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Status codes
// ---------------------------------------------------------------------------
typedef enum {
    BTN_OK = 0,
    BTN_ERR_INVALID_ARG,
    BTN_ERR_NOT_INIT,
    BTN_ERR_ALREADY_INIT,
    BTN_ERR_INTERNAL
} status_btn;

// ---------------------------------------------------------------------------
// Button identity
// ---------------------------------------------------------------------------
typedef enum {
    BTN_ID_B1 = 0,
    BTN_ID_B2,
    BTN_ID_B3,
    BTN_ID_MAX
} button_id_t;

// ---------------------------------------------------------------------------
// Button logical role
// ---------------------------------------------------------------------------
typedef enum {
    BTN_FUNC_NONE = 0,
    BTN_FUNC_LEFT_CLICK,
    BTN_FUNC_RIGHT_CLICK,
    BTN_FUNC_MIDDLE_CLICK,
    BTN_FUNC_DISCONNECT,
    BTN_FUNC_CUSTOM
} button_function_t;

// ---------------------------------------------------------------------------
// Button events
// ---------------------------------------------------------------------------
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_SHORT_CLICK,
    BTN_EVENT_LONG_CLICK,
    BTN_EVENT_DOUBLE_CLICK
} button_event_t;

// ---------------------------------------------------------------------------
// Button descriptor
//
// Zero-initialise the internal fields (_raw_active, _wait_double,
// _first_release_ms) before calling btn_init(). Using designated
// initialisers (C99) as shown below is sufficient:
//
//   button_t b = {
//       .id = BTN_ID_B1, .function = BTN_FUNC_RIGHT_CLICK,
//       .gpio = 19, .active_low = true,
//       .debounce_ms = 20, .short_click_min_ms = 50,
//       .short_click_max_ms = 250, .long_click_ms = 500,
//       .double_click_gap_ms = 250
//   };
// ---------------------------------------------------------------------------
typedef struct {
    // --- Configuration (set before btn_init) ---
    button_id_t       id;
    button_function_t function;
    int               gpio;
    bool              active_low;

    uint32_t debounce_ms;
    uint32_t short_click_min_ms;
    uint32_t short_click_max_ms;
    uint32_t long_click_ms;
    uint32_t double_click_gap_ms;

    // --- Runtime state (managed by driver) ---
    bool           initialized;
    bool           pressed;        // confirmed (debounced) press state
    bool           long_reported;  // long-click already emitted for this press

    uint32_t last_change_time_ms;  // when raw GPIO last changed (debounce timer)
    uint32_t press_time_ms;        // timestamp of confirmed press
    uint32_t release_time_ms;      // timestamp of confirmed release

    button_event_t last_event;

    // --- Internal FSM (do not access directly) ---
    bool     _raw_active;          // last raw GPIO reading
    bool     _wait_double;         // waiting for a second click
    uint32_t _first_release_ms;    // timestamp of the first short-click release
} button_t;

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
status_btn btn_init(button_t *btn);
status_btn btn_deinit(button_t *btn);

// ---------------------------------------------------------------------------
// Update — call as fast as possible (e.g. every loop iteration)
// ---------------------------------------------------------------------------
status_btn btn_update(button_t *btn, uint32_t now_ms);

// ---------------------------------------------------------------------------
// Event retrieval
// ---------------------------------------------------------------------------
button_event_t btn_get_event(button_t *btn);           // returns and clears
button_event_t btn_peek_event(const button_t *btn);    // returns without clearing

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------
bool              btn_is_pressed(const button_t *btn);
button_function_t btn_get_function(const button_t *btn);
button_id_t       btn_get_id(const button_t *btn);
int               btn_get_gpio(const button_t *btn);

// ---------------------------------------------------------------------------
// Convenience — each returns true once then clears the event
// ---------------------------------------------------------------------------
bool btn_is_short_click(button_t *btn);
bool btn_is_long_click(button_t *btn);
bool btn_is_double_click(button_t *btn);
