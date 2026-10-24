/**
 * @file app.c
 * @brief Máquina de estados do Datalogger.
 *
 * INIT ──ok──> IDLE ──botão──> LOGGING (arquivo novo) ──botão──> IDLE
 *   │                               │
 *   └──erro──> ERROR <──────────────┘ falha de escrita / cartão cheio
 *                 └── cartão volta (nova tentativa a cada 2 s) ──> IDLE, ou
 *                     LOGGING num arquivo novo se estava gravando
 *
 * O boot só confere sensores e cartão; a gravação começa no botão B1.
 *
 * Aquisição:
 *   INT1 do LSM6DSOX (PB1) -> ISR lê a amostra por I2C -> buffer circular
 *   loop: BME280 a cada 20 ms + logger_process() (CSV no cartão)
 *
 * Sinalização:
 *   LED status (verde na Nucleo): pisca 1x a cada 2 s = gravando
 *                                 aceso = pronto/parado: aperte o botão para
 *                                 gravar, ou tire o cartão com segurança
 *   LED erro (azul na Nucleo):    2 pulsos = sem cartão
 *                                 3 pulsos = falha de sensor (no boot, ou
 *                                            BME280/IMU perdido durante a
 *                                            operação: a gravação continua e
 *                                            o sensor é recuperado a cada 1 s)
 *                                 rápido   = falha ao iniciar o cartão ou sem FAT
 *                                 aceso    = cartão cheio
 */
#include "hal.h"
#include "app.h"
#include "logger.h"

#define APP_STATUS_PRINT_MS 5000U
#define APP_SD_RETRY_MS     2000U
#define APP_PRINT_MS        1000U
#define APP_ENV_READ_MS     20U
#define APP_BTN_DEBOUNCE_MS 300U
#define APP_IMU_STALL_MS    10U   /* ~4 períodos a 416 Hz sem INT1 = travou */
#define APP_IMU_LOST_MS     100U  /* sem leitura boa da IMU por 100 ms = IMU perdida */
#define APP_IMU_RETRY_MS    1000U /* nova tentativa de recuperar a IMU */
#define APP_ENV_FAIL_MAX    5U    /* leituras falhas seguidas = BME280 perdido (100 ms) */
#define APP_ENV_STALE_MAX   100U  /* leituras idênticas seguidas = dado congelado (2 s) */
#define APP_ENV_RETRY_MS    1000U /* nova tentativa de recuperar o BME280 */
#define APP_SENSOR_LOST     (APP_ERROR_ENV_LOST | APP_ERROR_IMU_LOST)

/* LSM6DSOX: ±16 g e ±2000 dps (as maiores faixas, por causa do motor do
   foguete), acc e gyro a 416 Hz, data-ready do acc no INT1 (PB1). Com o I2C1
   a 400 kHz, cada leitura de 12 bytes leva ~0,4 ms. */
static const hal_imu_config_t app_imu_cfg = {
    .accel_fs = HAL_IMU_ACCEL_16G,
    .gyro_fs = HAL_IMU_GYRO_2000DPS,
    .odr = HAL_IMU_ODR_416HZ,
    .drdy_int1 = true,
};

/* BME280 em modo normal, medição contínua. Com T x2, P x4, H x1 a medição
   leva ~14 ms (DS 9.1), então sai uma amostra nova a cada ~15 ms. O filtro
   IIR de coeficiente 4 tira o ruído da pressão sem atrasar demais a resposta
   a uma subida rápida. */
static const hal_env_config_t app_env_cfg = {
    .osr_temp = HAL_ENV_OSR_2X,
    .osr_press = HAL_ENV_OSR_4X,
    .osr_hum = HAL_ENV_OSR_1X,
    .filter_coef = 4,
    .standby_ms = 0, /* 0,5 ms */
};

static app_state_t app_state = APP_STATE_INIT;
static uint32_t app_errors = APP_ERROR_NONE;
static volatile bool app_btn_pressed = false;
static bool app_btn_event = false;
static uint32_t app_btn_tmr = 0;
static bool app_log_wanted = false; /* o usuário pediu para gravar (botão) */
static uint32_t app_status_tmr = 0;
static uint32_t app_sd_retry_tmr = 0;
static uint32_t app_print_tmr = 0;

/* IMU: lida na ISR do INT1. */
static volatile bool app_imu_ready = false;
static volatile uint32_t app_imu_drdy_cnt = 0;
static volatile uint32_t app_imu_ok_cnt = 0;
static volatile uint32_t app_imu_err_cnt = 0;
static volatile uint32_t app_imu_isr_ms = 0;
static volatile uint32_t app_imu_ok_ms = 0; /* última leitura boa */
static uint32_t app_imu_kick_cnt = 0;
static uint32_t app_imu_lost_cnt = 0;
static uint32_t app_imu_retry_tmr = 0;
static logger_imu_sample_t app_imu_last;
static uint32_t app_imu_drdy_last = 0;
static uint32_t app_imu_ok_last = 0;
static float app_acc_scale = 0.0f;
static float app_gyr_scale = 0.0f;

/* BME280: lido no loop. */
static hal_env_data_t app_env;
static bool app_env_valid = false;
static uint32_t app_env_tmr = 0;
static uint32_t app_env_fail_cnt = 0;
static uint32_t app_env_same_cnt = 0;
static uint32_t app_env_retry_tmr = 0;

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

static void app_leds_update(void)
{
    hal_led_mode_t err_mode = HAL_LED_MODE_OFF;
    hal_led_mode_t sts_mode = HAL_LED_MODE_OFF;

    if(app_errors & APP_ERROR_SD_ABSENT)
        err_mode = HAL_LED_MODE_BLINK_FAST_2X;
    else if(app_errors & (APP_ERROR_IMU | APP_ERROR_ENV | APP_SENSOR_LOST))
        err_mode = HAL_LED_MODE_BLINK_FAST_3X;
    else if(app_errors & (APP_ERROR_SD_INIT | APP_ERROR_SD_FS))
        err_mode = HAL_LED_MODE_BLINK_FAST;
    else if(app_errors & APP_ERROR_SD_FULL)
        err_mode = HAL_LED_MODE_ON;

    /* O status mostra o estado da gravação, mesmo com um sensor perdido. */
    if(app_state == APP_STATE_LOGGING)
        sts_mode = HAL_LED_MODE_BLINK_SLOW_1X;
    else if(app_state == APP_STATE_IDLE)
        sts_mode = HAL_LED_MODE_ON;

    hal_led_mode_set(HAL_GPIO_LED_ERROR, err_mode);
    hal_led_mode_set(HAL_GPIO_LED_STATUS, sts_mode);
}

static void app_state_set(app_state_t new_state)
{
    if(new_state != app_state)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Estado %s -> %s\r\n", app_state_names[app_state], app_state_names[new_state]);
        app_state = new_state;
    }

    app_leds_update();
}

/* ------------------------------------------------------- interrupções */

/* ISR do EXTI1 (INT1 do LSM6DSOX), uma vez por amostra. Prioridade 5, abaixo
   do SysTick (ver port_cpu_init), então o timeout do I2C funciona aqui. */
static void app_imu_drdy_isr(void)
{
    logger_imu_sample_t s;

    app_imu_drdy_cnt++;
    app_imu_isr_ms = hal_time_get_ms();

    if(!app_imu_ready)
        return;

    s.t_us = hal_time_get_us(); /* instante do data-ready, antes da leitura */

    if(!hal_imu_raw_read(HAL_IMU_DEV_LSM6DSOX, &s.acc, &s.gyr))
    {
        app_imu_err_cnt++;
        return;
    }

    app_imu_last = s;
    app_imu_ok_cnt++;
    app_imu_ok_ms = app_imu_isr_ms;
    logger_imu_push(&s);
}

/* Com o data-ready travado, o INT1 só desce quando a amostra é lida. Se uma
   leitura falhar ou uma borda se perder, o pino fica alto e a IMU para de
   gerar interrupções. Aqui o loop percebe e gera a interrupção por software,
   que lê a amostra e libera o pino. */
static void app_imu_stall_check(void)
{
    if(!app_imu_ready || hal_time_elapsed_since_ms(app_imu_isr_ms) < APP_IMU_STALL_MS)
        return;

    app_imu_isr_ms = hal_time_get_ms();
    app_imu_kick_cnt++;
    hal_gpio_interrupt_trigger(HAL_GPIO_IMU_INT1);
}

/* Liga a leitura na ISR depois de um init/setup bem-sucedido. */
static void app_imu_start(void)
{
    app_acc_scale = hal_imu_accel_scale_get(HAL_IMU_DEV_LSM6DSOX);
    app_gyr_scale = hal_imu_gyro_scale_get(HAL_IMU_DEV_LSM6DSOX);
    app_imu_ok_ms = hal_time_get_ms();
    app_imu_isr_ms = app_imu_ok_ms;
    app_imu_ready = true; /* a partir daqui a ISR lê a IMU */
}

/* Vigia a IMU. Se nenhuma leitura der certo por APP_IMU_LOST_MS (o I2C
   travou, a IMU resetou ou perdeu a configuração), a ISR para de acessar o
   barramento e a cada 1 s há uma tentativa de recuperação: destrava o I2C,
   reseta e reconfigura o LSM6DSOX. A gravação continua; enquanto a IMU
   estiver perdida não saem linhas no CSV (aparece um salto em t_ms). */
static void app_imu_poll(void)
{
    if((app_errors & APP_ERROR_IMU) != 0U)
        return; /* falhou no boot: só com reset */

    if((app_errors & APP_ERROR_IMU_LOST) == 0U)
    {
        if(!app_imu_ready)
            return; /* ainda no INIT */

        app_imu_stall_check();

        if(hal_time_elapsed_since_ms(app_imu_ok_ms) < APP_IMU_LOST_MS)
            return;

        /* A ISR tem prioridade maior que o loop: quando o loop roda, ela não
           está no meio de uma leitura. */
        app_imu_ready = false;
        app_imu_lost_cnt++;
        app_errors |= APP_ERROR_IMU_LOST;
        app_imu_retry_tmr = hal_time_get_ms();
        UTL_DBG_PRINTF(UTL_DBG_MOD_IMU, "LSM6DSOX perdido (%lu erros de leitura): sem linhas no CSV ate recuperar\r\n",
                       (unsigned long) app_imu_err_cnt);
        app_leds_update();
        return;
    }

    if(hal_time_elapsed_since_ms(app_imu_retry_tmr) < APP_IMU_RETRY_MS)
        return;

    app_imu_retry_tmr = hal_time_get_ms();
    if(hal_imu_init(HAL_IMU_DEV_LSM6DSOX) && hal_imu_setup(HAL_IMU_DEV_LSM6DSOX, &app_imu_cfg))
    {
        app_errors &= ~(uint32_t) APP_ERROR_IMU_LOST;
        app_imu_start();
        UTL_DBG_PRINTF(UTL_DBG_MOD_IMU, "LSM6DSOX recuperado\r\n");
        app_leds_update();
    }
}

/* ISR do EXTI13 (botão): só levanta o flag. */
static void app_btn_isr(void)
{
    app_btn_pressed = true;
}

/* ------------------------------------------------------------ cartão */

/* Inicia o cartão e, com open_file, monta o FAT e cria um arquivo novo. */
static void app_sd_start(bool open_file)
{
    hal_sd_status_t st = hal_sd_init();
    hal_sd_info_t info;

    app_errors &= ~(uint32_t) (APP_ERROR_SD_ABSENT | APP_ERROR_SD_INIT | APP_ERROR_SD_FS | APP_ERROR_SD_FULL);

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

    if(!open_file)
        return;

    switch(logger_start(app_acc_scale, app_gyr_scale))
    {
        case LOGGER_OK:
            break;
        case LOGGER_ERR_FULL:
            app_errors |= APP_ERROR_SD_FULL;
            UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Cartao cheio\r\n");
            break;
        case LOGGER_ERR_MOUNT:
            app_errors |= APP_ERROR_SD_FS;
            UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Cartao sem FAT valido: formate em FAT32\r\n");
            break;
        default:
            app_errors |= APP_ERROR_SD_FS;
            break;
    }
}

/* Estado depois de (re)iniciar o cartão. */
static void app_after_sd_start(void)
{
    /* Sensor perdido não impede gravar: o outro continua e ele volta sozinho
       (BME280: colunas vazias; IMU: sem linhas enquanto estiver fora). */
    if((app_errors & ~(uint32_t) APP_SENSOR_LOST) != APP_ERROR_NONE)
        app_state_set(APP_STATE_ERROR);
    else if(app_log_wanted && logger_is_open())
        app_state_set(APP_STATE_LOGGING);
    else
        app_state_set(APP_STATE_IDLE);
}

/* --------------------------------------------------------- sensores */

/* Lê o BME280 a cada APP_ENV_READ_MS e entrega ao logger. */
static void app_env_lost(const char* why)
{
    app_errors |= APP_ERROR_ENV_LOST;
    app_env_valid = false;
    logger_env_invalidate();
    app_env_retry_tmr = hal_time_get_ms();
    UTL_DBG_PRINTF(UTL_DBG_MOD_ENV, "BME280 perdido (%s): colunas vazias no CSV ate recuperar\r\n", why);
    app_leds_update();
}

/* Lê o BME280 a cada APP_ENV_READ_MS e entrega ao logger. Se ele parar de
   responder (leituras falhando) ou congelar (mesmo valor por 2 s), as colunas
   dele ficam vazias no CSV e a cada 1 s há uma tentativa de recuperação:
   destrava o I2C, reseta e reconfigura o sensor. A IMU não para. */
static void app_env_poll(void)
{
    hal_env_data_t env;

    if((app_errors & APP_ERROR_ENV) != 0U)
        return; /* falhou no boot: só com reset */

    if((app_errors & APP_ERROR_ENV_LOST) != 0U)
    {
        if(hal_time_elapsed_since_ms(app_env_retry_tmr) < APP_ENV_RETRY_MS)
            return;

        app_env_retry_tmr = hal_time_get_ms();
        if(hal_env_init(HAL_ENV_DEV_BME280) && hal_env_setup(HAL_ENV_DEV_BME280, &app_env_cfg))
        {
            app_errors &= ~(uint32_t) APP_ERROR_ENV_LOST;
            app_env_fail_cnt = 0;
            app_env_same_cnt = 0;
            app_env_tmr = hal_time_get_ms(); /* primeira medição sai em ~15 ms */
            UTL_DBG_PRINTF(UTL_DBG_MOD_ENV, "BME280 recuperado\r\n");
            app_leds_update();
        }
        return;
    }

    if(hal_time_elapsed_since_ms(app_env_tmr) < APP_ENV_READ_MS)
        return;

    app_env_tmr = hal_time_get_ms();

    if(!hal_env_read(HAL_ENV_DEV_BME280, &env))
    {
        if(++app_env_fail_cnt >= APP_ENV_FAIL_MAX)
            app_env_lost("sem resposta");
        return;
    }

    app_env_fail_cnt = 0;

    /* Em modo normal a pressão (resolução de 1/256 Pa) muda a cada medição;
       o mesmo valor por 2 s significa que o sensor parou de medir. */
    if(app_env_valid && env.press_pa == app_env.press_pa && env.temp_c == app_env.temp_c)
    {
        if(++app_env_same_cnt >= APP_ENV_STALE_MAX)
        {
            app_env_lost("valor congelado");
            return;
        }
    }
    else
    {
        app_env_same_cnt = 0;
    }

    app_env = env;
    app_env_valid = true;
    logger_env_set(&app_env);
}

/* Uma linha por segundo no terminal: BME280 | IMU | arquivo. Um sensor com
   erro aparece como "--". */
static void app_sensors_print(void)
{
    char env_txt[64] = "[ENV] --";
    char imu_txt[144] = "[IMU] --";
    char log_txt[96] = "[LOG] parado";
    logger_imu_sample_t s;

    if(hal_time_elapsed_since_ms(app_print_tmr) < APP_PRINT_MS)
        return;

    app_print_tmr = hal_time_get_ms();

    if((app_errors & APP_ERROR_ENV) == 0U && app_env_valid)
        utl_snprintf(env_txt, sizeof(env_txt), "[ENV] T=%.2f C P=%.2f hPa UR=%.1f %%", (double) app_env.temp_c,
                     (double) (app_env.press_pa / 100.0f), (double) app_env.hum_rh);

    if((app_errors & APP_ERROR_IMU_LOST) != 0U)
    {
        utl_snprintf(imu_txt, sizeof(imu_txt), "[IMU] -- perdido (perdas %lu, erros %lu)",
                     (unsigned long) app_imu_lost_cnt, (unsigned long) app_imu_err_cnt);
        app_imu_ok_last = app_imu_ok_cnt;
        app_imu_drdy_last = app_imu_drdy_cnt;
    }
    else if((app_errors & APP_ERROR_IMU) == 0U)
    {
        uint32_t cs = hal_cpu_critical_section_enter();
        uint32_t drdy = app_imu_drdy_cnt;
        uint32_t ok = app_imu_ok_cnt;
        s = app_imu_last;
        hal_cpu_critical_section_leave(cs);

        utl_snprintf(imu_txt, sizeof(imu_txt),
                     "[IMU] %lu/s (INT1 %lu, erros %lu, resgates %lu) acc=%+.3f %+.3f %+.3f g gyr=%+.2f %+.2f %+.2f dps",
                     (unsigned long) (ok - app_imu_ok_last), (unsigned long) (drdy - app_imu_drdy_last),
                     (unsigned long) app_imu_err_cnt, (unsigned long) app_imu_kick_cnt, (double) (s.acc.x * app_acc_scale),
                     (double) (s.acc.y * app_acc_scale), (double) (s.acc.z * app_acc_scale),
                     (double) (s.gyr.x * app_gyr_scale), (double) (s.gyr.y * app_gyr_scale),
                     (double) (s.gyr.z * app_gyr_scale));
        app_imu_ok_last = ok;
        app_imu_drdy_last = drdy;
    }

    if(logger_is_open())
    {
        logger_stats_t st;
        logger_stats_get(&st);
        utl_snprintf(log_txt, sizeof(log_txt), "[LOG] %s %lu linhas, %lu perdidas, buffer max %lu",
                     logger_file_name(), (unsigned long) st.lines, (unsigned long) st.dropped,
                     (unsigned long) st.ring_max);
    }

    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "%s | %s | %s\r\n", env_txt, imu_txt, log_txt);
}

/* ----------------------------------------------------------- estados */

static void app_state_init_exec(void)
{
    if(hal_imu_init(HAL_IMU_DEV_LSM6DSOX) && hal_imu_setup(HAL_IMU_DEV_LSM6DSOX, &app_imu_cfg))
        app_imu_start();
    else
        app_errors |= APP_ERROR_IMU;

    if(!hal_env_init(HAL_ENV_DEV_BME280) || !hal_env_setup(HAL_ENV_DEV_BME280, &app_env_cfg))
        app_errors |= APP_ERROR_ENV;

    /* Só confere o cartão; o arquivo é criado quando o botão for apertado. */
    if((app_errors & (APP_ERROR_IMU | APP_ERROR_ENV)) == 0U)
        app_sd_start(false);

    app_after_sd_start();
    if(app_state == APP_STATE_IDLE)
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Pronto: aperte o botao para gravar\r\n");
    app_status_tmr = hal_time_get_ms();
}

static void app_state_logging_exec(void)
{
    logger_status_t st;

    app_env_poll();

    if(app_btn_event)
    {
        app_log_wanted = false;
        logger_stop();
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Gravacao parada: pode remover o cartao\r\n");
        app_state_set(APP_STATE_IDLE);
        return;
    }

    st = logger_process();
    if(st == LOGGER_OK)
        return;

    /* Cartão cheio: para de vez. Outra falha (ex.: cartão removido): o estado
       ERROR tenta de novo e, se o cartão voltar, grava num arquivo novo. */
    logger_stop();
    app_errors |= (st == LOGGER_ERR_FULL) ? APP_ERROR_SD_FULL : APP_ERROR_SD_INIT;
    if(st == LOGGER_ERR_FULL)
    {
        app_log_wanted = false;
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Cartao cheio: gravacao encerrada\r\n");
    }
    app_sd_retry_tmr = hal_time_get_ms();
    app_state_set(APP_STATE_ERROR);
}

static void app_state_idle_exec(void)
{
    app_env_poll();

    /* Botão: começa a gravar num arquivo novo. */
    if(app_btn_event)
    {
        app_log_wanted = true;
        hal_sd_deinit();
        app_sd_start(true);
        app_after_sd_start();
    }
}

static void app_state_error_exec(void)
{
    app_env_poll();

    /* Só os erros do cartão têm recuperação; erro de sensor exige reset. */
    if((app_errors & (APP_ERROR_IMU | APP_ERROR_ENV)) != 0U)
        return;

    /* Cartão inserido depois do boot (com card detect). */
    if((app_errors & APP_ERROR_SD_ABSENT) && hal_sd_is_present())
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "Cartao inserido\r\n");
        app_sd_start(app_log_wanted);
        app_after_sd_start();
        return;
    }

    /* Falha ao iniciar, sem FAT ou falha de escrita (sem card detect, é o que
       acontece sem cartão no slot): tenta de novo a cada APP_SD_RETRY_MS. */
    if((app_errors & (APP_ERROR_SD_INIT | APP_ERROR_SD_FS)) &&
       hal_time_elapsed_since_ms(app_sd_retry_tmr) >= APP_SD_RETRY_MS)
    {
        app_sd_retry_tmr = hal_time_get_ms();
        logger_stop();
        hal_sd_deinit();
        app_sd_start(app_log_wanted); /* se estava gravando, continua num arquivo novo */
        app_after_sd_start();
    }
}

/* ------------------------------------------------------------- API */

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
    hal_gpio_interrupt_cbk_set(HAL_GPIO_IMU_INT1, app_imu_drdy_isr);

    app_state = APP_STATE_INIT;
    app_errors = APP_ERROR_NONE;
}

void app_loop(void)
{
    hal_cpu_watchdog_refresh();

    /* Botão com debounce: um evento a cada APP_BTN_DEBOUNCE_MS no máximo. */
    app_btn_event = false;
    if(app_btn_pressed)
    {
        app_btn_pressed = false;
        if(hal_time_elapsed_since_ms(app_btn_tmr) >= APP_BTN_DEBOUNCE_MS)
        {
            app_btn_tmr = hal_time_get_ms();
            app_btn_event = true;
            UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Botao\r\n");
        }
    }

    app_imu_poll();

    switch(app_state)
    {
        case APP_STATE_INIT:    app_state_init_exec();    break;
        case APP_STATE_IDLE:    app_state_idle_exec();    break;
        case APP_STATE_LOGGING: app_state_logging_exec(); break;
        case APP_STATE_ERROR:   app_state_error_exec();   break;
        default:                app_state_set(APP_STATE_ERROR); break;
    }

    app_sensors_print();

    if(hal_time_elapsed_since_ms(app_status_tmr) >= APP_STATUS_PRINT_MS)
    {
        app_status_tmr = hal_time_get_ms();
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "estado=%s erros=0x%02lX\r\n", app_state_names[app_state],
                       (unsigned long) app_errors);
    }
}
