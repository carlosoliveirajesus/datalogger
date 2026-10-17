/**
 * @file bme280.h
 * @brief Driver do BME280 (Bosch), independente do barramento.
 *
 * Referência: datasheet BME280 BST-BME280-DS002.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define BME280_I2C_ADDR_PRIMARY   0x76 /* SDO em GND */
#define BME280_I2C_ADDR_SECONDARY 0x77 /* SDO em VDDIO */

#define BME280_CHIP_ID_VALUE 0x60
#define BMP280_CHIP_ID_VALUE 0x58 /* módulo vendido como BME mas sem umidade */

#define BME280_REG_CHIP_ID 0xD0
#define BME280_REG_RESET   0xE0
#define BME280_REG_STATUS  0xF3

#define BME280_RESET_CMD         0xB6
#define BME280_STATUS_IM_UPDATE  (1U << 0)

typedef struct bme280_bus_s
{
    bool (*read)(uint8_t addr, uint8_t reg, uint8_t* data, uint16_t len);
    bool (*write)(uint8_t addr, uint8_t reg, const uint8_t* data, uint16_t len);
    void (*delay_ms)(uint32_t ms);
} bme280_bus_t;

typedef struct bme280_s
{
    const bme280_bus_t* bus;
    uint8_t addr;    /**< endereço I2C de 7 bits detectado */
    uint8_t chip_id;
    bool initialized;
} bme280_t;

/** Procura o chip em 0x76 e 0x77, confere o chip ID e faz reset. */
bool bme280_init(bme280_t* dev, const bme280_bus_t* bus);
void bme280_deinit(bme280_t* dev);
