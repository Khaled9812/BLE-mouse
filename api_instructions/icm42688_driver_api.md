# ICM-42688-P Driver API Specification

## 1. Purpose

This document defines the low-level driver API for the **ICM-42688-P** 6-axis IMU.

The driver API is responsible for:

- bringing the IMU up over **I2C**
- configuring power mode, ODR, and full-scale ranges
- reading raw and scaled accelerometer / gyroscope / temperature data
- handling offset calibration and status checks
- exposing a clean hardware abstraction to upper layers

This API is intentionally hardware-oriented and does **not** contain cursor or mouse logic.

---

## 2. Device Assumptions

This API assumes the project uses the ICM-42688-P in **I2C mode**.

### 2.1 Electrical / Bus Assumptions

- Host interface: **I2C**
- I2C clock: up to **1 MHz**
- Device address:
  - `0x68` when `AP_AD0 = 0`
  - `0x69` when `AP_AD0 = 1`
- `AP_CS` must be tied to **VDDIO** in I2C mode
- SDA and SCL require pull-up resistors

### 2.2 Functional Assumptions

- Device provides:
  - 3-axis gyroscope
  - 3-axis accelerometer
  - temperature sensor
  - 2 kB FIFO
  - programmable interrupts
- The driver will primarily use **sensor data registers**, not FIFO, for the first revision

---

## 3. Driver Design Goals

The driver must:

- be reusable
- be independent of the mouse layer
- support both raw and scaled data access
- allow calibration of gyro bias and optional accel bias
- allow future support for FIFO and interrupts

---

## 4. Data Types

## 4.1 Return Status

```c
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
```

---

## 4.2 Interface Type

```c
typedef enum {
    IMU_IF_I2C = 0,
    IMU_IF_SPI
} imu_interface_t;
```

---

## 4.3 Power Mode

```c
typedef enum {
    IMU_POWER_SLEEP = 0,
    IMU_POWER_STANDBY,
    IMU_POWER_ACCEL_LP,
    IMU_POWER_ACCEL_LN,
    IMU_POWER_GYRO_LN,
    IMU_POWER_6AXIS_LN
} imu_power_mode_t;
```

---

## 4.4 Gyroscope Full Scale

```c
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
```

---

## 4.5 Accelerometer Full Scale

```c
typedef enum {
    IMU_ACCEL_FS_16G = 0,
    IMU_ACCEL_FS_8G,
    IMU_ACCEL_FS_4G,
    IMU_ACCEL_FS_2G
} imu_accel_fs_t;
```

---

## 4.6 Output Data Rate

```c
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
```

---

## 4.7 Raw Sample Format

```c
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} imu_raw_sample_t;
```

---

## 4.8 Scaled Sample Format

```c
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
```

---

## 4.9 Bias / Calibration Data

```c
typedef struct {
    float gyro_bias_dps_x;
    float gyro_bias_dps_y;
    float gyro_bias_dps_z;

    float accel_bias_g_x;
    float accel_bias_g_y;
    float accel_bias_g_z;

    bool valid;
} imu_calibration_t;
```

---

## 4.10 Driver Configuration

```c
typedef struct {
    imu_interface_t interface_type;

    uint8_t i2c_addr;
    uint32_t bus_speed_hz;

    imu_power_mode_t power_mode;
    imu_gyro_fs_t gyro_fs;
    imu_accel_fs_t accel_fs;
    imu_odr_t gyro_odr;
    imu_odr_t accel_odr;

    bool use_temperature;
    bool enable_fifo;
    bool enable_interrupts;
} imu_config_t;
```

---

## 4.11 Device Object

```c
typedef struct {
    bool initialized;
    imu_config_t config;
    imu_calibration_t calibration;

    float gyro_lsb_per_dps;
    float accel_lsb_per_g;

    uint8_t who_am_i;
} imu_device_t;
```

---

## 5. Public API

## 5.1 Initialization and Lifecycle

```c
imu_status_t imu_init(imu_device_t *imu, const imu_config_t *cfg);
imu_status_t imu_deinit(imu_device_t *imu);
imu_status_t imu_reset(imu_device_t *imu);
imu_status_t imu_who_am_i(imu_device_t *imu, uint8_t *id);
```

### Behavior

- `imu_init()` initializes the bus, validates the chip ID, configures the IMU, and computes scale factors.
- `imu_deinit()` releases driver state.
- `imu_reset()` performs a soft reset and restores the current configuration.
- `imu_who_am_i()` reads and returns the `WHO_AM_I` register.

---

## 5.2 Configuration API

```c
imu_status_t imu_set_power_mode(imu_device_t *imu, imu_power_mode_t mode);
imu_status_t imu_set_gyro_fs(imu_device_t *imu, imu_gyro_fs_t fs);
imu_status_t imu_set_accel_fs(imu_device_t *imu, imu_accel_fs_t fs);
imu_status_t imu_set_gyro_odr(imu_device_t *imu, imu_odr_t odr);
imu_status_t imu_set_accel_odr(imu_device_t *imu, imu_odr_t odr);
imu_status_t imu_apply_config(imu_device_t *imu);
```

### Notes

- `imu_apply_config()` writes the staged configuration to the sensor.
- Initial recommended mode for cursor work:
  - `IMU_POWER_6AXIS_LN`
  - `IMU_GYRO_FS_500DPS`
  - `IMU_ACCEL_FS_4G`
  - `IMU_ODR_1KHZ` for gyro
  - `IMU_ODR_1KHZ` for accel

---

## 5.3 Read API

```c
imu_status_t imu_read_raw(imu_device_t *imu, imu_raw_sample_t *raw);
imu_status_t imu_read_sample(imu_device_t *imu, imu_sample_t *sample);
imu_status_t imu_read_temperature_c(imu_device_t *imu, float *temp_c);
```

### Behavior

- `imu_read_raw()` reads the current sensor registers without scaling.
- `imu_read_sample()` reads registers and converts them to engineering units.
- `imu_read_temperature_c()` returns temperature in °C.

---

## 5.4 Calibration API

```c
imu_status_t imu_calibrate_gyro_bias(imu_device_t *imu, uint16_t sample_count);
imu_status_t imu_set_calibration(imu_device_t *imu, const imu_calibration_t *cal);
imu_status_t imu_get_calibration(const imu_device_t *imu, imu_calibration_t *cal);
imu_status_t imu_clear_calibration(imu_device_t *imu);
```

### Behavior

- `imu_calibrate_gyro_bias()` assumes the device is stationary and averages multiple samples.
- Gyro bias correction is mandatory for cursor stability.
- Accelerometer bias correction is optional for first revision.

---

## 5.5 Status / Health API

```c
bool imu_is_initialized(const imu_device_t *imu);
bool imu_is_data_ready(const imu_device_t *imu);
imu_status_t imu_get_last_error(const imu_device_t *imu);
```

---

## 5.6 Optional Low-Level Register API

These functions are optional but useful during bring-up and debugging.

```c
imu_status_t imu_write_reg(imu_device_t *imu, uint8_t reg, uint8_t value);
imu_status_t imu_read_reg(imu_device_t *imu, uint8_t reg, uint8_t *value);
imu_status_t imu_read_regs(imu_device_t *imu, uint8_t start_reg, uint8_t *buf, uint16_t len);
```

---

## 6. Register Usage for First Revision

The first revision of the driver should use the following register groups:

- `WHO_AM_I`
- `PWR_MGMT0`
- `GYRO_CONFIG0`
- `ACCEL_CONFIG0`
- temperature data registers
- accelerometer data registers
- gyroscope data registers
- optional interrupt status registers

FIFO support is intentionally deferred to a later revision.

---

## 7. Scaling Rules

The driver must convert raw values using the currently selected full-scale range.

### Recommended default scaling

#### Gyroscope at ±500 dps

```text
65.5 LSB per dps
```

#### Accelerometer at ±4 g

```text
8192 LSB per g
```

### Conversion

```c
gyro_dps = raw_gyro / gyro_lsb_per_dps;
accel_g  = raw_accel / accel_lsb_per_g;
```

### Temperature conversion

```c
temp_c = (raw_temp / 132.48f) + 25.0f;
```

---

## 8. Recommended Initial Configuration for Cursor Use

For an air-mouse style device, the recommended initial configuration is:

- power mode: **6-axis low-noise**
- gyroscope full-scale: **±500 dps**
- accelerometer full-scale: **±4 g**
- gyro ODR: **1 kHz**
- accel ODR: **1 kHz**
- FIFO: disabled for first revision
- interrupts: disabled for first revision

Reason:

- gyro is the main motion source for cursor movement
- accel is still needed for tilt / gravity reference and future filtering
- 1 kHz gives responsive data for cursor control without overcomplicating the first revision

---

## 9. Driver Execution Model

The driver is intended to be polled from the main loop or a periodic task.

Typical usage:

```c
imu_device_t imu;
imu_config_t cfg = { ... };
imu_sample_t sample;

imu_init(&imu, &cfg);

while (1) {
    imu_read_sample(&imu, &sample);
    // pass sample to motion-to-cursor layer
}
```

---

## 10. Future Extensions

The API must allow later support for:

- FIFO burst reads
- interrupt-driven data-ready handling
- self-test
- external clock input support
- user programmable digital filter control
- persistent calibration storage

---

## 11. Summary

This driver API owns:

- bus communication
- register configuration
- raw/scaled data access
- calibration
- device state

It does **not** own:

- cursor mapping
- BLE mouse actions
- UI behavior
- gesture behavior

Those belong to the upper motion-translation layer.
