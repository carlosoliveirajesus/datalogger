#include "hal.h"

#if HAL_ENV_ENABLED == 1

static hal_env_driver_t* drv = &HAL_ENV_DRIVER;

bool hal_env_init(hal_env_dev_t dev)
{
    return drv->init(dev);
}

void hal_env_deinit(hal_env_dev_t dev)
{
    drv->deinit(dev);
}

bool hal_env_setup(hal_env_dev_t dev, const hal_env_config_t* config)
{
    return drv->setup(dev, config);
}

bool hal_env_read(hal_env_dev_t dev, hal_env_data_t* data)
{
    return drv->read(dev, data);
}

#endif
