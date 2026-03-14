#pragma once

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Error codes
// ---------------------------------------------------------------------------
typedef enum {
    BLE_OK = 0,
    BLE_ERR_ALREADY_INIT,
    BLE_ERR_NOT_INIT,
    BLE_ERR_ALREADY_ADVERTISING,
    BLE_ERR_NOT_ADVERTISING,
    BLE_ERR_NOT_CONNECTED,
    BLE_ERR_FAIL,
} ble_err_t;

// ---------------------------------------------------------------------------
// Connection state
// ---------------------------------------------------------------------------
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
} ble_state_t;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
ble_err_t esp_ble_init(void);
ble_err_t esp_ble_deinit(void);

// ---------------------------------------------------------------------------
// Advertising
// ---------------------------------------------------------------------------
ble_err_t esp_ble_start_advertising(void);
ble_err_t esp_ble_stop_advertising(void);

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------
ble_err_t esp_ble_disconnect(void);
ble_err_t esp_ble_restart_advertising(void);  // use this to re-advertise after disconnect

// ---------------------------------------------------------------------------
// Status queries
// ---------------------------------------------------------------------------
ble_state_t esp_ble_get_state(void);
bool        esp_ble_is_connected(void);
bool        esp_ble_is_paired(void);
bool        esp_ble_can_send(void);