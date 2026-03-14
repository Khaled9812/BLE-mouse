# BLE Air Mouse — ESP32 WROOM

A Bluetooth Low Energy HID mouse implemented on the **ESP32 WROOM**.
The device acts as a wireless air mouse: rotate it in the air to move the cursor, and use the three physical buttons for left click, right click, and BLE disconnect/reconnect.

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32 WROOM |
| IMU | ICM-42688-P (GY-601N1 module) via I2C |
| I2C pins | SDA = GPIO21, SCL = GPIO22 |
| IMU address | 0x68 (AP_AD0 tied LOW) |
| Button B1 (Right Click) | GPIO19 |
| Button B2 (Left Click) | GPIO18 |
| Button B3 (BLE Disconnect) | GPIO23 |

### Button wiring
All buttons are wired **active-HIGH**: GPIO → pull-down resistor → GND, button connects GPIO to 3.3 V.

### IMU wiring
CS pin must be tied to VDDIO (3.3 V) to enable I2C mode. SDA and SCL require pull-up resistors.

---

## Button Behavior

| Button | Short press | Long press (≥ 500 ms) |
|---|---|---|
| B1 | Right click | — |
| B2 | Left click | — |
| B3 | — | Connected → disconnect / Idle → start advertising |

B3 long-press threshold is 800 ms to avoid accidental disconnection.

---

## Air Mouse Controls

- **Yaw left/right** → cursor moves left/right
- **Pitch up/down** → cursor moves up/down
- **Roll** → ignored

Motion is **rate-based** (gyro angular velocity → cursor velocity), not angle-based, to avoid yaw drift.

---

## Software Architecture

```
ble-mouse.ino          ← main sketch: wires everything together
    │
    ├── mouse_api       ← high-level mouse actions (press/release/move/scroll)
    │       └── ble_report  ← packs and sends 4-byte HID reports over BLE notify
    │               └── ble_hid  ← BLE HID service lifecycle and state machine
    │
    ├── btn             ← generic button driver (debounce, short/long-click FSM)
    │
    ├── icm42688        ← ICM-42688-P I2C driver (raw/scaled read, gyro calibration)
    │
    └── motion_mapper   ← converts gyro rates to cursor dx/dy (dead-zone, IIR, gain)
```

Each layer only calls the layer directly below it.

---

## Build & Flash

Open the project folder in **Arduino IDE** and select:

- Board: `ESP32 Dev Module` (or ESP32 WROOM-32)
- Upload Speed: `921600`
- Flash Size: `4MB`

Then click **Upload**. Open Serial Monitor at **115200 baud**.

Alternatively, with `arduino-cli`:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ble-mouse
arduino-cli upload  --fqbn esp32:esp32:esp32 --port COM<N> ble-mouse
```

---

## First Boot

On power-up the device:
1. Initializes BLE HID (starts advertising immediately)
2. Calibrates gyro bias — **hold the device still for ~400 ms** while the Serial Monitor shows:
   ```
   [IMU] Calibrating gyro, hold still...
   [IMU] Gyro bias: X=0.012 Y=-0.023 Z=0.008 dps
   [MOTION] init OK
   Ready — waiting for BLE connection...
   ```
3. Pair from your host OS (appears as "BLE Mouse")

---

## Tuning

All motion parameters live in `_motion_cfg` inside `ble-mouse.ino`.

| Symptom | Parameter to change |
|---|---|
| Cursor drifts at rest | Increase `deadzone_dps_x/y` (try 2.0–5.0) |
| Wrong X direction | Flip `invert_x` |
| Wrong Y direction | Flip `invert_y` |
| Wrong axis assignment | Swap `x_axis_source` / `y_axis_source` |
| Too slow | Increase `pixels_per_dps` (try 0.15–0.20) |
| Sluggish / laggy | Decrease `smoothing_alpha` (try 0.65) |
| Jittery | Increase `smoothing_alpha` (try 0.90) |
| Cursor jumps | Decrease `max_delta_x/y` |
| WHO_AM_I fails | Try `i2c_addr = 0x69`; check CS = HIGH and pull-ups |

IMU poll rate is set by `IMU_POLL_INTERVAL_MS` (default 10 ms = 100 Hz). Raise to 20 if the BLE stack is still unstable.

---

## Repository Contents

| Path | Description |
|---|---|
| `ble-mouse.ino` | Main sketch |
| `ble_hid.h/cpp` | BLE HID service |
| `ble_report.h/cpp` | HID report packing and notify |
| `mouse_api.h/cpp` | High-level mouse actions |
| `btn.h/cpp` | Generic button driver |
| `icm42688.h/cpp` | ICM-42688-P I2C driver |
| `motion_mapper.h/cpp` | Gyro-to-cursor motion mapper |
| `icm42688_driver_api.md` | IMU driver API specification |
| `imu_cursor_mapper_api.md` | Motion mapper API specification |
| `button_mouse_api.md` | Button/mouse API specification |
| `datasheet_icm42688/` | ICM-42688-P datasheet |
| `test.drawio` | Hardware wiring diagram |
