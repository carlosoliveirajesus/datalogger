/**
 * @file hal_time.h
 * @brief Base de tempo em ms e callbacks periódicos.
 *
 * Os callbacks rodam dentro da interrupção do SysTick: devem ser curtos e
 * normalmente só levantam um flag para o loop principal.
 */
#pragma once

typedef enum hal_time_cbk_type_e
{
    HAL_TIME_100MS_CBK = 0,
    HAL_TIME_1S_CBK,
    HAL_TIME_NUM_CBKS,
} hal_time_cbk_type_t;

typedef void (*hal_time_cbk_t)(void);

typedef struct hal_time_driver_s
{
    void (*init)(void);
    uint32_t (*get_ms)(void);
    uint64_t (*get_us)(void);
    void (*cbk_register)(hal_time_cbk_t cbk, hal_time_cbk_type_t type);
} hal_time_driver_t;

void hal_time_init(void);
uint32_t hal_time_get_ms(void);
/** Tempo desde o boot em µs (não estoura na prática). Pode ser chamado de
    ISRs com prioridade menor que a do SysTick. */
uint64_t hal_time_get_us(void);
/** Tempo decorrido entre dois instantes, correto mesmo no estouro de 32 bits. */
uint32_t hal_time_elapsed_ms(uint32_t tmr_old_ms, uint32_t tmr_new_ms);
/** Tempo decorrido desde @p tmr_old_ms até agora. */
uint32_t hal_time_elapsed_since_ms(uint32_t tmr_old_ms);
void hal_time_cbk_register(hal_time_cbk_t cbk, hal_time_cbk_type_t type);
