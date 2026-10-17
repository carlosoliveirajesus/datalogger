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

static bool port_bme280_init(void)
{
    bool ok = bme280_init(&bme280_dev, &bme280_bus);

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
        .setup = NULL, /* TODO passo 2 */
        .read = NULL,  /* TODO passo 2 */
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
