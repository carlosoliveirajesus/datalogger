/**
 * @file lsm6dsox.h
 * @brief Driver do LSM6DSOX (ST), independente do barramento.
 *
 * O acesso ao barramento é injetado pelo port (lsm6dsox_bus_t), então o
 * mesmo driver serve para SPI ou I2C. Referência: datasheet LSM6DSOX DS12814.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LSM6DSOX_WHO_AM_I_VALUE 0x6C

/* Endereços I2C de 7 bits, conforme o pino SA0/SDO (DS12814, 6.3). */
#define LSM6DSOX_I2C_ADDR_SA0_LOW  0x6A
#define LSM6DSOX_I2C_ADDR_SA0_HIGH 0x6B

/* Registradores usados (DS12814, seção 8). */
#define LSM6DSOX_REG_INT1_CTRL 0x0D
#define LSM6DSOX_REG_WHO_AM_I  0x0F
#define LSM6DSOX_REG_CTRL1_XL  0x10
#define LSM6DSOX_REG_CTRL2_G   0x11
#define LSM6DSOX_REG_CTRL3_C   0x12
#define LSM6DSOX_REG_CTRL4_C   0x13
#define LSM6DSOX_REG_STATUS    0x1E
#define LSM6DSOX_REG_OUTX_L_G  0x22
#define LSM6DSOX_REG_OUTX_L_A  0x28

#define LSM6DSOX_CTRL3_C_BOOT     (1U << 7)
#define LSM6DSOX_CTRL3_C_BDU      (1U << 6)
#define LSM6DSOX_CTRL3_C_IF_INC   (1U << 2)
#define LSM6DSOX_CTRL3_C_SW_RESET (1U << 0)
#define LSM6DSOX_CTRL4_C_I2C_DIS  (1U << 2)

typedef struct lsm6dsox_bus_s
{
    bool (*read)(uint8_t reg, uint8_t* data, uint16_t len);
    bool (*write)(uint8_t reg, const uint8_t* data, uint16_t len);
    void (*delay_ms)(uint32_t ms);
    bool is_spi; /**< true: desliga a interface I2C do chip no init */
} lsm6dsox_bus_t;

typedef struct lsm6dsox_s
{
    const lsm6dsox_bus_t* bus;
    float accel_scale; /**< g/LSB   */
    float gyro_scale;  /**< dps/LSB */
    bool initialized;
} lsm6dsox_t;

/**
 * Confere o WHO_AM_I e faz reset por software. Em SPI (bus->is_spi) também
 * desliga a interface I2C do chip; em I2C ela fica ligada, claro.
 */
bool lsm6dsox_init(lsm6dsox_t* dev, const lsm6dsox_bus_t* bus);
void lsm6dsox_deinit(lsm6dsox_t* dev);
bool lsm6dsox_who_am_i_get(lsm6dsox_t* dev, uint8_t* id);
