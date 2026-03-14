#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "icm42688.h"   // for imu_sample_t

// ---------------------------------------------------------------------------
// Return Status
// ---------------------------------------------------------------------------
typedef enum {
    MOTION_OK = 0,
    MOTION_ERR_INVALID_ARG,
    MOTION_ERR_NOT_INIT,
    MOTION_ERR_BAD_CONFIG,
    MOTION_ERR_INTERNAL
} motion_status_t;

// ---------------------------------------------------------------------------
// Motion Axes
// ---------------------------------------------------------------------------
typedef enum {
    MOTION_AXIS_NONE = 0,
    MOTION_AXIS_GYRO_X,
    MOTION_AXIS_GYRO_Y,
    MOTION_AXIS_GYRO_Z,
    MOTION_AXIS_ACCEL_X,
    MOTION_AXIS_ACCEL_Y,
    MOTION_AXIS_ACCEL_Z
} motion_axis_t;

// ---------------------------------------------------------------------------
// Cursor Output
// ---------------------------------------------------------------------------
typedef struct {
    int16_t dx;
    int16_t dy;
    bool    valid;
} cursor_delta_t;

// ---------------------------------------------------------------------------
// Filter State
// ---------------------------------------------------------------------------
typedef struct {
    float filtered_x;
    float filtered_y;

    float last_pitch_deg;
    float last_yaw_deg;

    uint32_t last_timestamp_ms;
} motion_state_t;

// ---------------------------------------------------------------------------
// Adjustable DPS Profile
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Mapper Configuration
// ---------------------------------------------------------------------------
typedef struct {
    motion_axis_t x_axis_source;
    motion_axis_t y_axis_source;

    bool  invert_x;
    bool  invert_y;

    float smoothing_alpha;

    int16_t max_delta_x;
    int16_t max_delta_y;

    bool ignore_roll;
    bool use_rate_mode;
    bool enable_pitch_stabilization;

    motion_dps_profile_t dps_profile;
} motion_mapper_config_t;

// ---------------------------------------------------------------------------
// Mapper Object
// ---------------------------------------------------------------------------
typedef struct {
    bool                  initialized;
    motion_mapper_config_t config;
    motion_state_t         state;
} motion_mapper_t;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Lifecycle
motion_status_t motion_mapper_init(motion_mapper_t *mapper,
                                   const motion_mapper_config_t *cfg);
motion_status_t motion_mapper_deinit(motion_mapper_t *mapper);
motion_status_t motion_mapper_reset(motion_mapper_t *mapper);

// Configuration
motion_status_t motion_mapper_set_config(motion_mapper_t *mapper,
                                         const motion_mapper_config_t *cfg);
motion_status_t motion_mapper_get_config(const motion_mapper_t *mapper,
                                         motion_mapper_config_t *cfg);
motion_status_t motion_mapper_set_dps_profile(motion_mapper_t *mapper,
                                              const motion_dps_profile_t *profile);
motion_status_t motion_mapper_get_dps_profile(const motion_mapper_t *mapper,
                                              motion_dps_profile_t *profile);

// Processing
motion_status_t motion_mapper_process_sample(motion_mapper_t *mapper,
                                             const imu_sample_t *sample,
                                             cursor_delta_t *delta);

// Optional helpers
motion_status_t motion_mapper_set_invert(motion_mapper_t *mapper,
                                         bool invert_x, bool invert_y);
motion_status_t motion_mapper_set_max_delta(motion_mapper_t *mapper,
                                            int16_t max_dx, int16_t max_dy);

// State query
bool motion_mapper_is_initialized(const motion_mapper_t *mapper);
