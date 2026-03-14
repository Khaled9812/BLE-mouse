# IMU Motion-to-Cursor Mapper API Specification

## 1. Purpose

This document defines the **motion interpretation layer** that converts IMU data into mouse cursor movement.

This layer sits above the IMU driver and below the BLE mouse API.

It is responsible for:

- interpreting IMU motion according to the project behavior
- filtering noise and drift
- applying dead-zone and sensitivity
- applying a configurable **degrees-per-second (dps) profile**
- converting motion into `dx` and `dy` cursor deltas
- ignoring unwanted axes such as roll

This layer does **not** access hardware directly.

---

## 2. Intended User Behavior

The project target behavior is:

- **Pitch up / down** → move cursor **up / down**
- **Yaw left / right** → move cursor **left / right**
- **Roll** → ignored for cursor movement

### Important Note

The ICM-42688-P is a **6-axis IMU** (gyro + accel only).

That means:

- pitch can be stabilized using accelerometer information
- yaw does **not** have an absolute heading reference
- yaw will drift over time if treated as an absolute angle

Therefore, for cursor control, the recommended strategy is:

- use **gyro rate** as the primary motion input
- convert angular velocity to cursor velocity
- do **not** rely on absolute yaw angle as the main control variable

---

## 3. Layer Responsibilities

This API must:

- consume scaled IMU samples from the driver layer
- remove gyro bias and small jitter
- apply filtering / smoothing
- apply a configurable dps response profile
- generate signed cursor deltas
- provide a clean interface to the mouse layer

---

## 4. Data Types

## 4.1 Return Status

```c
typedef enum {
    MOTION_OK = 0,
    MOTION_ERR_INVALID_ARG,
    MOTION_ERR_NOT_INIT,
    MOTION_ERR_BAD_CONFIG,
    MOTION_ERR_INTERNAL
} motion_status_t;
```

---

## 4.2 Motion Axes

```c
typedef enum {
    MOTION_AXIS_NONE = 0,
    MOTION_AXIS_GYRO_X,
    MOTION_AXIS_GYRO_Y,
    MOTION_AXIS_GYRO_Z,
    MOTION_AXIS_ACCEL_X,
    MOTION_AXIS_ACCEL_Y,
    MOTION_AXIS_ACCEL_Z
} motion_axis_t;
```

---

## 4.3 Cursor Output

```c
typedef struct {
    int16_t dx;
    int16_t dy;
    bool valid;
} cursor_delta_t;
```

---

## 4.4 Motion Filter State

```c
typedef struct {
    float filtered_x;
    float filtered_y;

    float last_pitch_deg;
    float last_yaw_deg;

    uint32_t last_timestamp_ms;
} motion_state_t;
```

---

## 4.5 Adjustable DPS Profile

This is the main tuning block you asked for.
The motion feel should be adjustable later by editing this structure rather than rewriting the mapper.

```c
typedef struct {
    float deadzone_dps_x;
    float deadzone_dps_y;

    float response_start_dps_x;
    float response_start_dps_y;

    float response_max_dps_x;
    float response_max_dps_y;

    float pixels_per_dps_x;
    float pixels_per_dps_y;

    float slow_zone_gain_x;
    float slow_zone_gain_y;

    float fast_zone_gain_x;
    float fast_zone_gain_y;
} motion_dps_profile_t;
```

### Meaning

- `deadzone_dps_*`:
  ignore tiny motion below this rate.
- `response_start_dps_*`:
  the dps where intentional cursor motion begins to feel active.
- `response_max_dps_*`:
  clamp or saturate input above this rate.
- `pixels_per_dps_*`:
  base conversion from dps to cursor delta.
- `slow_zone_gain_*`:
  extra control for fine movement.
- `fast_zone_gain_*`:
  extra gain for larger hand motion.

This makes the cursor feel tunable without changing the algorithm itself.

---

## 4.6 Mapper Configuration

```c
typedef struct {
    motion_axis_t x_axis_source;
    motion_axis_t y_axis_source;

    bool invert_x;
    bool invert_y;

    float smoothing_alpha;

    int16_t max_delta_x;
    int16_t max_delta_y;

    bool ignore_roll;
    bool use_rate_mode;
    bool enable_pitch_stabilization;

    motion_dps_profile_t dps_profile;
} motion_mapper_config_t;
```

---

## 4.7 Mapper Object

```c
typedef struct {
    bool initialized;
    motion_mapper_config_t config;
    motion_state_t state;
} motion_mapper_t;
```

---

## 5. Design Strategy

## 5.1 Cursor X Source

Cursor X must come from **yaw motion**.

Recommended default:

- use **gyro Z rate** as cursor X source if the board is mounted flat in the hand

If board orientation differs, the axis mapping must be configurable.

## 5.2 Cursor Y Source

Cursor Y must come from **pitch motion**.

Recommended default:

- use **gyro X** or **gyro Y** depending on actual board mounting

## 5.3 Roll Handling

Roll must be ignored for cursor movement in this project revision.

---

## 6. Recommended First-Revision Mapping

Because this is an air mouse and not a full 3D orientation tracker, the recommended first revision is:

- **rate-based mapping**, not angle-based mapping
- **yaw rate** → `dx`
- **pitch rate** → `dy`
- roll ignored

### Reason

Rate-based control:

- feels more like a mouse
- avoids absolute yaw drift problems
- is simpler than full pose estimation
- is easier to tune
- matches a dps-profile approach very well

---

## 7. Public API

## 7.1 Lifecycle API

```c
motion_status_t motion_mapper_init(motion_mapper_t *mapper,
                                   const motion_mapper_config_t *cfg);
motion_status_t motion_mapper_deinit(motion_mapper_t *mapper);
motion_status_t motion_mapper_reset(motion_mapper_t *mapper);
```

---

## 7.2 Configuration API

```c
motion_status_t motion_mapper_set_config(motion_mapper_t *mapper,
                                         const motion_mapper_config_t *cfg);
motion_status_t motion_mapper_get_config(const motion_mapper_t *mapper,
                                         motion_mapper_config_t *cfg);
motion_status_t motion_mapper_set_dps_profile(motion_mapper_t *mapper,
                                              const motion_dps_profile_t *profile);
motion_status_t motion_mapper_get_dps_profile(const motion_mapper_t *mapper,
                                              motion_dps_profile_t *profile);
```

---

## 7.3 Processing API

```c
motion_status_t motion_mapper_process_sample(motion_mapper_t *mapper,
                                             const imu_sample_t *sample,
                                             cursor_delta_t *delta);
```

### Behavior

This is the main function.

It must:

1. read the configured source axes from the IMU sample
2. apply dps dead-zone
3. apply smoothing
4. apply the dps profile gains
5. clamp the result
6. convert to integer `dx` and `dy`

---

## 7.4 Optional Helper API

```c
motion_status_t motion_mapper_set_invert(motion_mapper_t *mapper,
                                         bool invert_x,
                                         bool invert_y);
motion_status_t motion_mapper_set_max_delta(motion_mapper_t *mapper,
                                            int16_t max_dx,
                                            int16_t max_dy);
```

---

## 7.5 State Query API

```c
bool motion_mapper_is_initialized(const motion_mapper_t *mapper);
```

---

## 8. Processing Rules

## 8.1 Axis Selection

The mapper must allow flexible axis selection because the board may be mounted in different physical orientations.

Recommended default example:

```c
.x_axis_source = MOTION_AXIS_GYRO_Z,
.y_axis_source = MOTION_AXIS_GYRO_X,
```

This must remain configurable.

---

## 8.2 DPS Dead-Zone

A dead-zone is required to suppress tiny unwanted movement caused by sensor noise and hand tremor.

Example:

```c
if (fabs(x_dps) < profile.deadzone_dps_x) x_dps = 0;
if (fabs(y_dps) < profile.deadzone_dps_y) y_dps = 0;
```

---

## 8.3 Smoothing

A first-order low-pass filter is enough for the first revision.

Example:

```c
filtered = alpha * previous + (1 - alpha) * current;
```

Where:

- `alpha` close to `1.0` = smoother but slower
- `alpha` lower = faster but noisier

Recommended starting value:

```text
0.80
```

---

## 8.4 DPS-Based Conversion

The mapper should work directly in dps before converting to cursor counts.

Example idea:

```c
usable_x = clamp(filtered_x, -profile.response_max_dps_x, profile.response_max_dps_x);
usable_y = clamp(filtered_y, -profile.response_max_dps_y, profile.response_max_dps_y);

cursor_x = usable_x * profile.pixels_per_dps_x;
cursor_y = usable_y * profile.pixels_per_dps_y;
```

A two-zone gain model may also be used:

- below `response_start_dps_*` use `slow_zone_gain_*`
- above `response_start_dps_*` use `fast_zone_gain_*`

This gives better fine control at low motion and faster travel at higher motion.

---

## 8.5 Clamping

To prevent large spikes from causing huge cursor jumps:

```c
dx = clamp(dx, -max_delta_x, max_delta_x);
dy = clamp(dy, -max_delta_y, max_delta_y);
```

---

## 8.6 Inversion

The mapper must support axis inversion because physical mounting may reverse perceived motion.

---

## 9. Optional Pitch Stabilization

Pitch can optionally be stabilized using accelerometer information.

This is useful if later you want:

- pitch-angle based control
- gravity-assisted filtering
- a hybrid gyro/accel complementary filter

First revision recommendation:

- keep **yaw** rate-based
- keep **pitch** primarily gyro-rate based
- do not overcomplicate the first revision with full fusion unless needed

---

## 10. Recommended Default Configuration

```c
motion_mapper_config_t motion_cfg = {
    .x_axis_source = MOTION_AXIS_GYRO_Z,
    .y_axis_source = MOTION_AXIS_GYRO_X,

    .invert_x = false,
    .invert_y = true,

    .smoothing_alpha = 0.80f,

    .max_delta_x = 25,
    .max_delta_y = 25,

    .ignore_roll = true,
    .use_rate_mode = true,
    .enable_pitch_stabilization = false,

    .dps_profile = {
        .deadzone_dps_x = 1.2f,
        .deadzone_dps_y = 1.2f,

        .response_start_dps_x = 8.0f,
        .response_start_dps_y = 8.0f,

        .response_max_dps_x = 180.0f,
        .response_max_dps_y = 180.0f,

        .pixels_per_dps_x = 0.10f,
        .pixels_per_dps_y = 0.10f,

        .slow_zone_gain_x = 1.0f,
        .slow_zone_gain_y = 1.0f,

        .fast_zone_gain_x = 1.5f,
        .fast_zone_gain_y = 1.5f,
    }
};
```

### Notes

These are starting values only.
They are intentionally placed inside a struct so you can retune the dps behavior later without changing the interface.

---

## 11. Example Flow

```c
imu_sample_t sample;
cursor_delta_t delta;
motion_dps_profile_t profile;

imu_read_sample(&imu, &sample);
motion_mapper_get_dps_profile(&mapper, &profile);
motion_mapper_process_sample(&mapper, &sample, &delta);

if (delta.valid) {
    mouse_move((int8_t)delta.dx, (int8_t)delta.dy);
}
```

---

## 12. Future Extensions

This API must allow future support for:

- dynamic sensitivity scaling
- acceleration curves
- gesture detection
- cursor recenter logic
- profile switching
- complementary or Kalman filtering
- motion smoothing profiles
- multiple dps profiles for different cursor feels

---

## 13. Summary

This layer owns:

- motion interpretation
- noise reduction
- dps-based tuning
- conversion to `dx` and `dy`

It does **not** own:

- IMU register access
- BLE transport
- button handling

Those belong to other layers.
