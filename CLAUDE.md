# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Arduino project targeting the **ESP32 WROOM** that implements a BLE HID mouse. The device uses:
- **ICM-42688-P** 6-axis IMU (via GY-601N1 module, I2C) for air-mouse cursor movement
- **3 physical buttons** (GPIO18, GPIO19, GPIO23) for left click, right click, and BLE disconnect

## Build & Flash

This project is built and flashed using the **Arduino IDE** or **arduino-cli** targeting the ESP32 board. There is no Makefile — use the Arduino toolchain. Serial monitor at **115200 baud**.

## Architecture

The codebase is organized as strict horizontal layers. Each layer has its own `.h`/`.cpp` pair and must not reach downward past its direct dependency.

```
ble-mouse.ino          ← main sketch: wires everything together
    │
    ├── mouse_api       ← high-level mouse actions (press/release/click/scroll)
    │       └── ble_report  ← packs and sends 4-byte HID reports over BLE notify
    │               └── ble_hid  ← BLE HID service lifecycle and state machine
    │
    ├── btn             ← generic button driver (debounce, short/long/double-click FSM)
    │
    ├── [imu driver]    ← ICM-42688-P hardware access over I2C (planned, not yet implemented)
    │
    └── [motion mapper] ← converts IMU gyro rates to cursor dx/dy (planned, not yet implemented)
```

### Layer Contracts

- **`ble_hid`** (`ble_hid.h/cpp`): BLE HID service init/deinit, advertising control, connection state (`BLE_STATE_IDLE / ADVERTISING / CONNECTED`). Error type: `ble_err_t`.
- **`ble_report`** (`ble_report.h/cpp`): `ble_hid_send_report(buttons, x, y, wheel)` — the only function that touches the BLE notify characteristic. Buttons bitmask: bit0=left, bit1=right, bit2=middle.
- **`mouse_api`** (`mouse_api.h/cpp`): Calls `mouse_init()` which internally calls `esp_ble_init()` + `esp_ble_start_advertising()`. Exposes `mouse_press/release/click/double_click/long_click/move/scroll`. Error type: `status_mouse`.
- **`btn`** (`btn.h/cpp`): Generic, reusable button driver. Configure a `button_t` struct with designated initializers, call `btn_init()`, then poll `btn_update(btn, now_ms)` every loop. Retrieve events with `btn_get_event()` (clears) or `btn_peek_event()` (keeps). Error type: `status_btn`.

### Button GPIO Assignments

| Button | GPIO | Function         |
|--------|------|------------------|
| B1     | 19   | Right Click      |
| B2     | 18   | Left Click       |
| B3     | 23   | BLE Disconnect   |

Buttons are wired active-HIGH (GPIO → pull-down → GND; button connects to 3.3 V). The `active_low` field in `button_t` must be set accordingly.

### BLE Disconnect Button (B3) Behavior

B3 does **not** use `mouse_handle_button_event()`. The main sketch handles it directly:
- Long press while **CONNECTED** → `esp_ble_disconnect()`
- Long press while **IDLE** → `esp_ble_start_advertising()`

### Button Timing Defaults

- Debounce: 20 ms
- Short click: 50–250 ms
- Long click: ≥ 500 ms (B3 uses 800 ms)
- Double-click gap: configured per button (0 = disabled)

## Planned Modules (not yet implemented)

- **IMU driver** (`icm42688_driver_api.md`): I2C driver for ICM-42688-P. Device address 0x68/0x69. Recommended initial config: 6-axis low-noise, ±500 dps gyro, ±4 g accel, 1 kHz ODR.
- **Motion mapper** (`imu_cursor_mapper_api.md`): Converts gyro rates (yaw → dx, pitch → dy, roll ignored) to cursor deltas using a configurable `motion_dps_profile_t`. Rate-based (not angle-based) to avoid yaw drift.

API specs for these modules are documented in `icm42688_driver_api.md` and `imu_cursor_mapper_api.md`.
