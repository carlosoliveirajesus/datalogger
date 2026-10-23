#include "lsm6dsox.h"

#define LSM6DSOX_RESET_TIMEOUT_MS 10U

static bool reg_read(lsm6dsox_t* dev, uint8_t reg, uint8_t* val)
{
    return dev->bus->read(reg, val, 1);
}

static bool reg_write(lsm6dsox_t* dev, uint8_t reg, uint8_t val)
{
    return dev->bus->write(reg, &val, 1);
}

bool lsm6dsox_who_am_i_get(lsm6dsox_t* dev, uint8_t* id)
{
    return reg_read(dev, LSM6DSOX_REG_WHO_AM_I, id);
}

bool lsm6dsox_init(lsm6dsox_t* dev, const lsm6dsox_bus_t* bus)
{
    uint8_t val = 0;

    dev->bus = bus;
    dev->initialized = false;
    dev->accel_scale = 0.0f;
    dev->gyro_scale = 0.0f;

    /* Em SPI, desliga o I2C antes de tudo: ruído em SDA/SCL pode confundir
       a interface I2C (DS12814, CTRL4_C). Em I2C isso cortaria a conversa. */
    if(bus->is_spi && !reg_write(dev, LSM6DSOX_REG_CTRL4_C, LSM6DSOX_CTRL4_C_I2C_DIS))
        return false;

    if(!lsm6dsox_who_am_i_get(dev, &val) || val != LSM6DSOX_WHO_AM_I_VALUE)
        return false;

    /* Reset por software e espera o bit voltar a zero. */
    if(!reg_write(dev, LSM6DSOX_REG_CTRL3_C, LSM6DSOX_CTRL3_C_SW_RESET))
        return false;

    for(uint32_t t = 0; t < LSM6DSOX_RESET_TIMEOUT_MS; t++)
    {
        bus->delay_ms(1);
        if(!reg_read(dev, LSM6DSOX_REG_CTRL3_C, &val))
            return false;
        if((val & LSM6DSOX_CTRL3_C_SW_RESET) == 0U)
            break;
    }

    if(val & LSM6DSOX_CTRL3_C_SW_RESET)
        return false;

    /* O reset volta o I2C a ligado (em SPI, desliga de novo). BDU evita ler
       LSB e MSB de amostras diferentes; IF_INC permite ler os 12 bytes de
       saída de uma vez, em SPI ou em I2C. */
    if(bus->is_spi && !reg_write(dev, LSM6DSOX_REG_CTRL4_C, LSM6DSOX_CTRL4_C_I2C_DIS))
        return false;
    if(!reg_write(dev, LSM6DSOX_REG_CTRL3_C, LSM6DSOX_CTRL3_C_BDU | LSM6DSOX_CTRL3_C_IF_INC))
        return false;

    dev->initialized = true;

    return true;
}

void lsm6dsox_deinit(lsm6dsox_t* dev)
{
    if(dev->initialized)
    {
        /* Power-down: ODR = 0 no acc e no gyro. */
        (void) reg_write(dev, LSM6DSOX_REG_CTRL1_XL, 0);
        (void) reg_write(dev, LSM6DSOX_REG_CTRL2_G, 0);
    }

    dev->initialized = false;
}

/* TODO (passo 3): lsm6dsox_setup() (ODR/FS, INT1 data-ready) e
   lsm6dsox_raw_read() (OUTX_L_G..OUTZ_H_A numa leitura de 12 bytes). */
