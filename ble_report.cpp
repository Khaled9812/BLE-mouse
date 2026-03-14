#include "ble_report.h"

#include <BLECharacteristic.h>
#include <Arduino.h>

extern BLECharacteristic* ble_hid_get_input_report(void);

ble_err_t ble_hid_send_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
    if (!esp_ble_is_connected()) {
        Serial.println("[REPORT] not connected");
        return BLE_ERR_NOT_CONNECTED;
    }

    BLECharacteristic* report = ble_hid_get_input_report();
    if (!report) {
        Serial.println("[REPORT] input report char is null");
        return BLE_ERR_NOT_INIT;
    }

    uint8_t data[4] = { buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel };
    Serial.printf("[REPORT] sending buttons=0x%02X x=%d y=%d wheel=%d\n", buttons, x, y, wheel);
    report->setValue(data, sizeof(data));
    report->notify();
    Serial.println("[REPORT] notify sent");

    return BLE_OK;
}
