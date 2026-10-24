#include "bme280.h"

#define BME280_RESET_TIMEOUT_MS 10U

/* Valor que os registradores de dados mantêm até a primeira conversão
   (ou com o canal desligado): 0x80000 para T e P, 0x8000 para H. */
#define BME280_ADC_TP_INVALID 0x80000
#define BME280_ADC_H_INVALID  0x8000

static bool reg_write(bme280_t* dev, uint8_t reg, uint8_t val)
{
    return dev->bus->write(dev->addr, reg, &val, 1);
}

static uint16_t u16_le(const uint8_t* b)
{
    return (uint16_t) ((uint16_t) b[1] << 8 | b[0]);
}

static int16_t s16_le(const uint8_t* b)
{
    return (int16_t) u16_le(b);
}

static bool bme280_probe(bme280_t* dev, uint8_t addr)
{
    uint8_t id = 0;

    if(!dev->bus->read(addr, BME280_REG_CHIP_ID, &id, 1))
        return false;

    dev->addr = addr;
    dev->chip_id = id;

    return true;
}

/* Lê os dois blocos de calibração (DS tabela 16). H4 e H5 são de 12 bits com
   sinal e dividem o registrador 0xE5. */
static bool bme280_calib_read(bme280_t* dev)
{
    uint8_t a[BME280_CALIB00_LEN];
    uint8_t b[BME280_CALIB26_LEN];
    bme280_calib_t* c = &dev->calib;

    if(!dev->bus->read(dev->addr, BME280_REG_CALIB00, a, sizeof(a)))
        return false;
    if(!dev->bus->read(dev->addr, BME280_REG_CALIB26, b, sizeof(b)))
        return false;

    c->t1 = u16_le(&a[0]);
    c->t2 = s16_le(&a[2]);
    c->t3 = s16_le(&a[4]);
    c->p1 = u16_le(&a[6]);
    c->p2 = s16_le(&a[8]);
    c->p3 = s16_le(&a[10]);
    c->p4 = s16_le(&a[12]);
    c->p5 = s16_le(&a[14]);
    c->p6 = s16_le(&a[16]);
    c->p7 = s16_le(&a[18]);
    c->p8 = s16_le(&a[20]);
    c->p9 = s16_le(&a[22]);
    c->h1 = a[25]; /* 0xA1; a[24] (0xA0) não é usado */

    c->h2 = s16_le(&b[0]);                                              /* 0xE1/0xE2 */
    c->h3 = b[2];                                                       /* 0xE3 */
    c->h4 = (int16_t) ((int16_t) (int8_t) b[3] * 16 | (b[4] & 0x0F));   /* 0xE4[7:0] 0xE5[3:0] */
    c->h5 = (int16_t) ((int16_t) (int8_t) b[5] * 16 | (b[4] >> 4));     /* 0xE6[7:0] 0xE5[7:4] */
    c->h6 = (int8_t) b[6];                                              /* 0xE7 */

    /* T1 e P1 nunca são zero num chip calibrado; zero indica leitura ruim. */
    return c->t1 != 0 && c->p1 != 0;
}

bool bme280_init(bme280_t* dev, const bme280_bus_t* bus)
{
    uint8_t status = 0;
    bool ready = false;

    dev->bus = bus;
    dev->addr = 0;
    dev->chip_id = 0;
    dev->initialized = false;
    dev->configured = false;

    if(!bme280_probe(dev, BME280_I2C_ADDR_PRIMARY) && !bme280_probe(dev, BME280_I2C_ADDR_SECONDARY))
        return false;

    if(dev->chip_id != BME280_CHIP_ID_VALUE)
        return false; /* 0x58 = BMP280 (sem umidade) */

    if(!reg_write(dev, BME280_REG_RESET, BME280_RESET_CMD))
        return false;

    /* Após o reset o chip copia a calibração da NVM; espera im_update = 0. */
    for(uint32_t t = 0; t < BME280_RESET_TIMEOUT_MS && !ready; t++)
    {
        bus->delay_ms(1);
        if(bus->read(dev->addr, BME280_REG_STATUS, &status, 1) && (status & BME280_STATUS_IM_UPDATE) == 0U)
            ready = true;
    }

    if(!ready || !bme280_calib_read(dev))
        return false;

    dev->initialized = true;

    return true;
}

void bme280_deinit(bme280_t* dev)
{
    if(dev->initialized)
        (void) reg_write(dev, BME280_REG_CTRL_MEAS, BME280_MODE_SLEEP);

    dev->initialized = false;
    dev->configured = false;
}

bool bme280_setup(bme280_t* dev, uint8_t osr_t, uint8_t osr_p, uint8_t osr_h, uint8_t filter,
                  bme280_standby_t standby)
{
    if(!dev->initialized || osr_t > BME280_OSR_MAX || osr_p > BME280_OSR_MAX || osr_h > BME280_OSR_MAX ||
       filter > BME280_FILTER_MAX || (uint32_t) standby > BME280_STANDBY_20MS)
        return false;

    dev->configured = false;

    /* O config só é aceito com certeza em sleep, e o ctrl_hum só vale depois
       de uma escrita no ctrl_meas (DS 5.4.3 e 5.4.6). Por isso a ordem:
       sleep -> ctrl_hum -> config -> ctrl_meas com o modo normal. */
    if(!reg_write(dev, BME280_REG_CTRL_MEAS, BME280_MODE_SLEEP))
        return false;
    if(!reg_write(dev, BME280_REG_CTRL_HUM, osr_h & 0x07U))
        return false;
    if(!reg_write(dev, BME280_REG_CONFIG, (uint8_t) (((uint8_t) standby << 5) | ((filter & 0x07U) << 2))))
        return false;
    if(!reg_write(dev, BME280_REG_CTRL_MEAS,
                  (uint8_t) (((osr_t & 0x07U) << 5) | ((osr_p & 0x07U) << 2) | BME280_MODE_NORMAL)))
        return false;

    dev->configured = true;

    return true;
}

/* ----------------------------------------------------------------------
 * Compensação inteira (DS 4.2.3). Deslocamentos à esquerda de valores que
 * podem ser negativos foram trocados por multiplicações (mesmo resultado,
 * sem comportamento indefinido em C). t_fine sai da temperatura e entra na
 * pressão e na umidade, então a temperatura é sempre calculada primeiro.
 * ---------------------------------------------------------------------- */

static int32_t bme280_comp_temp(const bme280_calib_t* c, int32_t adc_t, int32_t* t_fine)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t) c->t1 << 1))) * (int32_t) c->t2) >> 11;
    int32_t var2 = (((((adc_t >> 4) - (int32_t) c->t1) * ((adc_t >> 4) - (int32_t) c->t1)) >> 12) *
                    (int32_t) c->t3) >> 14;

    *t_fine = var1 + var2;

    return (*t_fine * 5 + 128) >> 8; /* 0,01 °C */
}

static uint32_t bme280_comp_press(const bme280_calib_t* c, int32_t adc_p, int32_t t_fine)
{
    int64_t var1 = (int64_t) t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t) c->p6;
    int64_t p;

    var2 = var2 + (var1 * (int64_t) c->p5) * 131072;
    var2 = var2 + (int64_t) c->p4 * 34359738368LL;
    var1 = ((var1 * var1 * (int64_t) c->p3) >> 8) + (var1 * (int64_t) c->p2) * 4096;
    var1 = ((((int64_t) 1 << 47) + var1) * (int64_t) c->p1) >> 33;

    if(var1 == 0)
        return 0; /* evita divisão por zero */

    p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t) c->p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t) c->p8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (int64_t) c->p7 * 16;

    return (uint32_t) p; /* Pa em Q24.8 */
}

static uint32_t bme280_comp_hum(const bme280_calib_t* c, int32_t adc_h, int32_t t_fine)
{
    int32_t v = t_fine - 76800;

    v = (((((adc_h << 14) - (int32_t) c->h4 * 1048576 - ((int32_t) c->h5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t) c->h6) >> 10) * (((v * (int32_t) c->h3) >> 11) + 32768)) >> 10) + 2097152) *
               (int32_t) c->h2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t) c->h1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;

    return (uint32_t) (v >> 12); /* %UR em Q22.10 */
}

bool bme280_read(bme280_t* dev, bme280_data_t* data)
{
    uint8_t b[BME280_DATA_LEN];
    int32_t adc_p;
    int32_t adc_t;
    int32_t adc_h;
    int32_t t_fine;

    if(!dev->initialized || !dev->configured)
        return false;

    /* Uma leitura só, de 0xF7 a 0xFE: os registradores ficam congelados
       durante a rajada, então T, P e H são da mesma medição (DS 4). */
    if(!dev->bus->read(dev->addr, BME280_REG_DATA, b, sizeof(b)))
        return false;

    adc_p = (int32_t) (((uint32_t) b[0] << 12) | ((uint32_t) b[1] << 4) | (b[2] >> 4));
    adc_t = (int32_t) (((uint32_t) b[3] << 12) | ((uint32_t) b[4] << 4) | (b[5] >> 4));
    adc_h = (int32_t) (((uint32_t) b[6] << 8) | b[7]);

    if(adc_t == BME280_ADC_TP_INVALID)
        return false; /* primeira conversão ainda não terminou */

    data->temp_c100 = bme280_comp_temp(&dev->calib, adc_t, &t_fine);
    data->press_pa256 = adc_p == BME280_ADC_TP_INVALID ? 0 : bme280_comp_press(&dev->calib, adc_p, t_fine);
    data->hum_rh1024 = adc_h == BME280_ADC_H_INVALID ? 0 : bme280_comp_hum(&dev->calib, adc_h, t_fine);

    return true;
}
