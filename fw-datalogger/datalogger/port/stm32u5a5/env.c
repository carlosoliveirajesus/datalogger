/**
 * @file env.c
 * @brief Port de hal_env: tabela de chips e barramento I2C3 do BME280.
 */
#include "main.h"
#include "i2c.h"
#include "hal.h"
#include "env/bme280.h"

#define ENV_I2C_TIMEOUT_MS 10U

/* ------------------------------------------------ barramento I2C3 */

static bool bme280_i2c_read(uint8_t addr, uint8_t reg, uint8_t* data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c3, (uint16_t) (addr << 1), reg, I2C_MEMADD_SIZE_8BIT, data, len,
                            ENV_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool bme280_i2c_write(uint8_t addr, uint8_t reg, const uint8_t* data, uint16_t len)
{
    return HAL_I2C_Mem_Write(&hi2c3, (uint16_t) (addr << 1), reg, I2C_MEMADD_SIZE_8BIT, (uint8_t*) data, len,
                             ENV_I2C_TIMEOUT_MS) == HAL_OK;
}

static const bme280_bus_t bme280_bus = {
    .read = bme280_i2c_read,
    .write = bme280_i2c_write,
    .delay_ms = HAL_Delay,
};

static bme280_t bme280_dev;

/* ---------------------------------------- adaptadores do BME280 */

/* Destrava o barramento I2C3. Se um transiente (ex.: dedo sobre o módulo)
   corta uma transferência no meio, o BME280 pode ficar segurando o SDA em 0
   esperando clocks que não vêm, e o I2C do STM32 passa a dar timeout. Aqui
   o SCL vira GPIO e recebe até 9 pulsos até o SDA soltar, gera um STOP e o
   periférico é reiniciado (MX_I2C3_Init, gerado pelo CubeMX). */
static void port_env_wait_us(uint32_t us)
{
    uint64_t t0 = hal_time_get_us();

    while((hal_time_get_us() - t0) < us)
    {
    }
}

static void port_env_i2c_recover(void)
{
    GPIO_InitTypeDef g = { 0 };

    (void) HAL_I2C_DeInit(&hi2c3);
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(BME_SCL_GPIO_Port, BME_SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BME_SDA_GPIO_Port, BME_SDA_Pin, GPIO_PIN_SET);
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = BME_SCL_Pin;
    HAL_GPIO_Init(BME_SCL_GPIO_Port, &g);
    g.Pin = BME_SDA_Pin;
    HAL_GPIO_Init(BME_SDA_GPIO_Port, &g); /* OD em 1 = solto, dá para ler */

    for(uint32_t n = 0; n < 9U && HAL_GPIO_ReadPin(BME_SDA_GPIO_Port, BME_SDA_Pin) == GPIO_PIN_RESET; n++)
    {
        HAL_GPIO_WritePin(BME_SCL_GPIO_Port, BME_SCL_Pin, GPIO_PIN_RESET);
        port_env_wait_us(5);
        HAL_GPIO_WritePin(BME_SCL_GPIO_Port, BME_SCL_Pin, GPIO_PIN_SET);
        port_env_wait_us(5);
    }

    /* STOP: SDA sobe com SCL alto. */
    HAL_GPIO_WritePin(BME_SDA_GPIO_Port, BME_SDA_Pin, GPIO_PIN_RESET);
    port_env_wait_us(5);
    HAL_GPIO_WritePin(BME_SCL_GPIO_Port, BME_SCL_Pin, GPIO_PIN_SET);
    port_env_wait_us(5);
    HAL_GPIO_WritePin(BME_SDA_GPIO_Port, BME_SDA_Pin, GPIO_PIN_SET);
    port_env_wait_us(5);

    MX_I2C3_Init();
}

static bool port_bme280_init(void)
{
    bool ok;

    port_env_i2c_recover();
    ok = bme280_init(&bme280_dev, &bme280_bus);

    if(bme280_dev.addr == 0)
        UTL_DBG_PRINTF(UTL_DBG_MOD_ENV, "BME280 nao respondeu em 0x%02X/0x%02X\r\n", BME280_I2C_ADDR_PRIMARY,
                       BME280_I2C_ADDR_SECONDARY);
    else
        UTL_DBG_PRINTF(UTL_DBG_MOD_ENV, "BME280 @0x%02X chip_id=0x%02X%s %s\r\n", bme280_dev.addr,
                       bme280_dev.chip_id, bme280_dev.chip_id == BMP280_CHIP_ID_VALUE ? " (BMP280!)" : "",
                       ok ? "OK" : "FALHA");

    return ok;
}

static void port_bme280_deinit(void)
{
    bme280_deinit(&bme280_dev);
}

static bool port_bme280_is_initialized(void)
{
    return bme280_dev.initialized;
}

/* Coeficiente do filtro IIR (0, 2, 4, 8, 16) -> código do registrador. */
static uint8_t port_bme280_filter_code(uint8_t coef)
{
    if(coef >= 16)
        return 4;
    if(coef >= 8)
        return 3;
    if(coef >= 4)
        return 2;
    if(coef >= 2)
        return 1;
    return 0;
}

/* Tempo entre medições em ms -> o código mais próximo sem passar dele. */
static bme280_standby_t port_bme280_standby_code(uint16_t ms)
{
    static const struct
    {
        uint16_t ms;
        bme280_standby_t code;
    } map[] = {
        { 1000, BME280_STANDBY_1000MS }, { 500, BME280_STANDBY_500MS }, { 250, BME280_STANDBY_250MS },
        { 125, BME280_STANDBY_125MS },   { 62, BME280_STANDBY_62MS5 },  { 20, BME280_STANDBY_20MS },
        { 10, BME280_STANDBY_10MS },
    };

    for(size_t n = 0; n < UTL_ARRAY_SIZE(map); n++)
    {
        if(ms >= map[n].ms)
            return map[n].code;
    }

    return BME280_STANDBY_0MS5;
}

static bool port_bme280_setup(const hal_env_config_t* cfg)
{
    /* hal_env_osr_t usa a mesma numeração do registrador (0 = skip, 5 = x16). */
    bool ok = bme280_setup(&bme280_dev, (uint8_t) cfg->osr_temp, (uint8_t) cfg->osr_press, (uint8_t) cfg->osr_hum,
                           port_bme280_filter_code(cfg->filter_coef), port_bme280_standby_code(cfg->standby_ms));

    UTL_DBG_PRINTF(UTL_DBG_MOD_ENV, "BME280 setup %s\r\n", ok ? "OK" : "FALHA");

    return ok;
}

static bool port_bme280_read(hal_env_data_t* data)
{
    bme280_data_t raw;

    if(!bme280_read(&bme280_dev, &raw))
        return false;

    data->temp_c = (float) raw.temp_c100 / 100.0f;
    data->press_pa = (float) raw.press_pa256 / 256.0f;
    data->hum_rh = (float) raw.hum_rh1024 / 1024.0f;

    return true;
}

/* ------------------------------------------------- tabela de chips */

typedef struct port_env_driver_s
{
    bool (*init)(void);
    void (*deinit)(void);
    bool (*is_initialized)(void);
    bool (*setup)(const hal_env_config_t* config);
    bool (*read)(hal_env_data_t* data);
} port_env_driver_t;

static const port_env_driver_t port_env_drv[HAL_ENV_NUM_DEVS] = {
    [HAL_ENV_DEV_BME280] = {
        .init = port_bme280_init,
        .deinit = port_bme280_deinit,
        .is_initialized = port_bme280_is_initialized,
        .setup = port_bme280_setup,
        .read = port_bme280_read,
    },
};

/* ------------------------------------- despacho genérico (HAL_ENV_DRIVER) */

static bool port_env_ready(hal_env_dev_t dev)
{
    return dev < HAL_ENV_NUM_DEVS && port_env_drv[dev].is_initialized && port_env_drv[dev].is_initialized();
}

static bool port_env_init(hal_env_dev_t dev)
{
    if(dev >= HAL_ENV_NUM_DEVS || port_env_drv[dev].init == NULL)
        return false;

    return port_env_drv[dev].init();
}

static void port_env_deinit(hal_env_dev_t dev)
{
    if(dev < HAL_ENV_NUM_DEVS && port_env_drv[dev].deinit)
        port_env_drv[dev].deinit();
}

static bool port_env_setup(hal_env_dev_t dev, const hal_env_config_t* config)
{
    if(!port_env_ready(dev) || port_env_drv[dev].setup == NULL || config == NULL)
        return false;

    return port_env_drv[dev].setup(config);
}

static bool port_env_read(hal_env_dev_t dev, hal_env_data_t* data)
{
    if(!port_env_ready(dev) || port_env_drv[dev].read == NULL || data == NULL)
        return false;

    return port_env_drv[dev].read(data);
}

hal_env_driver_t HAL_ENV_DRIVER = {
    .init = port_env_init,
    .deinit = port_env_deinit,
    .setup = port_env_setup,
    .read = port_env_read,
};
