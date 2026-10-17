/**
 * @file hal_imu.h
 * @brief IMU (acelerômetro + giroscópio), independente do chip.
 *
 * Os dispositivos são listados em HAL_IMU_DEVS (hal_cfg.h). O port mantém
 * uma tabela port_imu_drv[dev] com o driver de cada chip.
 */
#pragma once

typedef enum hal_imu_dev_e
{
    HAL_IMU_DEVS,
    HAL_IMU_NUM_DEVS,
} hal_imu_dev_t;

typedef enum hal_imu_accel_fs_e
{
    HAL_IMU_ACCEL_2G = 0,
    HAL_IMU_ACCEL_4G,
    HAL_IMU_ACCEL_8G,
    HAL_IMU_ACCEL_16G,
} hal_imu_accel_fs_t;

typedef enum hal_imu_gyro_fs_e
{
    HAL_IMU_GYRO_125DPS = 0,
    HAL_IMU_GYRO_250DPS,
    HAL_IMU_GYRO_500DPS,
    HAL_IMU_GYRO_1000DPS,
    HAL_IMU_GYRO_2000DPS,
} hal_imu_gyro_fs_t;

typedef enum hal_imu_odr_e
{
    HAL_IMU_ODR_OFF = 0,
    HAL_IMU_ODR_12HZ5,
    HAL_IMU_ODR_26HZ,
    HAL_IMU_ODR_52HZ,
    HAL_IMU_ODR_104HZ,
    HAL_IMU_ODR_208HZ,
    HAL_IMU_ODR_416HZ,
    HAL_IMU_ODR_833HZ,
    HAL_IMU_ODR_1666HZ,
} hal_imu_odr_t;

typedef struct hal_imu_config_s
{
    hal_imu_accel_fs_t accel_fs;
    hal_imu_gyro_fs_t gyro_fs;
    hal_imu_odr_t odr;           /**< mesmo ODR para acc e gyro      */
    bool drdy_int1;              /**< data-ready do acc no pino INT1 */
} hal_imu_config_t;

typedef struct hal_imu_raw_s
{
    int16_t x;
    int16_t y;
    int16_t z;
} hal_imu_raw_t;

typedef struct hal_imu_real_s
{
    float x;
    float y;
    float z;
} hal_imu_real_t;

typedef struct hal_imu_driver_s
{
    bool (*init)(hal_imu_dev_t dev);
    void (*deinit)(hal_imu_dev_t dev);
    bool (*setup)(hal_imu_dev_t dev, const hal_imu_config_t* config);
    bool (*raw_read)(hal_imu_dev_t dev, hal_imu_raw_t* accel, hal_imu_raw_t* gyro);
    float (*accel_scale_get)(hal_imu_dev_t dev); /**< g por LSB   */
    float (*gyro_scale_get)(hal_imu_dev_t dev);  /**< dps por LSB */
} hal_imu_driver_t;

/** Detecta e reseta o chip. Retorna false se ele não responder. */
bool hal_imu_init(hal_imu_dev_t dev);
void hal_imu_deinit(hal_imu_dev_t dev);
bool hal_imu_setup(hal_imu_dev_t dev, const hal_imu_config_t* config);
/** Lê acc e gyro brutos numa única transação (mesmo instante). */
bool hal_imu_raw_read(hal_imu_dev_t dev, hal_imu_raw_t* accel, hal_imu_raw_t* gyro);
/** Igual a hal_imu_raw_read, convertido para g e dps. */
bool hal_imu_real_read(hal_imu_dev_t dev, hal_imu_real_t* accel, hal_imu_real_t* gyro);
float hal_imu_accel_scale_get(hal_imu_dev_t dev);
float hal_imu_gyro_scale_get(hal_imu_dev_t dev);
