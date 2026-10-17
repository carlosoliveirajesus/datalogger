/**
 * @file imu.c
 * @brief Port de hal_imu: tabela de chips e barramento SPI1 do LSM6DSOX.
 *
 * Para adicionar outro chip: criar imu/<chip>.c com a mesma ideia de
 * barramento injetado, uma entrada em HAL_IMU_DEVS (hal_cfg.h) e uma
 * entrada em port_imu_drv[].
 */
#include "main.h"
#include "spi.h"
#include "hal.h"
#include "imu/lsm6dsox.h"

#define IMU_SPI_TIMEOUT_MS 10U
#define IMU_SPI_READ_BIT   0x80U

/* ------------------------------------------------ barramento SPI1 */

static bool lsm6dsox_spi_read(uint8_t reg, uint8_t* data, uint16_t len)
{
    uint8_t addr = (uint8_t) (reg | IMU_SPI_READ_BIT);
    bool ok;

    hal_gpio_set(HAL_GPIO_IMU_CS, false);
    ok = (HAL_SPI_Transmit(&hspi1, &addr, 1, IMU_SPI_TIMEOUT_MS) == HAL_OK) &&
         (HAL_SPI_Receive(&hspi1, data, len, IMU_SPI_TIMEOUT_MS) == HAL_OK);
    hal_gpio_set(HAL_GPIO_IMU_CS, true);

    return ok;
}

static bool lsm6dsox_spi_write(uint8_t reg, const uint8_t* data, uint16_t len)
{
    uint8_t addr = (uint8_t) (reg & ~IMU_SPI_READ_BIT);
    bool ok;

    hal_gpio_set(HAL_GPIO_IMU_CS, false);
    ok = (HAL_SPI_Transmit(&hspi1, &addr, 1, IMU_SPI_TIMEOUT_MS) == HAL_OK) &&
         (HAL_SPI_Transmit(&hspi1, (uint8_t*) data, len, IMU_SPI_TIMEOUT_MS) == HAL_OK);
    hal_gpio_set(HAL_GPIO_IMU_CS, true);

    return ok;
}

static const lsm6dsox_bus_t lsm6dsox_bus = {
    .read = lsm6dsox_spi_read,
    .write = lsm6dsox_spi_write,
    .delay_ms = HAL_Delay,
};

static lsm6dsox_t lsm6dsox_dev;

/* --------------------------------------- adaptadores do LSM6DSOX */

static bool port_lsm6dsox_init(void)
{
    uint8_t id = 0;
    bool ok = lsm6dsox_init(&lsm6dsox_dev, &lsm6dsox_bus);

    (void) lsm6dsox_who_am_i_get(&lsm6dsox_dev, &id);
    UTL_DBG_PRINTF(UTL_DBG_MOD_IMU, "LSM6DSOX WHO_AM_I=0x%02X (esperado 0x%02X) %s\r\n", id,
                   LSM6DSOX_WHO_AM_I_VALUE, ok ? "OK" : "FALHA");

    return ok;
}

static void port_lsm6dsox_deinit(void)
{
    lsm6dsox_deinit(&lsm6dsox_dev);
}

static bool port_lsm6dsox_is_initialized(void)
{
    return lsm6dsox_dev.initialized;
}

static float port_lsm6dsox_accel_scale_get(void)
{
    return lsm6dsox_dev.accel_scale;
}

static float port_lsm6dsox_gyro_scale_get(void)
{
    return lsm6dsox_dev.gyro_scale;
}

/* ------------------------------------------------- tabela de chips */

typedef struct port_imu_driver_s
{
    bool (*init)(void);
    void (*deinit)(void);
    bool (*is_initialized)(void);
    bool (*setup)(const hal_imu_config_t* config);
    bool (*raw_read)(hal_imu_raw_t* accel, hal_imu_raw_t* gyro);
    float (*accel_scale_get)(void);
    float (*gyro_scale_get)(void);
} port_imu_driver_t;

static const port_imu_driver_t port_imu_drv[HAL_IMU_NUM_DEVS] = {
    [HAL_IMU_DEV_LSM6DSOX] = {
        .init = port_lsm6dsox_init,
        .deinit = port_lsm6dsox_deinit,
        .is_initialized = port_lsm6dsox_is_initialized,
        .setup = NULL,    /* TODO passo 3 */
        .raw_read = NULL, /* TODO passo 3 */
        .accel_scale_get = port_lsm6dsox_accel_scale_get,
        .gyro_scale_get = port_lsm6dsox_gyro_scale_get,
    },
};

/* ------------------------------------- despacho genérico (HAL_IMU_DRIVER) */

static bool port_imu_ready(hal_imu_dev_t dev)
{
    return dev < HAL_IMU_NUM_DEVS && port_imu_drv[dev].is_initialized && port_imu_drv[dev].is_initialized();
}

static bool port_imu_init(hal_imu_dev_t dev)
{
    if(dev >= HAL_IMU_NUM_DEVS || port_imu_drv[dev].init == NULL)
        return false;

    return port_imu_drv[dev].init();
}

static void port_imu_deinit(hal_imu_dev_t dev)
{
    if(dev < HAL_IMU_NUM_DEVS && port_imu_drv[dev].deinit)
        port_imu_drv[dev].deinit();
}

static bool port_imu_setup(hal_imu_dev_t dev, const hal_imu_config_t* config)
{
    if(!port_imu_ready(dev) || port_imu_drv[dev].setup == NULL || config == NULL)
        return false;

    return port_imu_drv[dev].setup(config);
}

static bool port_imu_raw_read(hal_imu_dev_t dev, hal_imu_raw_t* accel, hal_imu_raw_t* gyro)
{
    if(!port_imu_ready(dev) || port_imu_drv[dev].raw_read == NULL)
        return false;

    return port_imu_drv[dev].raw_read(accel, gyro);
}

static float port_imu_accel_scale_get(hal_imu_dev_t dev)
{
    return port_imu_ready(dev) && port_imu_drv[dev].accel_scale_get ? port_imu_drv[dev].accel_scale_get() : 0.0f;
}

static float port_imu_gyro_scale_get(hal_imu_dev_t dev)
{
    return port_imu_ready(dev) && port_imu_drv[dev].gyro_scale_get ? port_imu_drv[dev].gyro_scale_get() : 0.0f;
}

hal_imu_driver_t HAL_IMU_DRIVER = {
    .init = port_imu_init,
    .deinit = port_imu_deinit,
    .setup = port_imu_setup,
    .raw_read = port_imu_raw_read,
    .accel_scale_get = port_imu_accel_scale_get,
    .gyro_scale_get = port_imu_gyro_scale_get,
};
