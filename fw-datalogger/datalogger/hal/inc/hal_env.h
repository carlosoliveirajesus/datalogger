/**
 * @file hal_env.h
 * @brief Sensor ambiental (temperatura, umidade, pressão), independente do chip.
 *
 * Os dispositivos são listados em HAL_ENV_DEVS (hal_cfg.h).
 */
#pragma once

typedef enum hal_env_dev_e
{
    HAL_ENV_DEVS,
    HAL_ENV_NUM_DEVS,
} hal_env_dev_t;

typedef enum hal_env_osr_e
{
    HAL_ENV_OSR_SKIP = 0, /**< canal desligado */
    HAL_ENV_OSR_1X,
    HAL_ENV_OSR_2X,
    HAL_ENV_OSR_4X,
    HAL_ENV_OSR_8X,
    HAL_ENV_OSR_16X,
} hal_env_osr_t;

typedef struct hal_env_config_s
{
    hal_env_osr_t osr_temp;
    hal_env_osr_t osr_press;
    hal_env_osr_t osr_hum;
    uint8_t filter_coef;    /**< filtro IIR: 0 (off), 2, 4, 8, 16 */
    uint16_t standby_ms;    /**< tempo entre medições no modo normal */
} hal_env_config_t;

typedef struct hal_env_data_s
{
    float temp_c;       /**< °C   */
    float press_pa;     /**< Pa   */
    float hum_rh;       /**< %UR  */
} hal_env_data_t;

typedef struct hal_env_driver_s
{
    bool (*init)(hal_env_dev_t dev);
    void (*deinit)(hal_env_dev_t dev);
    bool (*setup)(hal_env_dev_t dev, const hal_env_config_t* config);
    bool (*read)(hal_env_dev_t dev, hal_env_data_t* data);
} hal_env_driver_t;

/** Detecta o chip, reseta e lê a calibração. false se não responder. */
bool hal_env_init(hal_env_dev_t dev);
void hal_env_deinit(hal_env_dev_t dev);
bool hal_env_setup(hal_env_dev_t dev, const hal_env_config_t* config);
/** Última medição, já compensada. */
bool hal_env_read(hal_env_dev_t dev, hal_env_data_t* data);
