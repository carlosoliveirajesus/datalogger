#include "hal.h"

#if HAL_CPU_ENABLED == 1

static hal_cpu_driver_t* drv = &HAL_CPU_DRIVER;

void hal_cpu_init(void)
{
    drv->init();
}

uint32_t hal_cpu_reset_source_get(void)
{
    return drv->reset_source_get();
}

uint32_t hal_cpu_critical_section_enter(void)
{
    return drv->critical_section_enter();
}

void hal_cpu_critical_section_leave(uint32_t state)
{
    drv->critical_section_leave(state);
}

void hal_cpu_watchdog_refresh(void)
{
    drv->watchdog_refresh();
}

void hal_cpu_reset(void)
{
    drv->reset();
}

void hal_cpu_id_get(uint8_t* id)
{
    drv->id_get(id);
}

void hal_cpu_sleep_ms(uint32_t tmr_ms)
{
    drv->sleep_ms(tmr_ms);
}

#endif
