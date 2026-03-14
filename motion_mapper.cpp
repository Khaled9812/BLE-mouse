#include "motion_mapper.h"
#include <math.h>
#include <string.h>
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float axis_value(const imu_sample_t *sample, motion_axis_t axis) {
    switch (axis) {
        case MOTION_AXIS_GYRO_X:  return sample->gyro_dps_x;
        case MOTION_AXIS_GYRO_Y:  return sample->gyro_dps_y;
        case MOTION_AXIS_GYRO_Z:  return sample->gyro_dps_z;
        case MOTION_AXIS_ACCEL_X: return sample->accel_g_x;
        case MOTION_AXIS_ACCEL_Y: return sample->accel_g_y;
        case MOTION_AXIS_ACCEL_Z: return sample->accel_g_z;
        default:                  return 0.0f;
    }
}

// ---------------------------------------------------------------------------
// motion_mapper_init
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_init(motion_mapper_t *mapper,
                                   const motion_mapper_config_t *cfg) {
    if (!mapper || !cfg) return MOTION_ERR_INVALID_ARG;

    mapper->config = *cfg;
    memset(&mapper->state, 0, sizeof(mapper->state));
    mapper->initialized = true;

    Serial.println("[MOTION] init OK");
    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// motion_mapper_deinit
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_deinit(motion_mapper_t *mapper) {
    if (!mapper) return MOTION_ERR_INVALID_ARG;
    mapper->initialized = false;
    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// motion_mapper_reset — zero filter state, keep config
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_reset(motion_mapper_t *mapper) {
    if (!mapper) return MOTION_ERR_INVALID_ARG;
    if (!mapper->initialized) return MOTION_ERR_NOT_INIT;
    memset(&mapper->state, 0, sizeof(mapper->state));
    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_set_config(motion_mapper_t *mapper,
                                         const motion_mapper_config_t *cfg) {
    if (!mapper || !cfg) return MOTION_ERR_INVALID_ARG;
    mapper->config = *cfg;
    return MOTION_OK;
}

motion_status_t motion_mapper_get_config(const motion_mapper_t *mapper,
                                         motion_mapper_config_t *cfg) {
    if (!mapper || !cfg) return MOTION_ERR_INVALID_ARG;
    *cfg = mapper->config;
    return MOTION_OK;
}

motion_status_t motion_mapper_set_dps_profile(motion_mapper_t *mapper,
                                              const motion_dps_profile_t *profile) {
    if (!mapper || !profile) return MOTION_ERR_INVALID_ARG;
    mapper->config.dps_profile = *profile;
    return MOTION_OK;
}

motion_status_t motion_mapper_get_dps_profile(const motion_mapper_t *mapper,
                                              motion_dps_profile_t *profile) {
    if (!mapper || !profile) return MOTION_ERR_INVALID_ARG;
    *profile = mapper->config.dps_profile;
    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// motion_mapper_process_sample
// Pipeline: extract → dead-zone → invert → smooth (IIR) → two-zone gain
//           → clamp → cast to int16
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_process_sample(motion_mapper_t *mapper,
                                             const imu_sample_t *sample,
                                             cursor_delta_t *delta) {
    if (!mapper || !sample || !delta) return MOTION_ERR_INVALID_ARG;
    if (!mapper->initialized) return MOTION_ERR_NOT_INIT;

    const motion_mapper_config_t *cfg = &mapper->config;
    const motion_dps_profile_t   *p   = &cfg->dps_profile;
    motion_state_t               *st  = &mapper->state;

    // 1. Extract raw axis values
    float raw_x = axis_value(sample, cfg->x_axis_source);
    float raw_y = axis_value(sample, cfg->y_axis_source);

    // 2. Dead-zone: values below threshold feed 0 into IIR (smooth stop)
    if (fabsf(raw_x) < p->deadzone_dps_x) raw_x = 0.0f;
    if (fabsf(raw_y) < p->deadzone_dps_y) raw_y = 0.0f;

    // 3. Invert before smoothing so filter state is in display-sign convention
    if (cfg->invert_x) raw_x = -raw_x;
    if (cfg->invert_y) raw_y = -raw_y;

    // 4. IIR low-pass: filtered = alpha * prev + (1-alpha) * current
    float alpha = cfg->smoothing_alpha;
    st->filtered_x = alpha * st->filtered_x + (1.0f - alpha) * raw_x;
    st->filtered_y = alpha * st->filtered_y + (1.0f - alpha) * raw_y;

    // 5. Update timestamp
    st->last_timestamp_ms = sample->timestamp_ms;

    // 6. Two-zone gain then pixels_per_dps conversion
    float gain_x = (fabsf(st->filtered_x) < p->response_start_dps_x)
                    ? p->slow_zone_gain_x : p->fast_zone_gain_x;
    float gain_y = (fabsf(st->filtered_y) < p->response_start_dps_y)
                    ? p->slow_zone_gain_y : p->fast_zone_gain_y;

    float fx = st->filtered_x * gain_x * p->pixels_per_dps_x;
    float fy = st->filtered_y * gain_y * p->pixels_per_dps_y;

    // 7. Clamp to max_delta
    fx = clampf(fx, -(float)cfg->max_delta_x, (float)cfg->max_delta_x);
    fy = clampf(fy, -(float)cfg->max_delta_y, (float)cfg->max_delta_y);

    // 8. Cast to int16
    delta->dx    = (int16_t)fx;
    delta->dy    = (int16_t)fy;
    delta->valid = (delta->dx != 0 || delta->dy != 0);

    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// Optional helpers
// ---------------------------------------------------------------------------
motion_status_t motion_mapper_set_invert(motion_mapper_t *mapper,
                                         bool invert_x, bool invert_y) {
    if (!mapper) return MOTION_ERR_INVALID_ARG;
    mapper->config.invert_x = invert_x;
    mapper->config.invert_y = invert_y;
    return MOTION_OK;
}

motion_status_t motion_mapper_set_max_delta(motion_mapper_t *mapper,
                                            int16_t max_dx, int16_t max_dy) {
    if (!mapper) return MOTION_ERR_INVALID_ARG;
    mapper->config.max_delta_x = max_dx;
    mapper->config.max_delta_y = max_dy;
    return MOTION_OK;
}

// ---------------------------------------------------------------------------
// State query
// ---------------------------------------------------------------------------
bool motion_mapper_is_initialized(const motion_mapper_t *mapper) {
    return mapper && mapper->initialized;
}
