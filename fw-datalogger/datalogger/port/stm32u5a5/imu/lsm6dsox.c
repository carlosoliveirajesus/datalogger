#include "lsm6dsox.h"

#define LSM6DSOX_RESET_TIMEOUT_MS 50U
#define LSM6DSOX_PROBE_TRIES      3U

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
    dev->configured = false;
    dev->accel_scale = 0.0f;
    dev->gyro_scale = 0.0f;
    dev->fail_step = 0;

    /* Em SPI, desliga o I2C antes de tudo: ruído em SDA/SCL pode confundir
       a interface I2C (DS12814, CTRL4_C). Em I2C isso cortaria a conversa. */
    dev->fail_step = LSM6DSOX_STEP_I2C_DIS;
    if(bus->is_spi && !reg_write(dev, LSM6DSOX_REG_CTRL4_C, LSM6DSOX_CTRL4_C_I2C_DIS))
        return false;

    /* O chip pode estar no meio de um reset anterior: tenta algumas vezes. */
    for(uint32_t n = 0; n < LSM6DSOX_PROBE_TRIES; n++)
    {
        if(lsm6dsox_who_am_i_get(dev, &val) && val == LSM6DSOX_WHO_AM_I_VALUE)
            break;
        val = 0;
        bus->delay_ms(1);
    }

    dev->fail_step = LSM6DSOX_STEP_WHO_AM_I;
    if(val != LSM6DSOX_WHO_AM_I_VALUE)
        return false;

    /* Um reset do MCU não reseta a IMU: ela pode estar medindo a 416 Hz da
       execução anterior. Põe acc e gyro em power-down e desliga o INT1
       antes do reset por software, para o reset partir de um estado
       conhecido; com os sensores ligados a inicialização falhava em uma de
       cada duas partidas. */
    dev->fail_step = LSM6DSOX_STEP_POWER_DOWN;
    if(!reg_write(dev, LSM6DSOX_REG_INT1_CTRL, 0) || !reg_write(dev, LSM6DSOX_REG_CTRL1_XL, 0) ||
       !reg_write(dev, LSM6DSOX_REG_CTRL2_G, 0))
        return false;

    /* Reset por software e espera o bit voltar a zero. Durante o reset o chip
       pode não responder (NACK): leitura falha conta como "ainda resetando". */
    dev->fail_step = LSM6DSOX_STEP_SW_RESET;
    if(!reg_write(dev, LSM6DSOX_REG_CTRL3_C, LSM6DSOX_CTRL3_C_SW_RESET))
        return false;

    val = LSM6DSOX_CTRL3_C_SW_RESET;
    for(uint32_t t = 0; t < LSM6DSOX_RESET_TIMEOUT_MS && (val & LSM6DSOX_CTRL3_C_SW_RESET) != 0U; t++)
    {
        bus->delay_ms(1);
        if(!reg_read(dev, LSM6DSOX_REG_CTRL3_C, &val))
            val = LSM6DSOX_CTRL3_C_SW_RESET;
    }

    dev->fail_step = LSM6DSOX_STEP_RESET_WAIT;
    if(val & LSM6DSOX_CTRL3_C_SW_RESET)
        return false;

    dev->fail_step = LSM6DSOX_STEP_CONFIG;

    /* O reset volta o I2C a ligado (em SPI, desliga de novo). BDU evita ler
       LSB e MSB de amostras diferentes; IF_INC permite ler os 12 bytes de
       saída de uma vez, em SPI ou em I2C. */
    if(bus->is_spi && !reg_write(dev, LSM6DSOX_REG_CTRL4_C, LSM6DSOX_CTRL4_C_I2C_DIS))
        return false;
    if(!reg_write(dev, LSM6DSOX_REG_CTRL3_C, LSM6DSOX_CTRL3_C_BDU | LSM6DSOX_CTRL3_C_IF_INC))
        return false;

    dev->fail_step = 0;
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
    dev->configured = false;
}

/* Código do registrador e sensibilidade de cada faixa (DS12814, tabela 2:
   0,061 / 0,122 / 0,244 / 0,488 mg/LSB e 4,375 / 8,75 / 17,5 / 35 / 70 mdps/LSB). */
static const struct
{
    uint8_t code;
    float g_per_lsb;
} xl_fs_tab[] = {
    [LSM6DSOX_XL_2G] = { 0x0, 0.061e-3f },
    [LSM6DSOX_XL_4G] = { 0x2, 0.122e-3f },
    [LSM6DSOX_XL_8G] = { 0x3, 0.244e-3f },
    [LSM6DSOX_XL_16G] = { 0x1, 0.488e-3f },
};

static const struct
{
    uint8_t bits; /* FS_G[3:2] e FS_125[1] já posicionados */
    float dps_per_lsb;
} g_fs_tab[] = {
    [LSM6DSOX_G_125DPS] = { 0x02, 4.375e-3f },
    [LSM6DSOX_G_250DPS] = { 0x00, 8.75e-3f },
    [LSM6DSOX_G_500DPS] = { 0x04, 17.5e-3f },
    [LSM6DSOX_G_1000DPS] = { 0x08, 35.0e-3f },
    [LSM6DSOX_G_2000DPS] = { 0x0C, 70.0e-3f },
};

bool lsm6dsox_setup(lsm6dsox_t* dev, uint8_t odr, lsm6dsox_xl_fs_t xl_fs, lsm6dsox_g_fs_t g_fs, bool drdy_int1)
{
    uint8_t ctrl1;
    uint8_t ctrl2;

    if(!dev->initialized || odr > LSM6DSOX_ODR_MAX || (uint32_t) xl_fs > LSM6DSOX_XL_16G ||
       (uint32_t) g_fs > LSM6DSOX_G_2000DPS)
        return false;

    dev->configured = false;

    ctrl1 = (uint8_t) ((odr << 4) | (xl_fs_tab[xl_fs].code << 2));
    ctrl2 = (uint8_t) ((odr << 4) | g_fs_tab[g_fs].bits);

    /* Data-ready travado (padrão): o INT1 fica alto até a leitura. No modo
       pulsado (75 µs) o datalogger perdia bordas com a IMU em jumpers; travado,
       o sinal tem tempo de sobra para ser visto. Se uma leitura falhar, o pino
       fica alto e não há borda nova: a app detecta e gera a interrupção por
       software (hal_gpio_interrupt_trigger). */
    if(!reg_write(dev, LSM6DSOX_REG_COUNTER_BDR_REG1, 0))
        return false;
    if(!reg_write(dev, LSM6DSOX_REG_INT1_CTRL, drdy_int1 ? LSM6DSOX_INT1_DRDY_XL : 0))
        return false;
    if(!reg_write(dev, LSM6DSOX_REG_CTRL1_XL, ctrl1))
        return false;
    if(!reg_write(dev, LSM6DSOX_REG_CTRL2_G, ctrl2))
        return false;

    dev->accel_scale = xl_fs_tab[xl_fs].g_per_lsb;
    dev->gyro_scale = g_fs_tab[g_fs].dps_per_lsb;
    dev->configured = true;

    return true;
}

bool lsm6dsox_raw_read(lsm6dsox_t* dev, lsm6dsox_raw_t* accel, lsm6dsox_raw_t* gyro)
{
    uint8_t b[12];

    if(!dev->configured)
        return false;

    /* Com IF_INC e BDU ligados no init, uma leitura de 0x22 a 0x2D traz gyro
       e acc da mesma amostra (little-endian, X/Y/Z). */
    if(!dev->bus->read(LSM6DSOX_REG_OUTX_L_G, b, sizeof(b)))
        return false;

    gyro->x = (int16_t) ((uint16_t) b[1] << 8 | b[0]);
    gyro->y = (int16_t) ((uint16_t) b[3] << 8 | b[2]);
    gyro->z = (int16_t) ((uint16_t) b[5] << 8 | b[4]);
    accel->x = (int16_t) ((uint16_t) b[7] << 8 | b[6]);
    accel->y = (int16_t) ((uint16_t) b[9] << 8 | b[8]);
    accel->z = (int16_t) ((uint16_t) b[11] << 8 | b[10]);

    return true;
}
