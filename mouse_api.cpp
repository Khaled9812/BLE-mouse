#include "mouse_api.h"

#include "ble_hid.h"
#include "ble_report.h"

#include <Arduino.h>  // delay()

// Button bitmask positions (match the HID report descriptor in ble_hid.cpp)
#define MASK_LEFT   (1u << 0)
#define MASK_RIGHT  (1u << 1)
#define MASK_MIDDLE (1u << 2)

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool    _initialized  = false;

static uint8_t _btn_state    = 0;  // current button bitmask held in reports

static uint8_t _mask(mouse_button_t btn) {
    switch (btn) {
        case MOUSE_BTN_LEFT:   return MASK_LEFT;
        case MOUSE_BTN_RIGHT:  return MASK_RIGHT;
        case MOUSE_BTN_MIDDLE: return MASK_MIDDLE;
        default:               return 0;
    }
}

// Send current button state with the given movement/scroll deltas
static status_mouse _send(int8_t x, int8_t y, int8_t wheel) {
    ble_err_t err = ble_hid_send_report(_btn_state, x, y, wheel);
    if (err == BLE_ERR_NOT_CONNECTED) return MOUSE_ERR_NOT_CONNECTED;
    if (err != BLE_OK)                return MOUSE_ERR_INTERNAL;
    return MOUSE_OK;
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

status_mouse mouse_init(void) {
    if (_initialized) return MOUSE_OK;  // idempotent

    ble_err_t err = esp_ble_init();
    if (err != BLE_OK && err != BLE_ERR_ALREADY_INIT) return MOUSE_ERR_INTERNAL;

    err = esp_ble_start_advertising();
    if (err != BLE_OK && err != BLE_ERR_ALREADY_ADVERTISING) return MOUSE_ERR_INTERNAL;

    _btn_state   = 0;
    _initialized = true;
    return MOUSE_OK;
}

status_mouse mouse_deinit(void) {
    if (!_initialized) return MOUSE_ERR_NOT_INIT;

    esp_ble_deinit();
    _initialized = false;
    _btn_state   = 0;
    return MOUSE_OK;
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

status_mouse mouse_move(int8_t dx, int8_t dy) {
    if (!_initialized)           return MOUSE_ERR_NOT_INIT;
    if (!esp_ble_can_send()) return MOUSE_ERR_NOT_CONNECTED;
    return _send(dx, dy, 0);
}

status_mouse mouse_scroll(int8_t wheel) {
    if (!_initialized)           return MOUSE_ERR_NOT_INIT;
    if (!esp_ble_can_send()) return MOUSE_ERR_NOT_CONNECTED;
    return _send(0, 0, wheel);
}

// ---------------------------------------------------------------------------
// Button actions
// ---------------------------------------------------------------------------

status_mouse mouse_press(mouse_button_t btn) {
    if (!_initialized)           return MOUSE_ERR_NOT_INIT;
    if (!esp_ble_can_send()) return MOUSE_ERR_NOT_CONNECTED;
    _btn_state |= _mask(btn);
    return _send(0, 0, 0);
}

status_mouse mouse_release(mouse_button_t btn) {
    if (!_initialized)           return MOUSE_ERR_NOT_INIT;
    if (!esp_ble_can_send()) return MOUSE_ERR_NOT_CONNECTED;
    _btn_state &= ~_mask(btn);
    return _send(0, 0, 0);
}

status_mouse mouse_click(mouse_button_t btn) {
    status_mouse err = mouse_press(btn);
    if (err != MOUSE_OK) return err;
    delay(20);
    return mouse_release(btn);
}

status_mouse mouse_double_click(mouse_button_t btn) {
    status_mouse err = mouse_click(btn);
    if (err != MOUSE_OK) return err;
    delay(80);
    return mouse_click(btn);
}

status_mouse mouse_long_click(mouse_button_t btn) {
    status_mouse err = mouse_press(btn);
    if (err != MOUSE_OK) return err;
    delay(600);
    return mouse_release(btn);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

bool mouse_is_ready(void) {
    return _initialized;
}

bool mouse_is_connected(void) {
    return esp_ble_can_send();
}

// ---------------------------------------------------------------------------
// Button-to-mouse mapping
// ---------------------------------------------------------------------------

status_mouse mouse_handle_button_event(button_t *btn, button_event_t event) {
    if (!btn)          return MOUSE_ERR_INVALID_ARG;
    if (!_initialized) return MOUSE_ERR_NOT_INIT;

    mouse_button_t mouse_btn;
    switch (btn->function) {
        case BTN_FUNC_LEFT_CLICK:   mouse_btn = MOUSE_BTN_LEFT;   break;
        case BTN_FUNC_RIGHT_CLICK:  mouse_btn = MOUSE_BTN_RIGHT;  break;
        case BTN_FUNC_MIDDLE_CLICK: mouse_btn = MOUSE_BTN_MIDDLE; break;
        default: return MOUSE_OK;  // BTN_FUNC_NONE / CUSTOM — nothing to map
    }

    switch (event) {
        case BTN_EVENT_SHORT_CLICK:  return mouse_click(mouse_btn);
        case BTN_EVENT_DOUBLE_CLICK: return mouse_double_click(mouse_btn);
        case BTN_EVENT_LONG_CLICK:   return mouse_long_click(mouse_btn);
        default:                     return MOUSE_OK;  // PRESS/RELEASE/NONE ignored
    }
}
