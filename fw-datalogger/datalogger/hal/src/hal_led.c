#include "hal.h"

#if HAL_LED_ENABLED == 1

#define LED_MAX_STEPS 6

/* Durações em múltiplos de 100 ms, alternando aceso/apagado (índice par = aceso). */
typedef struct led_pattern_s
{
    uint8_t num_steps;
    uint8_t steps_100ms[LED_MAX_STEPS];
    bool repeat;
} led_pattern_t;

typedef struct led_ctrl_s
{
    hal_gpio_pin_t pin;
    hal_led_mode_t mode;
    uint8_t step;
    uint8_t counter;
    bool in_use;
} led_ctrl_t;

static const led_pattern_t led_patterns[HAL_LED_NUM_MODES] = {
    [HAL_LED_MODE_OFF]          = { 0, { 0 }, false },
    [HAL_LED_MODE_ON]           = { 0, { 0 }, false },
    [HAL_LED_MODE_BLINK_ONCE]   = { 2, { 1, 1 }, false },
    [HAL_LED_MODE_BLINK_FAST]   = { 2, { 1, 1 }, true },
    [HAL_LED_MODE_BLINK_SLOW_1X]= { 2, { 1, 19 }, true },
    [HAL_LED_MODE_BLINK_FAST_2X]= { 4, { 1, 2, 1, 16 }, true },
    [HAL_LED_MODE_BLINK_FAST_3X]= { 6, { 1, 2, 1, 2, 1, 13 }, true },
};

static led_ctrl_t leds[HAL_LED_NUM_AUTO_LEDS];

static void led_write(hal_gpio_pin_t pin, bool on)
{
    hal_gpio_set(pin, on ? HAL_LED_ON_LEVEL : !HAL_LED_ON_LEVEL);
}

/* Roda na ISR do SysTick a cada 100 ms. */
static void hal_led_100ms_cbk(void)
{
    for(uint32_t n = 0; n < HAL_LED_NUM_AUTO_LEDS; n++)
    {
        led_ctrl_t* led = &leds[n];

        if(!led->in_use || led->mode <= HAL_LED_MODE_ON)
            continue;

        const led_pattern_t* pat = &led_patterns[led->mode];

        if(led->step >= pat->num_steps)
            continue; /* padrão de uma vez só, já terminou */

        if(++led->counter < pat->steps_100ms[led->step])
            continue;

        led->counter = 0;
        led->step++;

        if(led->step >= pat->num_steps)
        {
            if(!pat->repeat)
            {
                led_write(led->pin, false);
                continue;
            }
            led->step = 0;
        }

        led_write(led->pin, (led->step & 1U) == 0U);
    }
}

void hal_led_init(void)
{
    memset(leds, 0, sizeof(leds));
    hal_time_cbk_register(hal_led_100ms_cbk, HAL_TIME_100MS_CBK);
}

void hal_led_mode_set(hal_gpio_pin_t pin, hal_led_mode_t mode)
{
    led_ctrl_t* slot = NULL;

    if(mode >= HAL_LED_NUM_MODES)
        return;

    for(uint32_t n = 0; n < HAL_LED_NUM_AUTO_LEDS; n++)
    {
        if(leds[n].in_use && leds[n].pin == pin)
        {
            slot = &leds[n];
            break;
        }
        if(!leds[n].in_use && slot == NULL)
            slot = &leds[n];
    }

    if(slot == NULL)
        return; /* aumentar HAL_LED_NUM_AUTO_LEDS */

    uint32_t cs = hal_cpu_critical_section_enter();
    slot->in_use = false;
    slot->pin = pin;
    slot->mode = mode;
    slot->step = 0;
    slot->counter = 0;
    led_write(pin, mode != HAL_LED_MODE_OFF);
    slot->in_use = true;
    hal_cpu_critical_section_leave(cs);
}

hal_led_mode_t hal_led_mode_get(hal_gpio_pin_t pin)
{
    for(uint32_t n = 0; n < HAL_LED_NUM_AUTO_LEDS; n++)
    {
        if(leds[n].in_use && leds[n].pin == pin)
            return leds[n].mode;
    }

    return HAL_LED_MODE_OFF;
}

#endif
