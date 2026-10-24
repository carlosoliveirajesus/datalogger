/**
 * @file bme280.h
 * @brief Driver do BME280 (Bosch), independente do barramento.
 *
 * Referência: datasheet BME280 BST-BME280-DS002 (rev. 1.6 ou posterior):
 * mapa de registradores (5.3), calibração (4.2.2) e compensação inteira
 * (4.2.3 e 8.2). A compensação foi reescrita a partir das fórmulas do
 * datasheet, sem usar a biblioteca da Bosch.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define BME280_I2C_ADDR_PRIMARY   0x76 /* SDO em GND */
#define BME280_I2C_ADDR_SECONDARY 0x77 /* SDO em VDDIO */

#define BME280_CHIP_ID_VALUE 0x60
#define BMP280_CHIP_ID_VALUE 0x58 /* módulo vendido como BME mas sem umidade */

/* Registradores (DS 5.3). */
#define BME280_REG_CALIB00   0x88 /* 0x88..0xA1: T1..T3, P1..P9, H1 */
#define BME280_REG_CHIP_ID   0xD0
#define BME280_REG_RESET     0xE0
#define BME280_REG_CALIB26   0xE1 /* 0xE1..0xE7: H2..H6 */
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_DATA      0xF7 /* 0xF7..0xFE: press, temp, hum */

#define BME280_CALIB00_LEN 26
#define BME280_CALIB26_LEN 7
#define BME280_DATA_LEN    8

#define BME280_RESET_CMD        0xB6
#define BME280_STATUS_IM_UPDATE (1U << 0)
#define BME280_MODE_SLEEP       0x00
#define BME280_MODE_NORMAL      0x03

/* Oversampling (osrs_x): 0 = canal desligado, 1..5 = x1, x2, x4, x8, x16. */
#define BME280_OSR_SKIP 0
#define BME280_OSR_MAX  5

/* Filtro IIR (config.filter): 0 = off, 1..4 = coeficiente 2, 4, 8, 16. */
#define BME280_FILTER_MAX 4

/* Tempo entre medições no modo normal (config.t_sb), DS tabela 27. */
typedef enum bme280_standby_e
{
    BME280_STANDBY_0MS5 = 0,
    BME280_STANDBY_62MS5,
    BME280_STANDBY_125MS,
    BME280_STANDBY_250MS,
    BME280_STANDBY_500MS,
    BME280_STANDBY_1000MS,
    BME280_STANDBY_10MS,
    BME280_STANDBY_20MS,
} bme280_standby_t;

typedef struct bme280_bus_s
{
    bool (*read)(uint8_t addr, uint8_t reg, uint8_t* data, uint16_t len);
    bool (*write)(uint8_t addr, uint8_t reg, const uint8_t* data, uint16_t len);
    void (*delay_ms)(uint32_t ms);
} bme280_bus_t;

/* Coeficientes de calibração gravados na NVM do chip (DS tabela 16). */
typedef struct bme280_calib_s
{
    uint16_t t1;
    int16_t t2;
    int16_t t3;
    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;
    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4;
    int16_t h5;
    int8_t h6;
} bme280_calib_t;

typedef struct bme280_s
{
    const bme280_bus_t* bus;
    bme280_calib_t calib;
    uint8_t addr;    /**< endereço I2C de 7 bits detectado */
    uint8_t chip_id;
    bool initialized;
    bool configured;
} bme280_t;

/** Medição compensada, em inteiros (sem ponto flutuante). */
typedef struct bme280_data_s
{
    int32_t temp_c100;    /**< temperatura em 0,01 °C   */
    uint32_t press_pa256; /**< pressão em Pa / 256 (Q24.8) */
    uint32_t hum_rh1024;  /**< umidade em %UR / 1024 (Q22.10) */
} bme280_data_t;

/** Procura o chip em 0x76 e 0x77, confere o chip ID, faz reset e lê a calibração. */
bool bme280_init(bme280_t* dev, const bme280_bus_t* bus);
void bme280_deinit(bme280_t* dev);

/**
 * Configura e põe o chip em modo normal (medição contínua).
 * @param osr_t, osr_p, osr_h  oversampling, 0..5 (ver BME280_OSR_*)
 * @param filter               filtro IIR, 0..4
 * @param standby              tempo entre medições
 */
bool bme280_setup(bme280_t* dev, uint8_t osr_t, uint8_t osr_p, uint8_t osr_h, uint8_t filter,
                  bme280_standby_t standby);

/** Lê a última medição (leitura em rajada 0xF7..0xFE) e compensa. */
bool bme280_read(bme280_t* dev, bme280_data_t* data);
