/**
 * @file time.c
 * @brief Port de hal_time para o STM32U5A5.
 *
 * Usa o SysTick de 1 ms que a HAL da ST já configura. HAL_IncTick() é fraca
 * na HAL; aqui ela é sobrescrita para também gerar os callbacks de 100 ms e
 * 1 s, sem precisar de um timer extra nem mexer no código gerado.
 */
#include "main.h"
#include "hal.h"

static hal_time_cbk_t time_cbks[HAL_TIME_NUM_CBKS] = { 0 };
static volatile uint32_t ms_100 = 0;
static volatile uint32_t ticks_100ms = 0;

/* Chamada pelo SysTick_Handler (stm32u5xx_it.c) a cada 1 ms. */
void HAL_IncTick(void)
{
    uwTick += (uint32_t) uwTickFreq;
    ms_100 += (uint32_t) uwTickFreq;

    if(ms_100 >= 100U)
    {
        ms_100 = 0;
        ticks_100ms++;

        if(time_cbks[HAL_TIME_100MS_CBK])
            time_cbks[HAL_TIME_100MS_CBK]();

        if((ticks_100ms % 10U) == 0U && time_cbks[HAL_TIME_1S_CBK])
            time_cbks[HAL_TIME_1S_CBK]();
    }
}

static void port_time_init(void)
{
    /* SysTick já foi iniciado por HAL_Init(). */
}

static uint32_t port_time_get_ms(void)
{
    return HAL_GetTick();
}

/* ms do uwTick + a fração já contada pelo SysTick (que conta para baixo).
   Se o SysTick estourar entre as duas leituras, o uwTick muda e a leitura é
   refeita; por isso o SysTick precisa ter prioridade maior que a de quem
   chama (ver port_cpu_init). */
static uint64_t port_time_get_us(void)
{
    uint32_t ms;
    uint32_t val;
    uint32_t load = SysTick->LOAD;

    do
    {
        ms = uwTick;
        val = SysTick->VAL;
    } while(ms != uwTick);

    return (uint64_t) ms * 1000U + ((uint64_t) (load - val) * 1000U) / ((uint64_t) load + 1U);
}

static void port_time_cbk_register(hal_time_cbk_t cbk, hal_time_cbk_type_t type)
{
    time_cbks[type] = cbk;
}

hal_time_driver_t HAL_TIME_DRIVER = {
    .init = port_time_init,
    .get_ms = port_time_get_ms,
    .get_us = port_time_get_us,
    .cbk_register = port_time_cbk_register,
};
