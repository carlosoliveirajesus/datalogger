#include "hal.h"

#if HAL_TIME_ENABLED == 1

static hal_time_driver_t* drv = &HAL_TIME_DRIVER;

void hal_time_init(void)
{
    drv->init();
}

uint32_t hal_time_get_ms(void)
{
    return drv->get_ms();
}

uint32_t hal_time_elapsed_ms(uint32_t tmr_old_ms, uint32_t tmr_new_ms)
{
    /* Aritmética sem sinal já trata o estouro do contador. */
    return tmr_new_ms - tmr_old_ms;
}

uint32_t hal_time_elapsed_since_ms(uint32_t tmr_old_ms)
{
    return hal_time_elapsed_ms(tmr_old_ms, hal_time_get_ms());
}

void hal_time_cbk_register(hal_time_cbk_t cbk, hal_time_cbk_type_t type)
{
    if(type < HAL_TIME_NUM_CBKS)
        drv->cbk_register(cbk, type);
}

#endif
