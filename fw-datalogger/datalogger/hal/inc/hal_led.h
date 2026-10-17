/**
 * @file hal_led.h
 * @brief Padrões automáticos de LED (pisca-pisca) sobre hal_gpio.
 *
 * Módulo só de HAL (não tem port): roda no callback de 100 ms de hal_time.
 */
#pragma once

typedef enum hal_led_mode_e
{
    HAL_LED_MODE_OFF = 0,
    HAL_LED_MODE_ON,
    HAL_LED_MODE_BLINK_ONCE,     /**< um pulso curto e apaga           */
    HAL_LED_MODE_BLINK_FAST,     /**< 5 Hz contínuo                     */
    HAL_LED_MODE_BLINK_SLOW_1X,  /**< 1 pulso a cada 2 s (heartbeat)    */
    HAL_LED_MODE_BLINK_FAST_2X,  /**< 2 pulsos a cada 2 s               */
    HAL_LED_MODE_BLINK_FAST_3X,  /**< 3 pulsos a cada 2 s               */
    HAL_LED_NUM_MODES,
} hal_led_mode_t;

void hal_led_init(void);
/** Associa um padrão a um pino. Até HAL_LED_NUM_AUTO_LEDS pinos. */
void hal_led_mode_set(hal_gpio_pin_t pin, hal_led_mode_t mode);
hal_led_mode_t hal_led_mode_get(hal_gpio_pin_t pin);
