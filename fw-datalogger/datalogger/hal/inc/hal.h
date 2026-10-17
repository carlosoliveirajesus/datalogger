/**
 * @file hal.h
 * @brief Ponto de entrada da HAL do Datalogger: inclua só este arquivo.
 *
 * Arquitetura em camadas (mesmo padrão do fw-slamming):
 *
 *   app/   regra de negócio (máquina de estados, logger). Só chama hal_*().
 *   hal/   interfaces genéricas. Cada módulo tem um header com os tipos e um
 *          struct hal_xxx_driver_t de ponteiros de função; o .c só repassa
 *          as chamadas para o driver concreto (drv->func()).
 *   port/  implementação para um MCU/placa (aqui STM32U5A5 sobre a HAL da ST).
 *          Cada port/xxx.c define a instância HAL_XXX_DRIVER e, quando há
 *          vários chips do mesmo tipo, uma tabela port_xxx_drv[dev].
 *   utl/   utilitários sem hardware (printf, buffer circular, debug).
 *
 * O que existe na placa é definido em app/inc/hal_cfg.h (HAL_XXX_ENABLED,
 * nomes de pinos, portas e sensores).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#ifndef __UNUSED
#define __UNUSED __attribute__((unused))
#endif

#include "hal_cfg.h"
#include "utl.h"

#if HAL_CPU_ENABLED == 1
#include "hal_cpu.h"
extern hal_cpu_driver_t HAL_CPU_DRIVER;
#endif

#if HAL_GPIO_ENABLED == 1
#include "hal_gpio.h"
extern hal_gpio_driver_t HAL_GPIO_DRIVER;
#endif

#if HAL_TIME_ENABLED == 1
#include "hal_time.h"
extern hal_time_driver_t HAL_TIME_DRIVER;
#endif

#if HAL_SER_ENABLED == 1
#include "hal_ser.h"
extern hal_ser_driver_t HAL_SER_DRIVER;
#endif

#if HAL_LED_ENABLED == 1
#include "hal_led.h"
#endif

#if HAL_IMU_ENABLED == 1
#include "hal_imu.h"
extern hal_imu_driver_t HAL_IMU_DRIVER;
#endif

#if HAL_ENV_ENABLED == 1
#include "hal_env.h"
extern hal_env_driver_t HAL_ENV_DRIVER;
#endif

#if HAL_SD_ENABLED == 1
#include "hal_sd.h"
extern hal_sd_driver_t HAL_SD_DRIVER;
#endif

/** Inicializa a HAL e chama app_init(). Chamado uma vez no main.c. */
void hal_init(void);
/** Chamado no laço infinito do main.c; chama app_loop(). */
void hal_loop(void);
bool hal_is_running(void);

/* Implementados pela aplicação (há versões fracas vazias em hal.c). */
void app_dbg_init(void);
void app_init(void);
void app_loop(void);
