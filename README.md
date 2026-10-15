# Datalogger Ambiental e Inercial — STM32U5A5

Datalogger para o payload de um foguete: registra **temperatura, umidade e pressão** (BME280, I2C) e **aceleração e velocidade angular** (LSM6DSOX, SPI) durante todo o voo, gravando os dados em **CSV num cartão microSD** (SDMMC 4 bits + FatFs).

Projeto desenvolvido para o **Desafio do Processo Seletivo da Payload — EPTA (Equipe de Propulsão e Tecnologia Aeroespacial, UFU)**. O enunciado está em [`docs/Desafio_Payload_Datalogger_STM32.pdf`](docs/Desafio_Payload_Datalogger_STM32.pdf).

> 🚧 **Em desenvolvimento.** Entrega prevista para 24/10/2026.

---

## Visão geral

| Bloco | Componente | Interface |
|---|---|---|
| MCU | STM32U5A5 (Cortex-M33, 160 MHz) | — |
| Temperatura, umidade e pressão | BME280 | I2C |
| IMU (acelerômetro + giroscópio) | LSM6DSOX | SPI + INT1 (data-ready) |
| Armazenamento | Slot microSD | SDMMC 4 bits + FatFs |
| Energia | Bateria LiPo 1S + USB-C, seleção automática, buck-boost 3,3 V (TPS63001) | — |
| Depuração | Conector SWD | SWDIO / SWCLK |

- **Protótipo:** placa NUCLEO-U5A5ZJ-Q com os sensores em protoboard.
- **PCB:** projeto próprio no Altium Designer, 4 camadas, com o STM32U5A5RJT6Q (LQFP64). É a mesma família da Nucleo e usa os mesmos pinos, então o firmware é o mesmo nos dois.

## Estrutura do repositório

```
├── fw-datalogger/datalogger/   Projeto STM32CubeIDE (STM32Cube FW_U5 V1.8.0)
├── hw-datalogger/datalogger/   Projeto Altium Designer: esquemático e PCB
├── docs/                         Enunciado, referências, diagramas, fotos e testes
├── scripts/                      Análise dos CSVs em Python
└── README.md
```

## Pinagem

Os mesmos pinos são usados na Nucleo (STM32U5A5ZJT6Q, LQFP144) e na PCB (STM32U5A5RJT6Q, LQFP64).

| Função | Pinos | Observação na Nucleo |
|---|---|---|
| SDMMC1 (microSD) | PC8 D0, PC9 D1, PC10 D2, PC11 D3, PC12 CK, PD2 CMD | conector CN8 |
| Card detect | PA8 | |
| SPI1 → LSM6DSOX | PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS | CN7 D13/D12/D11 |
| LSM6DSOX INT1 / INT2 | PB0 / PB2 | |
| I2C3 → BME280 | PC0 SCL, PC1 SDA | CN9 A5/A4 |
| USB | PA11 DM, PA12 DP | USB-C da Nucleo (CN15) |
| SWD | PA13 SWDIO, PA14 SWCLK, PB3 SWO | ST-LINK |
| UART de debug | USART1: PA9 TX, PA10 RX | Virtual COM do ST-LINK |
| Cristais | PH0/PH1 HSE 16 MHz, PC14/PC15 LSE 32,768 kHz | |
| LED status / erro | PC7 / PB7 | LD1 verde / LD2 azul |
| Botão | PC13 | B1 |
| Tensão da bateria | PA0 (ADC, com divisor) | |

## Formato do CSV

_A definir._ Previsão: `timestamp_ms, ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, temp_C, umid_pct, press_hPa`.

## Build e gravação

**Requisitos:** STM32CubeIDE (testado com STM32Cube FW_U5 V1.8.0) e a placa NUCLEO-U5A5ZJ-Q.

1. Clone o repositório:
   ```bash
   git clone https://github.com/carlosoliveirajesus/datalogger.git
   ```
2. No STM32CubeIDE: **File → Import → Existing Projects into Workspace** e selecione `fw-datalogger/datalogger`.
3. **Project → Build Project**.
4. Conecte a Nucleo pela porta USB do ST-LINK e use **Run → Debug** (ou **Run**) para gravar.

## Status

- [x] Repositório e estrutura inicial
- [ ] Firmware: drivers do LSM6DSOX e do BME280
- [ ] Firmware: SDMMC + FatFs + gravação de CSV
- [ ] Firmware: tratamento de erros (cartão ausente, cartão cheio, falha de sensor)
- [ ] Hardware: esquemático (ERC limpo)
- [ ] Hardware: layout da PCB (DRC limpo, Gerbers, BOM, posicionamento)
- [ ] Validação na Nucleo
- [ ] Documentação e apresentação final

## Referências

Datasheets e manuais utilizados: [`docs/referencias.md`](docs/referencias.md).

## Autor

Carlos Eduardo Oliveira de Jesus
