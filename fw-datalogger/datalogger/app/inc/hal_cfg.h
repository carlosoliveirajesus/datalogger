/**
 * @file hal_cfg.h
 * @brief Configuração da HAL para o Datalogger EPTA.
 *
 * Único arquivo que define "o que existe nesta placa": quais módulos da HAL
 * são compilados e os nomes lógicos de pinos, portas seriais e sensores.
 * As camadas hal/ e port/ incluem este arquivo; trocar de placa (Nucleo →
 * PCB) é trocar este arquivo e a pasta port/, sem tocar na app.
 */
#pragma once

/* ---------------------------------------------------------------- versão */
#define APP_VER_MAJOR 0
#define APP_VER_MINOR 1
#define APP_VER_BUILD 0

/* ------------------------------------------------- módulos habilitados  */
#define HAL_CPU_ENABLED  1
#define HAL_GPIO_ENABLED 1
#define HAL_TIME_ENABLED 1
#define HAL_SER_ENABLED  1
#define HAL_LED_ENABLED  1
#define HAL_IMU_ENABLED  1
#define HAL_ENV_ENABLED  1
#define HAL_SD_ENABLED   1

/* IWDG: habilitar só depois de configurar o IWDG no CubeMX. */
#define HAL_WDG_ENABLED  0

/* --------------------------------------------------------------- GPIOs */
/* A ordem aqui deve ser a mesma da tabela em port/stm32u5a5/gpio.c.      */
#define HAL_GPIO_PORT_NAMES                 \
    HAL_GPIO_LED_STATUS = 0, /* PC7  LD1 */ \
    HAL_GPIO_LED_ERROR,      /* PB7  LD2 */ \
    HAL_GPIO_BTN,            /* PC13 B1  */ \
    HAL_GPIO_IMU_CS,         /* PA4      */ \
    HAL_GPIO_IMU_INT1,       /* PB1      */ \
    HAL_GPIO_IMU_INT2,       /* PB2      */ \
    HAL_GPIO_SD_CD           /* PA8      */

/* Nível lógico de LED aceso e de cartão presente. */
#define HAL_LED_ON_LEVEL      true
#define HAL_SD_CD_PRESENT_LVL false /* chave do slot fecha para GND */

/* Número de LEDs controlados automaticamente por hal_led. */
#define HAL_LED_NUM_AUTO_LEDS 2

/* -------------------------------------------------------------- seriais */
#define HAL_SER_PORTS \
    HAL_SER_DEV_DBG = 0 /* USART1, VCP do ST-LINK */

#define HAL_SER_DBG_RX_BUF_SIZE 128

/* ------------------------------------------------------------- sensores */
#define HAL_IMU_DEVS \
    HAL_IMU_DEV_LSM6DSOX = 0 /* SPI1 ou I2C, ver HAL_IMU_BUS */

/* Barramento do LSM6DSOX:
 *   HAL_IMU_BUS_SPI  SPI1 (PA4 CS, PA5 SCK, PA6 MISO, PA7 MOSI). É o da PCB.
 *   HAL_IMU_BUS_I2C  módulo montado para I2C (só protótipo). O handle do
 *                    I2C é escolhido em port/stm32u5a5/imu.c (IMU_I2C_HANDLE).
 * INT1 (PB1) vale para os dois modos. */
#define HAL_IMU_BUS_SPI 0
#define HAL_IMU_BUS_I2C 1
#define HAL_IMU_BUS     HAL_IMU_BUS_SPI

#define HAL_ENV_DEVS \
    HAL_ENV_DEV_BME280 = 0 /* I2C3 */

/* --------------------------------------------------------- utilitários */
/* Módulos de debug (prefixo das mensagens no terminal). */
#define XMACRO_DBG_MODULES     \
    X(UTL_DBG_MOD_APP, 0)      \
    X(UTL_DBG_MOD_HAL, 1)      \
    X(UTL_DBG_MOD_PORT, 2)     \
    X(UTL_DBG_MOD_IMU, 3)      \
    X(UTL_DBG_MOD_ENV, 4)      \
    X(UTL_DBG_MOD_SD, 5)       \
    X(UTL_DBG_MOD_LOGGER, 6)
