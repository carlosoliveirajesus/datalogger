/**
 * @file cpu.c
 * @brief Port de hal_cpu para o STM32U5A5 (HAL da ST).
 */
#include "main.h"
#include "hal.h"

#if HAL_WDG_ENABLED == 1
#include "iwdg.h" /* gerado pelo CubeMX quando o IWDG for habilitado */
#endif

static uint32_t reset_source = 0;

static void port_cpu_reset_source_capture(void)
{
    static const struct
    {
        uint32_t flag;
        uint32_t hal;
    } map[] = {
        { RCC_FLAG_OBLRST,  HAL_CPU_RESET_OPTION_BYTE },
        { RCC_FLAG_PINRST,  HAL_CPU_RESET_PIN         },
        { RCC_FLAG_BORRST,  HAL_CPU_RESET_BOR         },
        { RCC_FLAG_SFTRST,  HAL_CPU_RESET_SOFTWARE    },
        { RCC_FLAG_IWDGRST, HAL_CPU_RESET_IWDG        },
        { RCC_FLAG_WWDGRST, HAL_CPU_RESET_WWDG        },
        { RCC_FLAG_LPWRRST, HAL_CPU_RESET_LOW_POWER   },
    };

    reset_source = 0;

    for(size_t n = 0; n < UTL_ARRAY_SIZE(map); n++)
    {
        if(__HAL_RCC_GET_FLAG(map[n].flag))
            reset_source |= map[n].hal;
    }

    /* Limpa para que o próximo boot mostre só a causa nova. */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/* Prioridades de interrupção (0 = mais alta). O CubeMX deixa o SysTick em 15
   e os EXTI em 0; aqui isso é invertido de propósito:
   - SysTick 0: o HAL_GetTick() precisa andar dentro da ISR do INT1, que lê a
     IMU por I2C com timeout, e hal_time_get_us() depende disso;
   - INT1 da IMU 5: lê a amostra assim que ela fica pronta, mesmo com o loop
     ocupado gravando no cartão;
   - botão 6. */
static void port_cpu_irq_priorities_set(void)
{
    uwTickPrio = 0U;
    HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
    HAL_NVIC_SetPriority(IMU_INT1_EXTI_IRQn, 5U, 0U);
    HAL_NVIC_SetPriority(BTN_EXTI_IRQn, 6U, 0U);
}

static void port_cpu_init(void)
{
    port_cpu_reset_source_capture();
    port_cpu_irq_priorities_set();
}

static uint32_t port_cpu_reset_source_get(void)
{
    return reset_source;
}

static uint32_t port_cpu_critical_section_enter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void port_cpu_critical_section_leave(uint32_t state)
{
    __set_PRIMASK(state);
}

static void port_cpu_watchdog_refresh(void)
{
#if HAL_WDG_ENABLED == 1
    HAL_IWDG_Refresh(&hiwdg);
#endif
}

static void port_cpu_reset(void)
{
    NVIC_SystemReset();
}

static void port_cpu_id_get(uint8_t* id)
{
    uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    memcpy(id, uid, HAL_CPU_ID_SIZE);
}

static void port_cpu_sleep_ms(uint32_t tmr_ms)
{
    uint32_t start = HAL_GetTick();

    while((HAL_GetTick() - start) < tmr_ms)
        port_cpu_watchdog_refresh();
}

hal_cpu_driver_t HAL_CPU_DRIVER = {
    .init = port_cpu_init,
    .reset_source_get = port_cpu_reset_source_get,
    .critical_section_enter = port_cpu_critical_section_enter,
    .critical_section_leave = port_cpu_critical_section_leave,
    .watchdog_refresh = port_cpu_watchdog_refresh,
    .reset = port_cpu_reset,
    .id_get = port_cpu_id_get,
    .sleep_ms = port_cpu_sleep_ms,
};
