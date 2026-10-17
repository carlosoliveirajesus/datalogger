#include "hal.h"

#if HAL_SD_ENABLED == 1

static hal_sd_driver_t* drv = &HAL_SD_DRIVER;

bool hal_sd_is_present(void)
{
    return drv->is_present();
}

hal_sd_status_t hal_sd_init(void)
{
    if(!drv->is_present())
        return HAL_SD_ERR_NO_CARD;

    return drv->init();
}

void hal_sd_deinit(void)
{
    drv->deinit();
}

bool hal_sd_is_ready(void)
{
    return drv->is_ready();
}

hal_sd_status_t hal_sd_read(uint8_t* buf, uint32_t block, uint32_t count)
{
    if(buf == NULL || count == 0)
        return HAL_SD_ERR_PARAM;

    return drv->read(buf, block, count);
}

hal_sd_status_t hal_sd_write(const uint8_t* buf, uint32_t block, uint32_t count)
{
    if(buf == NULL || count == 0)
        return HAL_SD_ERR_PARAM;

    return drv->write(buf, block, count);
}

hal_sd_status_t hal_sd_info_get(hal_sd_info_t* info)
{
    if(info == NULL)
        return HAL_SD_ERR_PARAM;

    return drv->info_get(info);
}

#endif
