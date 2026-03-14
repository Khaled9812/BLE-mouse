#include "icm42688.h"
#include <Wire.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Internal register map
// ---------------------------------------------------------------------------
#define ICM_SDA_PIN        21
#define ICM_SCL_PIN        22

#define REG_WHO_AM_I       0x75   // expected: 0x47
#define REG_PWR_MGMT0      0x4E
#define REG_GYRO_CONFIG0   0x4F
#define REG_ACCEL_CONFIG0  0x50
#define REG_ACCEL_DATA_X1  0x1F   // 6 bytes: AX1 AX0 AY1 AY0 AZ1 AZ0
#define REG_GYRO_DATA_X1   0x25   // 6 bytes: GX1 GX0 GY1 GY0 GZ1 GZ0
#define REG_TEMP_DATA1     0x1D

#define PWR_6AXIS_LN       0x0F   // bits[3:2]=11 gyro LN, bits[1:0]=11 accel LN
#define GYRO_500DPS_1KHZ   0x46   // bits[7:5]=010 (500dps), bits[3:0]=0110 (1kHz)
#define ACCEL_4G_1KHZ      0x46   // bits[7:5]=010 (4g),     bits[3:0]=0110 (1kHz)

#define GYRO_LSB_PER_DPS   65.5f
#define ACCEL_LSB_PER_G    8192.0f
#define POWER_SETTLE_MS    50

// ---------------------------------------------------------------------------
// Static Wire helpers
// ---------------------------------------------------------------------------
static imu_status_t wire_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    if (Wire.endTransmission(true) != 0) {
        return IMU_ERR_COMM;
    }
    return IMU_OK;
}

static imu_status_t wire_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    // endTransmission(false) → repeated start, keeps bus held for read
    if (Wire.endTransmission(false) != 0) {
        return IMU_ERR_COMM;
    }
    uint8_t received = Wire.requestFrom((uint8_t)addr, (uint8_t)len, (uint8_t)true);
    if (received != len) {
        return IMU_ERR_COMM;
    }
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return IMU_OK;
}

static imu_status_t wire_read_reg(uint8_t addr, uint8_t reg, uint8_t *value) {
    return wire_read_regs(addr, reg, value, 1);
}

// ---------------------------------------------------------------------------
// imu_init
// ---------------------------------------------------------------------------
imu_status_t imu_init(imu_device_t *imu, const imu_config_t *cfg) {
    if (!imu || !cfg) return IMU_ERR_INVALID_ARG;
    if (imu->initialized) return IMU_ERR_ALREADY_INIT;

    imu->config = *cfg;

    uint32_t clock = cfg->bus_speed_hz ? cfg->bus_speed_hz : 400000UL;
    Wire.begin(ICM_SDA_PIN, ICM_SCL_PIN);
    Wire.setClock(clock);

    // Validate device identity
    uint8_t who = 0;
    if (wire_read_reg(cfg->i2c_addr, REG_WHO_AM_I, &who) != IMU_OK) {
        return IMU_ERR_COMM;
    }
    imu->who_am_i = who;
    if (who != 0x47) {
        Serial.printf("[IMU] WHO_AM_I mismatch: got 0x%02X, expected 0x47\n", who);
        return IMU_ERR_BAD_ID;
    }

    // Power on all axes in low-noise mode
    if (wire_write_reg(cfg->i2c_addr, REG_PWR_MGMT0, PWR_6AXIS_LN) != IMU_OK) {
        return IMU_ERR_COMM;
    }
    delay(POWER_SETTLE_MS);

    // Gyro: ±500 dps, 1 kHz ODR
    if (wire_write_reg(cfg->i2c_addr, REG_GYRO_CONFIG0, GYRO_500DPS_1KHZ) != IMU_OK) {
        return IMU_ERR_COMM;
    }

    // Accel: ±4 g, 1 kHz ODR
    if (wire_write_reg(cfg->i2c_addr, REG_ACCEL_CONFIG0, ACCEL_4G_1KHZ) != IMU_OK) {
        return IMU_ERR_COMM;
    }

    imu->gyro_lsb_per_dps  = GYRO_LSB_PER_DPS;
    imu->accel_lsb_per_g   = ACCEL_LSB_PER_G;

    memset(&imu->calibration, 0, sizeof(imu->calibration));
    imu->calibration.valid = false;

    imu->initialized = true;
    Serial.println("[IMU] init OK");
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_deinit
// ---------------------------------------------------------------------------
imu_status_t imu_deinit(imu_device_t *imu) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->initialized = false;
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_reset  — soft reset then re-apply stored config
// ---------------------------------------------------------------------------
imu_status_t imu_reset(imu_device_t *imu) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    // Soft reset via DEVICE_CONFIG register (0x11), bit 0
    wire_write_reg(imu->config.i2c_addr, 0x11, 0x01);
    delay(100);

    // Re-apply power and config registers
    wire_write_reg(imu->config.i2c_addr, REG_PWR_MGMT0,     PWR_6AXIS_LN);
    delay(POWER_SETTLE_MS);
    wire_write_reg(imu->config.i2c_addr, REG_GYRO_CONFIG0,  GYRO_500DPS_1KHZ);
    wire_write_reg(imu->config.i2c_addr, REG_ACCEL_CONFIG0, ACCEL_4G_1KHZ);
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_who_am_i
// ---------------------------------------------------------------------------
imu_status_t imu_who_am_i(imu_device_t *imu, uint8_t *id) {
    if (!imu || !id) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    return wire_read_reg(imu->config.i2c_addr, REG_WHO_AM_I, id);
}

// ---------------------------------------------------------------------------
// Configuration setters (update struct; call imu_apply_config to write)
// ---------------------------------------------------------------------------
imu_status_t imu_set_power_mode(imu_device_t *imu, imu_power_mode_t mode) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->config.power_mode = mode;
    return IMU_OK;
}

imu_status_t imu_set_gyro_fs(imu_device_t *imu, imu_gyro_fs_t fs) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->config.gyro_fs = fs;
    return IMU_OK;
}

imu_status_t imu_set_accel_fs(imu_device_t *imu, imu_accel_fs_t fs) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->config.accel_fs = fs;
    return IMU_OK;
}

imu_status_t imu_set_gyro_odr(imu_device_t *imu, imu_odr_t odr) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->config.gyro_odr = odr;
    return IMU_OK;
}

imu_status_t imu_set_accel_odr(imu_device_t *imu, imu_odr_t odr) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    imu->config.accel_odr = odr;
    return IMU_OK;
}

imu_status_t imu_apply_config(imu_device_t *imu) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    // For first revision the only config we write is the fixed defaults
    if (wire_write_reg(imu->config.i2c_addr, REG_PWR_MGMT0, PWR_6AXIS_LN) != IMU_OK)
        return IMU_ERR_COMM;
    delay(POWER_SETTLE_MS);
    if (wire_write_reg(imu->config.i2c_addr, REG_GYRO_CONFIG0, GYRO_500DPS_1KHZ) != IMU_OK)
        return IMU_ERR_COMM;
    if (wire_write_reg(imu->config.i2c_addr, REG_ACCEL_CONFIG0, ACCEL_4G_1KHZ) != IMU_OK)
        return IMU_ERR_COMM;
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_read_raw
// Reads accel (0x1F-0x24) and gyro (0x25-0x2A) in a single 12-byte burst.
// ---------------------------------------------------------------------------
imu_status_t imu_read_raw(imu_device_t *imu, imu_raw_sample_t *raw) {
    if (!imu || !raw) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    uint8_t buf[12];
    imu_status_t st = wire_read_regs(imu->config.i2c_addr, REG_ACCEL_DATA_X1, buf, 12);
    if (st != IMU_OK) return st;

    // Bytes 0-5: accel (big-endian int16)
    raw->accel_x = (int16_t)((buf[0]  << 8) | buf[1]);
    raw->accel_y = (int16_t)((buf[2]  << 8) | buf[3]);
    raw->accel_z = (int16_t)((buf[4]  << 8) | buf[5]);
    // Bytes 6-11: gyro (big-endian int16)
    raw->gyro_x  = (int16_t)((buf[6]  << 8) | buf[7]);
    raw->gyro_y  = (int16_t)((buf[8]  << 8) | buf[9]);
    raw->gyro_z  = (int16_t)((buf[10] << 8) | buf[11]);
    raw->temp    = 0; // read separately if needed
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_read_sample
// ---------------------------------------------------------------------------
imu_status_t imu_read_sample(imu_device_t *imu, imu_sample_t *sample) {
    if (!imu || !sample) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    imu_raw_sample_t raw;
    imu_status_t st = imu_read_raw(imu, &raw);
    if (st != IMU_OK) return st;

    // Scale
    sample->accel_g_x  = (float)raw.accel_x / imu->accel_lsb_per_g;
    sample->accel_g_y  = (float)raw.accel_y / imu->accel_lsb_per_g;
    sample->accel_g_z  = (float)raw.accel_z / imu->accel_lsb_per_g;

    sample->gyro_dps_x = (float)raw.gyro_x / imu->gyro_lsb_per_dps;
    sample->gyro_dps_y = (float)raw.gyro_y / imu->gyro_lsb_per_dps;
    sample->gyro_dps_z = (float)raw.gyro_z / imu->gyro_lsb_per_dps;

    // Subtract gyro bias if calibration is valid
    if (imu->calibration.valid) {
        sample->gyro_dps_x -= imu->calibration.gyro_bias_dps_x;
        sample->gyro_dps_y -= imu->calibration.gyro_bias_dps_y;
        sample->gyro_dps_z -= imu->calibration.gyro_bias_dps_z;
    }

    sample->temp_c        = 0.0f; // not read unless use_temperature
    sample->timestamp_ms  = millis();
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_read_temperature_c
// ---------------------------------------------------------------------------
imu_status_t imu_read_temperature_c(imu_device_t *imu, float *temp_c) {
    if (!imu || !temp_c) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;

    uint8_t buf[2];
    imu_status_t st = wire_read_regs(imu->config.i2c_addr, REG_TEMP_DATA1, buf, 2);
    if (st != IMU_OK) return st;

    int16_t raw_temp = (int16_t)((buf[0] << 8) | buf[1]);
    *temp_c = (raw_temp / 132.48f) + 25.0f;
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// imu_calibrate_gyro_bias
// ---------------------------------------------------------------------------
imu_status_t imu_calibrate_gyro_bias(imu_device_t *imu, uint16_t sample_count) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    if (sample_count == 0) return IMU_ERR_INVALID_ARG;

    Serial.println("[IMU] Calibrating gyro, hold still...");

    double sum_x = 0, sum_y = 0, sum_z = 0;
    imu_raw_sample_t raw;

    for (uint16_t i = 0; i < sample_count; i++) {
        if (imu_read_raw(imu, &raw) != IMU_OK) {
            return IMU_ERR_COMM;
        }
        sum_x += raw.gyro_x;
        sum_y += raw.gyro_y;
        sum_z += raw.gyro_z;
        delay(2);
    }

    imu->calibration.gyro_bias_dps_x = (float)(sum_x / sample_count) / imu->gyro_lsb_per_dps;
    imu->calibration.gyro_bias_dps_y = (float)(sum_y / sample_count) / imu->gyro_lsb_per_dps;
    imu->calibration.gyro_bias_dps_z = (float)(sum_z / sample_count) / imu->gyro_lsb_per_dps;
    imu->calibration.valid = true;

    Serial.printf("[IMU] Gyro bias: X=%.3f Y=%.3f Z=%.3f dps\n",
                  imu->calibration.gyro_bias_dps_x,
                  imu->calibration.gyro_bias_dps_y,
                  imu->calibration.gyro_bias_dps_z);
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// Calibration helpers
// ---------------------------------------------------------------------------
imu_status_t imu_set_calibration(imu_device_t *imu, const imu_calibration_t *cal) {
    if (!imu || !cal) return IMU_ERR_INVALID_ARG;
    imu->calibration = *cal;
    return IMU_OK;
}

imu_status_t imu_get_calibration(const imu_device_t *imu, imu_calibration_t *cal) {
    if (!imu || !cal) return IMU_ERR_INVALID_ARG;
    *cal = imu->calibration;
    return IMU_OK;
}

imu_status_t imu_clear_calibration(imu_device_t *imu) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    memset(&imu->calibration, 0, sizeof(imu->calibration));
    imu->calibration.valid = false;
    return IMU_OK;
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------
bool imu_is_initialized(const imu_device_t *imu) {
    return imu && imu->initialized;
}

bool imu_is_data_ready(const imu_device_t *imu) {
    // Polling mode — data is always ready at 1 kHz; just read it
    return imu && imu->initialized;
}

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------
imu_status_t imu_write_reg(imu_device_t *imu, uint8_t reg, uint8_t value) {
    if (!imu) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    return wire_write_reg(imu->config.i2c_addr, reg, value);
}

imu_status_t imu_read_reg(imu_device_t *imu, uint8_t reg, uint8_t *value) {
    if (!imu || !value) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    return wire_read_reg(imu->config.i2c_addr, reg, value);
}

imu_status_t imu_read_regs(imu_device_t *imu, uint8_t start_reg, uint8_t *buf, uint16_t len) {
    if (!imu || !buf) return IMU_ERR_INVALID_ARG;
    if (!imu->initialized) return IMU_ERR_NOT_INIT;
    return wire_read_regs(imu->config.i2c_addr, start_reg, buf, len);
}
