#include "ble_hid.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLECharacteristic.h>
#include <BLESecurity.h>

// BLE appearance value for HID mouse (Bluetooth SIG assigned number)
#define HID_MOUSE_APPEARANCE 0x03C2
static bool _hid_ready = false;
static uint32_t _ready_at_ms = 0;
// ---------------------------------------------------------------------------
// HID report descriptor — 3 buttons + X + Y + Wheel (4 bytes per report)
//
//  Byte 0 : [bit7..3 = padding] [bit2 = middle] [bit1 = right] [bit0 = left]
//  Byte 1 : X  movement  (int8, relative)
//  Byte 2 : Y  movement  (int8, relative)
//  Byte 3 : Wheel scroll (int8, relative)
// ---------------------------------------------------------------------------
static const uint8_t _mouseReportDesc[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    0x85, 0x01,  //   Report ID (1) — must match inputReport(1)
    // --- Buttons ---
    0x05, 0x09,  //     Usage Page (Button)
    0x19, 0x01,  //     Usage Minimum (Button 1 / Left)
    0x29, 0x03,  //     Usage Maximum (Button 3 / Middle)
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x95, 0x03,  //     Report Count (3)
    0x75, 0x01,  //     Report Size (1 bit)
    0x81, 0x02,  //     Input (Data, Variable, Absolute)
    // --- Padding to fill the byte ---
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x05,  //     Report Size (5 bits)
    0x81, 0x03,  //     Input (Constant)
    // --- X, Y, Wheel ---
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum ( 127)
    0x75, 0x08,  //     Report Size (8 bits)
    0x95, 0x03,  //     Report Count (3)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    0xC0,        //   End Collection (Physical)
    0xC0         // End Collection (Application)
};

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static bool _initialized  = false;
static bool _advertising  = false;
static bool _connected    = false;
static bool _paired       = false;
static uint16_t _conn_id  = 0;

static BLEServer*        _server   = nullptr;
static BLEHIDDevice*     _hid      = nullptr;
static BLECharacteristic* _inputReport = nullptr;
static BLEAdvertising*   _adv      = nullptr;

// ---------------------------------------------------------------------------
// Server callbacks
// ---------------------------------------------------------------------------
class _ServerCallbacks : public BLEServerCallbacks {
void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
    _conn_id     = param->connect.conn_id;
    _connected   = true;
    _advertising = false;

    // Re-arm HID input notifications after reconnect
    BLEDescriptor *desc = _inputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (desc) {
        uint8_t val[] = {0x01, 0x00};
        desc->setValue(val, 2);
    }

    Serial.printf("[BLE] onConnect  conn_id=%d\n", _conn_id);
}

void onDisconnect(BLEServer* server) override {
    Serial.printf("[BLE] onDisconnect  conn_id=%d\n", _conn_id);
    _connected = false;
    _conn_id   = 0;
    _advertising = false;
}
};

static _ServerCallbacks _callbacks;

// ---------------------------------------------------------------------------
// API implementation
// ---------------------------------------------------------------------------

ble_err_t esp_ble_init(void) {
    if (_initialized) return BLE_ERR_ALREADY_INIT;

    BLEDevice::init("BLE Mouse");

    // Enable bonding so CCCD state is stored and restored on auto-reconnect.
    // Without this, Windows skips re-enabling notifications after reconnect.
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    _server = BLEDevice::createServer();
    _server->setCallbacks(&_callbacks);

    _hid = new BLEHIDDevice(_server);
    _inputReport = _hid->inputReport(1);  // Report ID 1

    _hid->manufacturer()->setValue("ESP32");
    _hid->pnp(0x02, 0x045E, 0x0000, 0x0110);  // BT SIG, arbitrary product/version
    _hid->hidInfo(0x00, 0x01);                 // country=0, normally connectable

    _hid->reportMap((uint8_t*)_mouseReportDesc, sizeof(_mouseReportDesc));
    _hid->startServices();

    _adv = BLEDevice::getAdvertising();
    _adv->setAppearance(HID_MOUSE_APPEARANCE);
    _adv->addServiceUUID(_hid->hidService()->getUUID());
    _adv->setScanResponse(false);
    _adv->setMinPreferred(0x06);  // recommended for iOS/Windows compatibility

    _initialized = true;
    return BLE_OK;
}

ble_err_t esp_ble_deinit(void) {
    if (!_initialized) return BLE_ERR_NOT_INIT;

    if (_connected)   esp_ble_disconnect();
    if (_advertising) esp_ble_stop_advertising();

    BLEDevice::deinit(true);

    _server      = nullptr;
    _hid         = nullptr;
    _inputReport = nullptr;
    _adv         = nullptr;
    _initialized = false;
    _connected   = false;
    _paired      = false;

    return BLE_OK;
}
bool esp_ble_can_send(void) {
    if (!_connected) return false;

    if (!_hid_ready && millis() >= _ready_at_ms) {
        _hid_ready = true;
    }

    return _hid_ready;
}
ble_err_t esp_ble_start_advertising(void) {
    if (!_initialized)  return BLE_ERR_NOT_INIT;
    if (_advertising)   return BLE_ERR_ALREADY_ADVERTISING;
    if (_connected)     return BLE_ERR_ALREADY_ADVERTISING;

    _adv->start();
    _advertising = true;
    return BLE_OK;
}

ble_err_t esp_ble_stop_advertising(void) {
    if (!_initialized) return BLE_ERR_NOT_INIT;
    if (!_advertising) return BLE_ERR_NOT_ADVERTISING;

    _adv->stop();
    _advertising = false;
    return BLE_OK;
}

ble_err_t esp_ble_disconnect(void) {
    if (!_initialized) return BLE_ERR_NOT_INIT;
    if (!_connected)   return BLE_ERR_NOT_CONNECTED;

    _server->disconnect(_conn_id);
    // State is updated in onDisconnect callback
    return BLE_OK;
}

ble_err_t esp_ble_restart_advertising(void) {
    if (!_initialized) return BLE_ERR_NOT_INIT;

    if (_advertising) {
        _adv->stop();
        _advertising = false;
    }

    _adv->start();
    _advertising = true;
    return BLE_OK;
}

ble_state_t esp_ble_get_state(void) {
    if (!_initialized)  return BLE_STATE_IDLE;
    if (_connected)     return BLE_STATE_CONNECTED;
    if (_advertising)   return BLE_STATE_ADVERTISING;
    return BLE_STATE_IDLE;
}

bool esp_ble_is_connected(void) {
    return _connected;
}

bool esp_ble_is_paired(void) {
    return _paired;
}

BLECharacteristic* ble_hid_get_input_report(void) {
    return _inputReport;
}
