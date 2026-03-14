#include "btn.h"

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

status_btn btn_init(button_t *btn) {
    if (!btn)             return BTN_ERR_INVALID_ARG;
    if (btn->initialized) return BTN_ERR_ALREADY_INIT;

    pinMode(btn->gpio, INPUT);

    btn->pressed              = false;
    btn->long_reported        = false;
    btn->last_event           = BTN_EVENT_NONE;
    btn->last_change_time_ms  = 0;
    btn->press_time_ms        = 0;
    btn->release_time_ms      = 0;
    btn->_raw_active          = false;
    btn->_wait_double         = false;
    btn->_first_release_ms    = 0;
    btn->initialized          = true;

    return BTN_OK;
}

status_btn btn_deinit(button_t *btn) {
    if (!btn)              return BTN_ERR_INVALID_ARG;
    if (!btn->initialized) return BTN_ERR_NOT_INIT;

    btn->initialized  = false;
    btn->pressed      = false;
    btn->_wait_double = false;
    btn->last_event   = BTN_EVENT_NONE;

    return BTN_OK;
}

// ---------------------------------------------------------------------------
// Update
//
// State machine:
//
//   Raw GPIO → debounce → confirmed press/release
//
//   On confirmed PRESS  : emit BTN_EVENT_PRESS
//   While held          : emit BTN_EVENT_LONG_CLICK once after long_click_ms
//   On confirmed RELEASE:
//     - emit BTN_EVENT_RELEASE
//     - if press was short AND a previous short click is pending within
//       double_click_gap_ms  → emit BTN_EVENT_DOUBLE_CLICK
//     - if press was short AND no pending double-click               → arm
//       double-click timer (SHORT_CLICK emitted after gap expires)
//     - presses shorter than short_click_min_ms are discarded as noise
//   After gap expires   : emit BTN_EVENT_SHORT_CLICK
// ---------------------------------------------------------------------------

status_btn btn_update(button_t *btn, uint32_t now_ms) {
    if (!btn)              return BTN_ERR_INVALID_ARG;
    if (!btn->initialized) return BTN_ERR_NOT_INIT;

    // --- 1. Read raw GPIO and convert to logical "active" ---
    int  raw        = digitalRead(btn->gpio);
    bool raw_active = btn->active_low ? (raw == LOW) : (raw == HIGH);

    // --- 2. Debounce ---
    if (raw_active != btn->_raw_active) {
        btn->_raw_active         = raw_active;
        btn->last_change_time_ms = now_ms;
    }

    if ((now_ms - btn->last_change_time_ms) < btn->debounce_ms) {
        return BTN_OK;  // still bouncing
    }

    // --- 3. Confirmed state transition ---
    bool confirmed = btn->_raw_active;

    if (confirmed != btn->pressed) {
        if (confirmed) {
            // ---- PRESS ----
            btn->pressed       = true;
            btn->press_time_ms = now_ms;
            btn->long_reported = false;
            btn->last_event    = BTN_EVENT_PRESS;

        } else {
            // ---- RELEASE ----
            btn->pressed         = false;
            btn->release_time_ms = now_ms;
            btn->last_event      = BTN_EVENT_RELEASE;

            uint32_t duration = now_ms - btn->press_time_ms;

            if (btn->long_reported) {
                // Long click was already emitted — nothing more to do
                btn->long_reported = false;

            } else if (duration < btn->short_click_min_ms) {
                // Too short — discard as noise

            } else if (duration <= btn->short_click_max_ms) {
                // Valid short click
                if (btn->_wait_double &&
                    (now_ms - btn->_first_release_ms) < btn->double_click_gap_ms) {
                    // Second click within the gap → DOUBLE CLICK
                    btn->last_event   = BTN_EVENT_DOUBLE_CLICK;
                    btn->_wait_double = false;
                } else {
                    // First short click — arm double-click timer
                    btn->_wait_double      = true;
                    btn->_first_release_ms = now_ms;
                    btn->last_event        = BTN_EVENT_NONE;  // hold until gap expires
                }
                // duration > short_click_max_ms but long not yet reported:
                // treat as long-click that fired just after the max threshold
            } else {
                btn->last_event = BTN_EVENT_LONG_CLICK;
            }
        }
    }

    // --- 4. Long click detection (fires while still held) ---
    if (btn->pressed && !btn->long_reported) {
        if ((now_ms - btn->press_time_ms) >= btn->long_click_ms) {
            btn->last_event    = BTN_EVENT_LONG_CLICK;
            btn->long_reported = true;
            btn->_wait_double  = false;  // cancel any pending double-click
        }
    }

    // --- 5. Double-click gap timeout → emit pending SHORT_CLICK ---
    if (btn->_wait_double && !btn->pressed) {
        if ((now_ms - btn->_first_release_ms) >= btn->double_click_gap_ms) {
            btn->last_event   = BTN_EVENT_SHORT_CLICK;
            btn->_wait_double = false;
        }
    }

    return BTN_OK;
}

// ---------------------------------------------------------------------------
// Event retrieval
// ---------------------------------------------------------------------------

button_event_t btn_get_event(button_t *btn) {
    if (!btn || !btn->initialized) return BTN_EVENT_NONE;
    button_event_t evt = btn->last_event;
    btn->last_event    = BTN_EVENT_NONE;
    return evt;
}

button_event_t btn_peek_event(const button_t *btn) {
    if (!btn || !btn->initialized) return BTN_EVENT_NONE;
    return btn->last_event;
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool btn_is_pressed(const button_t *btn) {
    return btn && btn->initialized && btn->pressed;
}

button_function_t btn_get_function(const button_t *btn) {
    if (!btn) return BTN_FUNC_NONE;
    return btn->function;
}

button_id_t btn_get_id(const button_t *btn) {
    if (!btn) return BTN_ID_MAX;
    return btn->id;
}

int btn_get_gpio(const button_t *btn) {
    if (!btn) return -1;
    return btn->gpio;
}

// ---------------------------------------------------------------------------
// Convenience helpers (each clears the event after returning true)
// ---------------------------------------------------------------------------

bool btn_is_short_click(button_t *btn) {
    if (!btn || !btn->initialized) return false;
    if (btn->last_event == BTN_EVENT_SHORT_CLICK) {
        btn->last_event = BTN_EVENT_NONE;
        return true;
    }
    return false;
}

bool btn_is_long_click(button_t *btn) {
    if (!btn || !btn->initialized) return false;
    if (btn->last_event == BTN_EVENT_LONG_CLICK) {
        btn->last_event = BTN_EVENT_NONE;
        return true;
    }
    return false;
}

bool btn_is_double_click(button_t *btn) {
    if (!btn || !btn->initialized) return false;
    if (btn->last_event == BTN_EVENT_DOUBLE_CLICK) {
        btn->last_event = BTN_EVENT_NONE;
        return true;
    }
    return false;
}
