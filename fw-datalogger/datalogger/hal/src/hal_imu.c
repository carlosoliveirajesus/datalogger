#include "hal.h"

#if HAL_IMU_ENABLED == 1

static hal_imu_driver_t* drv = &HAL_IMU_DRIVER;

bool hal_imu_init(hal_imu_dev_t dev)
{
    return drv->init(dev);
}

void hal_imu_deinit(hal_imu_dev_t dev)
{
    drv->deinit(dev);
}

bool hal_imu_setup(hal_imu_dev_t dev, const hal_imu_config_t* config)
{
    return drv->setup(dev, config);
}

bool hal_imu_raw_read(hal_imu_dev_t dev, hal_imu_raw_t* accel, hal_imu_raw_t* gyro)
{
    return drv->raw_read(dev, accel, gyro);
}

bool hal_imu_real_read(hal_imu_dev_t dev, hal_imu_real_t* accel, hal_imu_real_t* gyro)
{
    hal_imu_raw_t a;
    hal_imu_raw_t g;

    if(!hal_imu_raw_read(dev, &a, &g))
        return false;

    float sa = hal_imu_accel_scale_get(dev);
    float sg = hal_imu_gyro_scale_get(dev);

    accel->x = a.x * sa;
    accel->y = a.y * sa;
    accel->z = a.z * sa;

    gyro->x = g.x * sg;
    gyro->y = g.y * sg;
    gyro->z = g.z * sg;

    return true;
}

float hal_imu_accel_scale_get(hal_imu_dev_t dev)
{
    return drv->accel_scale_get(dev);
}

float hal_imu_gyro_scale_get(hal_imu_dev_t dev)
{
    return drv->gyro_scale_get(dev);
}

#endif
