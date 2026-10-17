/**
 * @file hal_cpu.h
 * @brief Serviços do núcleo: causa de reset, seção crítica, watchdog, ID.
 */
#pragma once

#define HAL_CPU_ID_SIZE 12

typedef enum hal_cpu_reset_source_e
{
    HAL_CPU_RESET_OPTION_BYTE = (1 << 0),
    HAL_CPU_RESET_PIN         = (1 << 1),
    HAL_CPU_RESET_BOR         = (1 << 2),
    HAL_CPU_RESET_SOFTWARE    = (1 << 3),
    HAL_CPU_RESET_IWDG        = (1 << 4),
    HAL_CPU_RESET_WWDG        = (1 << 5),
    HAL_CPU_RESET_LOW_POWER   = (1 << 6),
} hal_cpu_reset_source_t;

typedef struct hal_cpu_driver_s
{
    void (*init)(void);
    uint32_t (*reset_source_get)(void);
    uint32_t (*critical_section_enter)(void);
    void (*critical_section_leave)(uint32_t state);
    void (*watchdog_refresh)(void);
    void (*reset)(void);
    void (*id_get)(uint8_t* id);
    void (*sleep_ms)(uint32_t tmr_ms);
} hal_cpu_driver_t;

void hal_cpu_init(void);
/** Máscara de hal_cpu_reset_source_t, capturada em hal_cpu_init(). */
uint32_t hal_cpu_reset_source_get(void);
/** Desliga as interrupções e retorna o estado anterior. */
uint32_t hal_cpu_critical_section_enter(void);
void hal_cpu_critical_section_leave(uint32_t state);
void hal_cpu_watchdog_refresh(void);
void hal_cpu_reset(void);
/** Copia o ID único do MCU (HAL_CPU_ID_SIZE bytes). */
void hal_cpu_id_get(uint8_t* id);
/** Espera ativa (bloqueante), alimentando o watchdog. */
void hal_cpu_sleep_ms(uint32_t tmr_ms);
