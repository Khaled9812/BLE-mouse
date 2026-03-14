#pragma once

#include "ble_hid.h"

// ---------------------------------------------------------------------------
// HID report sender
//
// Sends a 4-byte mouse input report over BLE notify.
//
//  buttons : bitmask  [bit2=middle | bit1=right | bit0=left]
//  x       : horizontal delta  (int8, relative, -127..127)
//  y       : vertical delta    (int8, relative, -127..127)
//  wheel   : scroll delta      (int8, relative, -127..127)
//
// Returns BLE_ERR_NOT_INIT if the HID service is not ready,
//         BLE_ERR_NOT_CONNECTED if no host is connected,
//         BLE_OK on success.
// ---------------------------------------------------------------------------
ble_err_t ble_hid_send_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);
