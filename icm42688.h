#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Return Status
// ---------------------------------------------------------------------------
typedef enum {
    IMU_OK = 0,
    IMU_ERR_INVALID_ARG,
    IMU_ERR_NOT_INIT,
    IMU_ERR_ALREADY_INIT,
    IMU_ERR_COMM,
    IMU_ERR_TIMEOUT,
    IMU_ERR_BAD_ID,
    IMU_ERR_CONFIG,
    IMU_ERR_INTERNAL
} imu_status_t;

// ---------------------------------------------------------------------------
// Interface Type
// ---------------------------------------------------------------------------
typedef enum {
    IMU_IF_I2C = 0,
    IMU_IF_SPI
} imu_interface_t;

// ---------------------------------------------------------------------------
// Power Mode
// ---------------------------------------------------------------------------
typedef enum {
    IMU_POWER_SLEEP = 0,
    IMU_POWER_STANDBY,
    IMU_POWER_ACCEL_LP,
    IMU_POWER_ACCEL_LN,
    IMU_POWER_GYRO_LN,
    IMU_POWER_6AXIS_LN
} imu_power_mode_t;

// ---------------------------------------------------------------------------
// Gyroscope Full Scale
// ---------------------------------------------------------------------------
typedef enum {
    IMU_GYRO_FS_2000DPS = 0,
    IMU_GYRO_FS_1000DPS,
    IMU_GYRO_FS_500DPS,
    IMU_GYRO_FS_250DPS,
    IMU_GYRO_FS_125DPS,
    IMU_GYRO_FS_62DPS,
    IMU_GYRO_FS_31DPS,
    IMU_GYRO_FS_15DPS
} imu_gyro_fs_t;

// ---------------------------------------------------------------------------
// Accelerometer Full Scale
// ---------------------------------------------------------------------------
typedef enum {
    IMU_ACCEL_FS_16G = 0,
    IMU_ACCEL_FS_8G,
    IMU_ACCEL_FS_4G,
    IMU_ACCEL_FS_2G
} imu_accel_fs_t;

// ---------------------------------------------------------------------------
// Output Data Rate
// ---------------------------------------------------------------------------
typedef enum {
    IMU_ODR_32KHZ = 0,
    IMU_ODR_16KHZ,
    IMU_ODR_8KHZ,
    IMU_ODR_4KHZ,
    IMU_ODR_2KHZ,
    IMU_ODR_1KHZ,
    IMU_ODR_200HZ,
    IMU_ODR_100HZ,
    IMU_ODR_50HZ,
    IMU_ODR_25HZ
} imu_odr_t;

// ---------------------------------------------------------------------------
// Raw Sample
// ---------------------------------------------------------------------------
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} imu_raw_sample_t;

// ---------------------------------------------------------------------------
// Scaled Sample
// ---------------------------------------------------------------------------
typedef struct {
    float accel_g_x;
    float accel_g_y;
    float accel_g_z;

    float gyro_dps_x;
    float gyro_dps_y;
    float gyro_dps_z;

    float temp_c;

    uint32_t timestamp_ms;
} imu_sample_t;

// ---------------------------------------------------------------------------
// Calibration / Bias
// ---------------------------------------------------------------------------
typedef struct {
    float gyro_bias_dps_x;
    float gyro_bias_dps_y;
    float gyro_bias_dps_z;

    float accel_bias_g_x;
    float accel_bias_g_y;
    float accel_bias_g_z;

    bool valid;
} imu_calibration_t;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
typedef struct {
    imu_interface_t interface_type;

    uint8_t  i2c_addr;
    uint32_t bus_speed_hz;

    imu_power_mode_t power_mode;
    imu_gyro_fs_t    gyro_fs;
    imu_accel_fs_t   accel_fs;
    imu_odr_t        gyro_odr;
    imu_odr_t        accel_odr;

    bool use_temperature;
    bool enable_fifo;
    bool enable_interrupts;
} imu_config_t;

// ---------------------------------------------------------------------------
// Device Object
// ---------------------------------------------------------------------------
typedef struct {
    bool             initialized;
    imu_config_t     config;
    imu_calibration_t calibration;

    float gyro_lsb_per_dps;
    float accel_lsb_per_g;

    uint8_t who_am_i;
} imu_device_t;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Lifecycle
imu_status_t imu_init(imu_device_t *imu, const imu_config_t *cfg);
imu_status_t imu_deinit(imu_device_t *imu);
imu_status_t imu_reset(imu_device_t *imu);
imu_status_t imu_who_am_i(imu_device_t *imu, uint8_t *id);

// Configuration
imu_status_t imu_set_power_mode(imu_device_t *imu, imu_power_mode_t mode);
imu_status_t imu_set_gyro_fs(imu_device_t *imu, imu_gyro_fs_t fs);
imu_status_t imu_set_accel_fs(imu_device_t *imu, imu_accel_fs_t fs);
imu_status_t imu_set_gyro_odr(imu_device_t *imu, imu_odr_t odr);
imu_status_t imu_set_accel_odr(imu_device_t *imu, imu_odr_t odr);
imu_status_t imu_apply_config(imu_device_t *imu);

// Read
imu_status_t imu_read_raw(imu_device_t *imu, imu_raw_sample_t *raw);
imu_status_t imu_read_sample(imu_device_t *imu, imu_sample_t *sample);
imu_status_t imu_read_temperature_c(imu_device_t *imu, float *temp_c);

// Calibration
imu_status_t imu_calibrate_gyro_bias(imu_device_t *imu, uint16_t sample_count);
imu_status_t imu_set_calibration(imu_device_t *imu, const imu_calibration_t *cal);
imu_status_t imu_get_calibration(const imu_device_t *imu, imu_calibration_t *cal);
imu_status_t imu_clear_calibration(imu_device_t *imu);

// Status
bool         imu_is_initialized(const imu_device_t *imu);
bool         imu_is_data_ready(const imu_device_t *imu);

// Low-level register access (debug / bring-up)
imu_status_t imu_write_reg(imu_device_t *imu, uint8_t reg, uint8_t value);
imu_status_t imu_read_reg(imu_device_t *imu, uint8_t reg, uint8_t *value);
imu_status_t imu_read_regs(imu_device_t *imu, uint8_t start_reg, uint8_t *buf, uint16_t len);
