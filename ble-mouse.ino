#include "mouse_api.h"
#include "btn.h"
#include "ble_hid.h"
#include "icm42688.h"
#include "motion_mapper.h"

// ---------------------------------------------------------------------------
// Button descriptors
// ---------------------------------------------------------------------------
static button_t button_b1 = {
    .id                  = BTN_ID_B1,
    .function            = BTN_FUNC_RIGHT_CLICK,
    .gpio                = 19,
    .active_low          = false,   // GPIO → pull-down → GND; button → 3v3 (active HIGH)
    .debounce_ms         = 20,
    .short_click_min_ms  = 50,
    .short_click_max_ms  = 250,
    .long_click_ms       = 500,
    .double_click_gap_ms = 0
};

static button_t button_b2 = {
    .id                  = BTN_ID_B2,
    .function            = BTN_FUNC_LEFT_CLICK,
    .gpio                = 18,
    .active_low          = false,   // same wiring
    .debounce_ms         = 20,
    .short_click_min_ms  = 50,
    .short_click_max_ms  = 250,
    .long_click_ms       = 500,
    .double_click_gap_ms = 0
};
static button_t button_b3 = {
    .id                  = BTN_ID_B3,
    .function            = BTN_FUNC_DISCONNECT,   // this button is not a mouse button
    .gpio                = 23,                // change if you wired another GPIO
    .active_low          = false,             // same wiring style as B1/B2
    .debounce_ms         = 20,
    .short_click_min_ms  = 50,
    .short_click_max_ms  = 250,
    .long_click_ms       = 800,               // safer for disconnect
    .double_click_gap_ms = 0
};
// ---------------------------------------------------------------------------
// IMU + Motion mapper objects
// ---------------------------------------------------------------------------
static imu_device_t    _imu;
static imu_sample_t    _imu_sample;
static motion_mapper_t _mapper;
static cursor_delta_t  _cursor;
static uint32_t        _last_imu_ms = 0;
#define IMU_POLL_INTERVAL_MS  10    // 100 Hz — raise to 20 for 50 Hz if still unstable

static imu_config_t _imu_cfg = {
    .interface_type    = IMU_IF_I2C,
    .i2c_addr          = 0x68,
    .bus_speed_hz      = 400000,
    .power_mode        = IMU_POWER_6AXIS_LN,
    .gyro_fs           = IMU_GYRO_FS_500DPS,
    .accel_fs          = IMU_ACCEL_FS_4G,
    .gyro_odr          = IMU_ODR_1KHZ,
    .accel_odr         = IMU_ODR_1KHZ,
    .use_temperature   = false,
    .enable_fifo       = false,
    .enable_interrupts = false
};

static motion_mapper_config_t _motion_cfg = {
    .x_axis_source             = MOTION_AXIS_GYRO_Z,
    .y_axis_source             = MOTION_AXIS_GYRO_X,
    .invert_x                  = true,
    .invert_y                  = true,
    .smoothing_alpha           = 0.80f,
    .max_delta_x               = 100,
    .max_delta_y               = 100,
    .ignore_roll               = true,
    .use_rate_mode             = true,
    .enable_pitch_stabilization = false,
    .dps_profile = {
        .deadzone_dps_x       = 1.5f,
        .deadzone_dps_y       = 1.5f,
        .response_start_dps_x = 8.0f,
        .response_start_dps_y = 8.0f,
        .response_max_dps_x   = 180.0f,
        .response_max_dps_y   = 180.0f,
        .pixels_per_dps_x     = 0.10f,
        .pixels_per_dps_y     = 0.10f,
        .slow_zone_gain_x     = 1.0f,
        .slow_zone_gain_y     = 1.0f,
        .fast_zone_gain_x     = 1.5f,
        .fast_zone_gain_y     = 1.5f
    }
};

// ---------------------------------------------------------------------------
// Per-button mouse state machine
// ---------------------------------------------------------------------------
typedef enum {
    BTN_MOUSE_IDLE = 0,
    BTN_MOUSE_HELD
} btn_mouse_state_t;

static btn_mouse_state_t _state_b1 = BTN_MOUSE_IDLE;
static btn_mouse_state_t _state_b2 = BTN_MOUSE_IDLE;

static void btn_mouse_update(button_t *btn, btn_mouse_state_t *state, mouse_button_t mouse_btn) {
    bool pressed = btn_is_pressed(btn);
    switch (*state) {
        case BTN_MOUSE_IDLE:
            if (pressed) {
                mouse_press(mouse_btn);
                *state = BTN_MOUSE_HELD;
                Serial.printf("[BTN] %s PRESS\n", mouse_btn == MOUSE_BTN_RIGHT ? "RIGHT" : "LEFT");
            }
            break;
        case BTN_MOUSE_HELD:
            if (!pressed) {
                mouse_release(mouse_btn);
                *state = BTN_MOUSE_IDLE;
                Serial.printf("[BTN] %s RELEASE\n", mouse_btn == MOUSE_BTN_RIGHT ? "RIGHT" : "LEFT");
            }
            break;
    }
}

static void handle_ble_disconnect_button(button_t *btn) {
    button_event_t evt = btn_get_event(btn);

    if (evt != BTN_EVENT_LONG_CLICK) {
        return;
    }

    ble_state_t state = esp_ble_get_state();

    switch (state) {
        case BLE_STATE_CONNECTED:
            Serial.println("[BLE] B3 long press -> disconnecting and going to IDLE");
            esp_ble_disconnect();
            break;

        case BLE_STATE_IDLE:
            Serial.println("[BLE] B3 long press -> starting advertising");
            esp_ble_start_advertising();
            break;

        case BLE_STATE_ADVERTISING:
            Serial.println("[BLE] B3 long press -> already advertising");
            break;

        default:
            Serial.println("[BLE] B3 long press -> unknown BLE state");
            break;
    }
}

// ---------------------------------------------------------------------------
// BLE state tracking
// ---------------------------------------------------------------------------
static ble_state_t _last_state = BLE_STATE_IDLE;

static const char* stateStr(ble_state_t s) {
    switch (s) {
        case BLE_STATE_IDLE:        return "IDLE";
        case BLE_STATE_ADVERTISING: return "ADVERTISING";
        case BLE_STATE_CONNECTED:   return "CONNECTED";
        default:                    return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    mouse_init();
    btn_init(&button_b1);
    btn_init(&button_b2);
    btn_init(&button_b3);

    if (imu_init(&_imu, &_imu_cfg) == IMU_OK) {
        imu_calibrate_gyro_bias(&_imu, 200);   // ~400 ms — hold still
    } else {
        Serial.println("[IMU] init failed — check wiring");
    }
    motion_mapper_init(&_mapper, &_motion_cfg);

    _last_state = esp_ble_get_state();
    Serial.println("Ready — waiting for BLE connection...");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    uint32_t now_ms = millis();

    if (imu_is_initialized(&_imu) && motion_mapper_is_initialized(&_mapper)) {
        if ((now_ms - _last_imu_ms) >= IMU_POLL_INTERVAL_MS) {
            _last_imu_ms = now_ms;
            if (imu_read_sample(&_imu, &_imu_sample) == IMU_OK) {
                motion_mapper_process_sample(&_mapper, &_imu_sample, &_cursor);
                if (_cursor.valid && mouse_is_connected()) {
                    mouse_move((int8_t)_cursor.dx, (int8_t)_cursor.dy);
                }
            }
        }
    }

    btn_update(&button_b1, now_ms);
    btn_update(&button_b2, now_ms);
    btn_update(&button_b3, now_ms);
    handle_ble_disconnect_button(&button_b3);

    if (mouse_is_connected()) {
        btn_mouse_update(&button_b1, &_state_b1, MOUSE_BTN_RIGHT);
        btn_mouse_update(&button_b2, &_state_b2, MOUSE_BTN_LEFT);
    } else {
        // Reset held state on disconnect — don't carry a held button across reconnect
        _state_b1 = BTN_MOUSE_IDLE;
        _state_b2 = BTN_MOUSE_IDLE;
    }

    // BLE state transitions
    ble_state_t state = esp_ble_get_state();
      if (state != _last_state) {
          Serial.printf("[STATE] %s -> %s\n", stateStr(_last_state), stateStr(state));
          _last_state = state;
      }
}
