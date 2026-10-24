/**
 * @file hal_gpio.h
 * @brief GPIO por nome lógico (HAL_GPIO_PORT_NAMES em hal_cfg.h).
 */
#pragma once

typedef void (*hal_gpio_cbk_t)(void);

typedef enum hal_gpio_pin_e
{
    HAL_GPIO_PORT_NAMES,
    HAL_GPIO_NUM_PINS,
} hal_gpio_pin_t;

typedef enum hal_gpio_mode_e
{
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT_PP,
    HAL_GPIO_MODE_OUTPUT_OD,
    HAL_GPIO_MODE_ANALOG,
    HAL_GPIO_MODE_IT_RISING,
    HAL_GPIO_MODE_IT_FALLING,
    HAL_GPIO_MODE_IT_RISING_FALLING,
} hal_gpio_mode_t;

typedef enum hal_gpio_pull_e
{
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN,
} hal_gpio_pull_t;

typedef enum hal_gpio_speed_e
{
    HAL_GPIO_SPEED_LOW = 0,
    HAL_GPIO_SPEED_MEDIUM,
    HAL_GPIO_SPEED_FAST,
    HAL_GPIO_SPEED_HIGH,
} hal_gpio_speed_t;

typedef enum hal_gpio_interrupt_state_e
{
    HAL_GPIO_INTERRUPT_DISABLE = 0,
    HAL_GPIO_INTERRUPT_ENABLE,
} hal_gpio_interrupt_state_t;

typedef struct hal_gpio_driver_s
{
    void (*configure)(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_speed_t speed, hal_gpio_pull_t pull);
    void (*interrupt_cbk_set)(hal_gpio_pin_t pin, hal_gpio_cbk_t cbk);
    void (*interrupt_set)(hal_gpio_pin_t pin, hal_gpio_interrupt_state_t state);
    void (*set)(hal_gpio_pin_t pin, bool state);
    bool (*get)(hal_gpio_pin_t pin);
    void (*toggle)(hal_gpio_pin_t pin);
    void (*interrupt_trigger)(hal_gpio_pin_t pin);
} hal_gpio_driver_t;

/**
 * Reconfigura um pino. Na Nucleo os pinos já saem configurados pelo CubeMX
 * (MX_GPIO_Init); use esta função só para mudar o modo em tempo de execução.
 */
void hal_gpio_configure(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_speed_t speed, hal_gpio_pull_t pull);
/** Registra a função chamada (na ISR) quando o pino gerar interrupção. */
void hal_gpio_interrupt_cbk_set(hal_gpio_pin_t pin, hal_gpio_cbk_t cbk);
void hal_gpio_interrupt_set(hal_gpio_pin_t pin, hal_gpio_interrupt_state_t state);
void hal_gpio_set(hal_gpio_pin_t pin, bool state);
bool hal_gpio_get(hal_gpio_pin_t pin);
void hal_gpio_toggle(hal_gpio_pin_t pin);
/** Gera por software a interrupção do pino, como se a borda tivesse chegado. */
void hal_gpio_interrupt_trigger(hal_gpio_pin_t pin);
