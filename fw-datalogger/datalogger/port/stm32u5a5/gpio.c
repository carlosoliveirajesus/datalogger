/**
 * @file gpio.c
 * @brief Port de hal_gpio para o STM32U5A5 (HAL da ST).
 *
 * A configuração inicial dos pinos é feita pelo CubeMX (MX_GPIO_Init) e os
 * vetores EXTIx_IRQHandler ficam no stm32u5xx_it.c gerado, que chama
 * HAL_GPIO_EXTI_IRQHandler(). Este arquivo sobrescreve os callbacks fracos
 * HAL_GPIO_EXTI_Rising/Falling_Callback e despacha para o callback de cada
 * pino lógico.
 */
#include "main.h"
#include "hal.h"

typedef struct port_gpio_s
{
    GPIO_TypeDef* port;
    uint16_t pin;
} port_gpio_t;

/* Mesma ordem de HAL_GPIO_PORT_NAMES (hal_cfg.h). Os nomes *_Pin vêm do main.h. */
static const port_gpio_t gpios[HAL_GPIO_NUM_PINS] = {
    [HAL_GPIO_LED_STATUS] = { LED_STATUS_GPIO_Port, LED_STATUS_Pin },
    [HAL_GPIO_LED_ERROR]  = { LED_ERROR_GPIO_Port,  LED_ERROR_Pin  },
    [HAL_GPIO_BTN]        = { BTN_GPIO_Port,        BTN_Pin        },
    [HAL_GPIO_IMU_CS]     = { IMU_CS_GPIO_Port,     IMU_CS_Pin     },
    [HAL_GPIO_IMU_INT1]   = { IMU_INT1_GPIO_Port,   IMU_INT1_Pin   },
    [HAL_GPIO_IMU_INT2]   = { IMU_INT2_GPIO_Port,   IMU_INT2_Pin   },
    [HAL_GPIO_SD_CD]      = { SD_CD_GPIO_Port,      SD_CD_Pin      },
};

static hal_gpio_cbk_t gpio_cbks[HAL_GPIO_NUM_PINS] = { 0 };

static const uint32_t gpio_mode_map[] = {
    [HAL_GPIO_MODE_INPUT]              = GPIO_MODE_INPUT,
    [HAL_GPIO_MODE_OUTPUT_PP]          = GPIO_MODE_OUTPUT_PP,
    [HAL_GPIO_MODE_OUTPUT_OD]          = GPIO_MODE_OUTPUT_OD,
    [HAL_GPIO_MODE_ANALOG]             = GPIO_MODE_ANALOG,
    [HAL_GPIO_MODE_IT_RISING]          = GPIO_MODE_IT_RISING,
    [HAL_GPIO_MODE_IT_FALLING]         = GPIO_MODE_IT_FALLING,
    [HAL_GPIO_MODE_IT_RISING_FALLING]  = GPIO_MODE_IT_RISING_FALLING,
};

static const uint32_t gpio_pull_map[] = {
    [HAL_GPIO_PULL_NONE] = GPIO_NOPULL,
    [HAL_GPIO_PULL_UP]   = GPIO_PULLUP,
    [HAL_GPIO_PULL_DOWN] = GPIO_PULLDOWN,
};

static const uint32_t gpio_speed_map[] = {
    [HAL_GPIO_SPEED_LOW]    = GPIO_SPEED_FREQ_LOW,
    [HAL_GPIO_SPEED_MEDIUM] = GPIO_SPEED_FREQ_MEDIUM,
    [HAL_GPIO_SPEED_FAST]   = GPIO_SPEED_FREQ_HIGH,
    [HAL_GPIO_SPEED_HIGH]   = GPIO_SPEED_FREQ_VERY_HIGH,
};

/* Posição do bit (0..15) a partir da máscara GPIO_PIN_x. */
static uint32_t port_gpio_pin_pos(uint16_t pin_mask)
{
    return (uint32_t) __builtin_ctz(pin_mask);
}

static IRQn_Type port_gpio_exti_irq(uint16_t pin_mask)
{
    /* No U5 cada linha EXTI tem seu próprio vetor, em sequência. */
    return (IRQn_Type) (EXTI0_IRQn + (int32_t) port_gpio_pin_pos(pin_mask));
}

static void port_gpio_configure(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_speed_t speed, hal_gpio_pull_t pull)
{
    GPIO_InitTypeDef init = { 0 };

    if(pin >= HAL_GPIO_NUM_PINS)
        return;

    init.Pin = gpios[pin].pin;
    init.Mode = gpio_mode_map[mode];
    init.Pull = gpio_pull_map[pull];
    init.Speed = gpio_speed_map[speed];

    HAL_GPIO_Init(gpios[pin].port, &init);
}

static void port_gpio_interrupt_cbk_set(hal_gpio_pin_t pin, hal_gpio_cbk_t cbk)
{
    if(pin < HAL_GPIO_NUM_PINS)
        gpio_cbks[pin] = cbk;
}

static void port_gpio_interrupt_set(hal_gpio_pin_t pin, hal_gpio_interrupt_state_t state)
{
    if(pin >= HAL_GPIO_NUM_PINS)
        return;

    IRQn_Type irq = port_gpio_exti_irq(gpios[pin].pin);

    if(state == HAL_GPIO_INTERRUPT_ENABLE)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(gpios[pin].pin);
        HAL_NVIC_ClearPendingIRQ(irq);
        HAL_NVIC_EnableIRQ(irq);
    }
    else
    {
        HAL_NVIC_DisableIRQ(irq);
    }
}

static void port_gpio_set(hal_gpio_pin_t pin, bool state)
{
    if(pin < HAL_GPIO_NUM_PINS)
        HAL_GPIO_WritePin(gpios[pin].port, gpios[pin].pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool port_gpio_get(hal_gpio_pin_t pin)
{
    if(pin >= HAL_GPIO_NUM_PINS)
        return false;

    return HAL_GPIO_ReadPin(gpios[pin].port, gpios[pin].pin) == GPIO_PIN_SET;
}

static void port_gpio_toggle(hal_gpio_pin_t pin)
{
    if(pin < HAL_GPIO_NUM_PINS)
        HAL_GPIO_TogglePin(gpios[pin].port, gpios[pin].pin);
}

/* Uma linha EXTI atende o mesmo número de pino em qualquer porta; chama o
   callback do pino lógico que usa essa linha. */
static void port_gpio_exti_dispatch(uint16_t pin_mask)
{
    for(uint32_t n = 0; n < HAL_GPIO_NUM_PINS; n++)
    {
        if(gpios[n].pin == pin_mask && gpio_cbks[n] != NULL)
        {
            gpio_cbks[n]();
            break;
        }
    }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    port_gpio_exti_dispatch(GPIO_Pin);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    port_gpio_exti_dispatch(GPIO_Pin);
}

hal_gpio_driver_t HAL_GPIO_DRIVER = {
    .configure = port_gpio_configure,
    .interrupt_cbk_set = port_gpio_interrupt_cbk_set,
    .interrupt_set = port_gpio_interrupt_set,
    .set = port_gpio_set,
    .get = port_gpio_get,
    .toggle = port_gpio_toggle,
};
