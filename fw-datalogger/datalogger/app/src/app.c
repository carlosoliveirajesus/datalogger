/**
 * @file app.c
 * @brief Máquina de estados do Datalogger.
 *
 * Sinalização:
 *   LED status (verde) pisca 1x a cada 2 s = vivo, sem erro.
 *   LED erro (azul):  2 pulsos = sem cartão
 *                     3 pulsos = falha de sensor
 *                     rápido   = falha ao iniciar o cartão
 */
#include "hal.h"
#include "app.h"

#define APP_STATUS_PRINT_MS 5000U

static app_state_t app_state = APP_STATE_INIT;
static uint32_t app_errors = APP_ERROR_NONE;
static volatile bool app_btn_pressed = false;
static uint32_t app_status_tmr = 0;

static const char* const app_state_names[] = {
    [APP_STATE_INIT] = "INIT",
    [APP_STATE_IDLE] = "IDLE",
    [APP_STATE_LOGGING] = "LOGGING",
    [APP_STATE_ERROR] = "ERROR",
};

app_state_t app_state_get(void)
{
    return app_state;
}

uint32_t app_errors_get(void)
{
    return app_errors;
}

static void app_state_set(app_state_t new_state)
{
    if(new_state != app_state)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Estado %s -> %s\r\n", app_state_names[app_state], app_state_names[new_state]);
        app_state = new_state;
    }
}

/* Chamado na ISR do EXTI13: só levanta o flag. */
static void app_btn_isr(void)
{
    app_btn_pressed = true;
}

static void app_leds_update(void)
{
    hal_led_mode_t err_mode = HAL_LED_MODE_OFF;

    if(app_errors & APP_ERROR_SD_ABSENT)
        err_mode = HAL_LED_MODE_BLINK_FAST_2X;
    else if(app_errors & (APP_ERROR_IMU | APP_ERROR_ENV))
        err_mode = HAL_LED_MODE_BLINK_FAST_3X;
    else if(app_errors & APP_ERROR_SD_INIT)
        err_mode = HAL_LED_MODE_BLINK_FAST;

    hal_led_mode_set(HAL_GPIO_LED_ERROR, err_mode);
    hal_led_mode_set(HAL_GPIO_LED_STATUS, app_errors ? HAL_LED_MODE_OFF : HAL_LED_MODE_BLINK_SLOW_1X);
}

static void app_sd_start(void)
{
    hal_sd_status_t st = hal_sd_init();
    hal_sd_info_t info;

    app_errors &= ~(uint32_t) (APP_ERROR_SD_ABSENT | APP_ERROR_SD_INIT);

    if(st == HAL_SD_ERR_NO_CARD)
    {
        app_errors |= APP_ERROR_SD_ABSENT;
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "Cartao ausente\r\n");
        return;
    }

    if(st != HAL_SD_OK)
    {
        app_errors |= APP_ERROR_SD_INIT;
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "Falha ao iniciar o cartao (%d)\r\n", (int) st);
        return;
    }

    if(hal_sd_info_get(&info) == HAL_SD_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "Cartao OK: %lu blocos de %lu B (%lu MB)\r\n", (unsigned long) info.block_count,
                       (unsigned long) info.block_size,
                       (unsigned long) (((uint64_t) info.block_count * info.block_size) / (1024U * 1024U)));
    }
}

void app_dbg_init(void)
{
    utl_dbg_mod_enable(UTL_DBG_MOD_APP);
    utl_dbg_mod_enable(UTL_DBG_MOD_HAL);
    utl_dbg_mod_enable(UTL_DBG_MOD_IMU);
    utl_dbg_mod_enable(UTL_DBG_MOD_ENV);
    utl_dbg_mod_enable(UTL_DBG_MOD_SD);
    utl_dbg_mod_enable(UTL_DBG_MOD_LOGGER);
}

void app_init(void)
{
    uint8_t uid[HAL_CPU_ID_SIZE];

    hal_cpu_id_get(uid);

    utl_printf("\r\n==== Datalogger EPTA v%d.%d.%d ====\r\n", APP_VER_MAJOR, APP_VER_MINOR, APP_VER_BUILD);
    utl_printf("UID: ");
    for(uint32_t n = 0; n < HAL_CPU_ID_SIZE; n++)
        utl_printf("%02X", uid[HAL_CPU_ID_SIZE - 1U - n]);
    utl_printf("\r\n");

    hal_gpio_interrupt_cbk_set(HAL_GPIO_BTN, app_btn_isr);

    app_state = APP_STATE_INIT;
    app_errors = APP_ERROR_NONE;
}

static void app_state_init_exec(void)
{
    if(!hal_imu_init(HAL_IMU_DEV_LSM6DSOX))
        app_errors |= APP_ERROR_IMU;

    if(!hal_env_init(HAL_ENV_DEV_BME280))
        app_errors |= APP_ERROR_ENV;

    app_sd_start();
    app_leds_update();

    /* TODO passos 2-4: hal_imu_setup / hal_env_setup e montar o FatFS. */

    app_state_set(app_errors ? APP_STATE_ERROR : APP_STATE_IDLE);
    app_status_tmr = hal_time_get_ms();
}

static void app_state_idle_exec(void)
{
    /* TODO passo 5: abrir o arquivo do boot e ir para APP_STATE_LOGGING. */
}

static void app_state_logging_exec(void)
{
    /* TODO passo 5: drenar o buffer circular da IMU e gravar blocos de 512 B. */
}

static void app_state_error_exec(void)
{
    /* Cartão inserido depois do boot: tenta de novo. */
    if((app_errors & APP_ERROR_SD_ABSENT) && hal_sd_is_present())
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "Cartao inserido\r\n");
        app_sd_start();
        app_leds_update();
        if(app_errors == APP_ERROR_NONE)
            app_state_set(APP_STATE_IDLE);
    }
}

void app_loop(void)
{
    hal_cpu_watchdog_refresh();

    if(app_btn_pressed)
    {
        app_btn_pressed = false;
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Botao\r\n");
    }

    switch(app_state)
    {
        case APP_STATE_INIT:    app_state_init_exec();    break;
        case APP_STATE_IDLE:    app_state_idle_exec();    break;
        case APP_STATE_LOGGING: app_state_logging_exec(); break;
        case APP_STATE_ERROR:   app_state_error_exec();   break;
        default:                app_state_set(APP_STATE_ERROR); break;
    }

    if(hal_time_elapsed_since_ms(app_status_tmr) >= APP_STATUS_PRINT_MS)
    {
        app_status_tmr = hal_time_get_ms();
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "estado=%s erros=0x%02lX\r\n", app_state_names[app_state],
                       (unsigned long) app_errors);
    }
}
