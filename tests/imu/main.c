// SPDX-License-Identifier: MIT
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pbio/imu.h>

#include <pbdrv/clock.h>
#include <pbdrv/imu.h>
#include <pbio/dcmotor.h>
#include <pbsys/storage.h>

// Synthetic device, motor and storage callbacks for the real PBIO implementation.
struct _pbdrv_imu_dev_t { int unused;
};
static struct _pbdrv_imu_dev_t device;
static pbdrv_imu_config_t config = { .gyro_scale = 0.1f, .accel_scale = 1, .sample_time = 0.001f };
static pbdrv_imu_handle_frame_data_func_t frame;
static pbdrv_imu_handle_stationary_data_func_t window;
static bool still, coasting, unavailable;
static uint32_t clock_ms;
static unsigned int writes;

uint32_t pbdrv_clock_get_ms(void) {
    return clock_ms;
}
bool pbio_dcmotor_all_coasting(void) {
    return coasting;
}
void pbsys_storage_request_write(void) {
    writes++;
}

pbio_error_t pbdrv_imu_get_imu(pbdrv_imu_dev_t **dev, pbdrv_imu_config_t **cfg) {
    if (unavailable) {
        return PBIO_ERROR_NO_DEV;
    }
    *dev = &device;
    *cfg = &config;
    return PBIO_SUCCESS;
}

bool pbdrv_imu_is_stationary(pbdrv_imu_dev_t *dev) {
    assert(dev == &device);
    return still;
}

void pbdrv_imu_set_data_handlers(pbdrv_imu_dev_t *dev, pbdrv_imu_handle_frame_data_func_t f,
    pbdrv_imu_handle_stationary_data_func_t w) {
    assert(dev == &device);
    frame = f;
    window = w;
}


int main(void) {
    pbio_imu_persistent_settings_t settings;
    pbio_imu_init();
    pbio_imu_set_default_settings(&settings);
    pbio_imu_apply_loaded_settings(&settings);
    int16_t sample[] = {0, 0, 0, 0, 0, 9807};
    frame(sample);
    pbio_geometry_xyz_t before, after;
    pbio_imu_get_acceleration(&before, false);
    assert(pbio_imu_get_up_side(true) == PBIO_GEOMETRY_SIDE_TOP);
    pbio_imu_get_acceleration(&after, false);
    assert(!memcmp(&before, &after, sizeof(before)));

    // One 90 deg/s sample advances heading by 0.09 degrees immediately.
    sample[2] = 900;
    frame(sample);
    assert(fabsf(pbio_imu_get_heading(PBIO_IMU_HEADING_TYPE_3D) + 0.09f) < 0.0001f);

    pbio_imu_persistent_settings_t original = settings;
    pbio_imu_persistent_settings_t patch = {
        .flags = PBIO_IMU_SETTINGS_FLAGS_GYRO_SCALE_SET | PBIO_IMU_SETTINGS_FLAGS_GYRO_STATIONARY_THRESHOLD_SET,
        .gyro_stationary_threshold = 1,
        .angular_velocity_scale = {.x = 350, .y = 0, .z = 360},
    };
    assert(pbio_imu_set_settings(&patch) == PBIO_ERROR_INVALID_ARG);
    assert(!memcmp(&settings, &original, sizeof(settings)));
    assert(config.gyro_stationary_threshold == 20 && writes == 0);
    const float invalid[] = {NAN, INFINITY, -INFINITY};
    for (unsigned int i = 0; i < 3; i++) {
        patch = settings;
        patch.flags = PBIO_IMU_SETTINGS_FLAGS_ACCEL_CALIBRATED;
        patch.gravity_pos.x = invalid[i];
        assert(pbio_imu_set_settings(&patch) == PBIO_ERROR_INVALID_ARG);
        patch.flags = PBIO_IMU_SETTINGS_FLAGS_GYRO_STATIONARY_THRESHOLD_SET;
        patch.gyro_stationary_threshold = invalid[i];
        assert(pbio_imu_set_settings(&patch) == PBIO_ERROR_INVALID_ARG);
        patch.flags = PBIO_IMU_SETTINGS_FLAGS_GYRO_BIAS_INITIAL_SET;
        patch.angular_velocity_bias_start.z = invalid[i];
        assert(pbio_imu_set_settings(&patch) == PBIO_ERROR_INVALID_ARG);
        assert(!memcmp(&settings, &original, sizeof(settings)));
        assert(config.gyro_stationary_threshold == 20 && writes == 0);
        pbio_geometry_xyz_t vector = {.x = invalid[i]}, out = before;
        assert(pbio_geometry_vector_normalize(&vector, &out) == PBIO_ERROR_INVALID_ARG);
        assert(!memcmp(&out, &before, sizeof(out)));
    }
    patch = (pbio_imu_persistent_settings_t) {
        .flags = PBIO_IMU_SETTINGS_FLAGS_GYRO_STATIONARY_THRESHOLD_SET,
        .gyro_stationary_threshold = -1,
    };
    assert(pbio_imu_set_settings(&patch) == PBIO_ERROR_INVALID_ARG);
    assert(!memcmp(&settings, &original, sizeof(settings)));
    patch.gyro_stationary_threshold = 1e30f;
    assert(pbio_imu_set_settings(&patch) == PBIO_SUCCESS);
    assert(config.gyro_stationary_threshold == INT16_MAX && writes == 1);
    puts("PBIO IMU regression checks passed");
    return 0;
}
