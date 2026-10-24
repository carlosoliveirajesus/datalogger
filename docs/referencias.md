# Referências

Datasheets, manuais, especificações e bibliotecas usados no projeto. Os PDFs ficam localmente em `docs/datasheets/` (fora do Git); aqui estão os links das fontes originais.

## Microcontrolador e placa de desenvolvimento
- **STM32U5A5xx — Datasheet (DS)**: https://www.st.com/resource/en/datasheet/stm32u5a5rj.pdf
- **RM0456 — STM32U5 Reference Manual**: https://www.st.com/resource/en/reference_manual/rm0456-stm32u5-series-armbased-32bit-mcus-stmicroelectronics.pdf
  - Usado no firmware: SDMMC (inicialização do cartão, modo 1/4 bits, controle de fluxo), EXTI (interrupção por software via `EXTI_SWIER1`), I2C, NVIC e SysTick.
- **ES0553 — Errata STM32U59x/U5Ax**: https://www.st.com/resource/en/errata_sheet/es0553-stm32u59xxx-and-stm32u5axxx-device-errata-stmicroelectronics.pdf
- **UM2861 — Manual da placa STM32U5 Nucleo-144 (MB1549)**: https://www.st.com/resource/en/user_manual/um2861-stm32u5-nucleo144-board-mb1549-stmicroelectronics.pdf
- **NUCLEO-U5A5ZJ-Q — Data brief**: https://www.st.com/resource/en/data_brief/nucleo-u5a5zj-q.pdf
- **Esquemático MB1549 (Nucleo-U5A5ZJ-Q)**: página da placa no site da ST, aba "CAD Resources": https://www.st.com/en/evaluation-tools/nucleo-u5a5zj-q.html

## Sensores
- **LSM6DSOX — Datasheet (ST)**: https://www.st.com/resource/en/datasheet/lsm6dsox.pdf (página do produto: https://www.st.com/en/mems-and-sensors/lsm6dsox.html)
  - Usado no driver `port/stm32u5a5/imu/lsm6dsox.c`: mapa de registradores (WHO_AM_I 0x6C, CTRL1_XL, CTRL2_G, CTRL3_C, CTRL4_C, INT1_CTRL, COUNTER_BDR_REG1), ODR, faixas e sensibilidades (mg/LSB e mdps/LSB), reset por software, BDU/IF_INC, data-ready travado/pulsado, endereço I2C pelo SA0 (0x6A/0x6B).
- **BME280 — Datasheet (Bosch Sensortec, BST-BME280-DS002)**: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf (página do produto: https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)
  - Usado no driver `port/stm32u5a5/env/bme280.c`: chip ID 0x60, calibração (0x88–0xA1 e 0xE1–0xE7), fórmulas de compensação inteiras (seção 4.2.3 e apêndice), modo normal, oversampling, filtro IIR e tempo de medição (seção 9.1).

## Armazenamento
- **SD Specifications Part 1 — Physical Layer Simplified Specification (SD Association)**: https://www.sdcard.org/downloads/pls/
  - Sequência de inicialização (CMD0, CMD8, ACMD41), leitura e escrita por blocos de 512 B, modo de 1 e 4 bits.
- **Adafruit microSD Card BFF** (módulo usado no protótipo): https://www.adafruit.com/product/5683 — pinagem e esquemático: https://learn.adafruit.com/adafruit-microsd-card-bff

## Barramento I2C
- **UM10204 — I2C-bus specification and user manual (NXP)**: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
  - Seção "Bus clear": até 9 pulsos de SCL com o SDA preso em 0, usados na recuperação do barramento (`port_env_i2c_recover`, `imu_bus_recover`).

## Software e bibliotecas
| Biblioteca | Uso no projeto | Licença | Fonte |
|---|---|---|---|
| STM32CubeU5 (HAL, CMSIS, CubeMX) — FW_U5 V1.8.0 | código gerado (`Core/`, `Drivers/`) e base do `port/stm32u5a5/` | BSD-3-Clause / Apache-2.0 | https://www.st.com/en/embedded-software/stm32cubeu5.html — https://github.com/STMicroelectronics/STM32CubeU5 (guia: UM2883 https://www.st.com/resource/en/user_manual/um2883-getting-started-with-stm32cubeu5-for-stm32u5-series-stmicroelectronics.pdf) |
| FatFs R0.16 (ChaN), pacote da ST | sistema de arquivos FAT32 no cartão (`utl/fatfs/`, com `diskio.c` próprio sobre `hal_sd`) | BSD-3-Clause (`utl/fatfs/LICENSE.md`, ChaN e ST) | http://elm-chan.org/fsw/ff/ — https://github.com/STMicroelectronics/stm32_mw_fatfs |
| printf (Marco Paland / Eyal Rozenberg) | `utl_printf` / `utl_snprintf` do terminal de debug (`utl/printf/`) | MIT | https://github.com/eyalroz/printf |

Os drivers do LSM6DSOX e do BME280 foram escritos do zero a partir dos datasheets acima. As bibliotecas oficiais (ST `lsm6dsox-pid` e Bosch `BME280_SensorAPI`) serviram só como conferência:
- https://github.com/STMicroelectronics/lsm6dsox-pid
- https://github.com/boschsensortec/BME280_SensorAPI
