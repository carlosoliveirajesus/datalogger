#include "hal.h"

static volatile bool running = false;

__WEAK void app_dbg_init(void) {}
__WEAK void app_init(void) {}
__WEAK void app_loop(void) {}

bool hal_is_running(void)
{
    return running;
}

#if HAL_CPU_ENABLED == 1
static void hal_reset_cause_print(void)
{
    static const struct
    {
        uint32_t mask;
        const char* name;
    } causes[] = {
        { HAL_CPU_RESET_OPTION_BYTE, "OPTION_BYTE" },
        { HAL_CPU_RESET_PIN,         "PIN"         },
        { HAL_CPU_RESET_BOR,         "BOR"         },
        { HAL_CPU_RESET_SOFTWARE,    "SOFTWARE"    },
        { HAL_CPU_RESET_IWDG,        "IWDG"        },
        { HAL_CPU_RESET_WWDG,        "WWDG"        },
        { HAL_CPU_RESET_LOW_POWER,   "LOW_POWER"   },
    };

    uint32_t src = hal_cpu_reset_source_get();

    for(size_t n = 0; n < UTL_ARRAY_SIZE(causes); n++)
    {
        if(src & causes[n].mask)
            UTL_DBG_PRINTF(UTL_DBG_MOD_HAL, "Reset cause: %s\r\n", causes[n].name);
    }
}
#endif

void hal_init(void)
{
    /* A serial vem primeiro para que todas as mensagens seguintes apareçam. */
#if HAL_SER_ENABLED == 1
    hal_ser_init();
#endif

    utl_dbg_init();
    app_dbg_init();

#if HAL_CPU_ENABLED == 1
    hal_cpu_init();
#endif

#if HAL_TIME_ENABLED == 1
    hal_time_init();
#endif

#if HAL_LED_ENABLED == 1
    hal_led_init();
#endif

#if HAL_CPU_ENABLED == 1
    hal_reset_cause_print();
#endif

    /* Sensores e cartão são inicializados pela app, que decide o que fazer
       em caso de falha (hal_imu_init, hal_env_init, hal_sd_init). */
    app_init();
    running = true;
}

void hal_loop(void)
{
    app_loop();
}
