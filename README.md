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
| Depuração | Conector SWD (6 pinos) + UART de debug (4 pinos) | SWDIO / SWCLK / SWO, USART1 |

- **Protótipo:** placa NUCLEO-U5A5ZJ-Q com os sensores em protoboard.
- **PCB:** projeto próprio no Altium Designer, 4 camadas, com o STM32U5A5RJT6Q (LQFP64). É a mesma família da Nucleo e usa os mesmos pinos, então o firmware é o mesmo nos dois.

## Estrutura do repositório

```
├── fw-datalogger/datalogger/   Projeto STM32CubeIDE (STM32Cube FW_U5 V1.8.0)
├── hw-datalogger/datalogger/   Projeto Altium Designer: esquemático, PCB e OutJob
├── hw-datalogger/fabricacao/   Saídas de fabricação: Gerbers/furação (JLCPCB), BOM, posicionamento, STEP, PDF
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
| LSM6DSOX INT1 / INT2 | PB1 / PB2 | (PB0 não existe no LQFP64 com SMPS) |
| I2C3 → BME280 | PC0 SCL, PC1 SDA | CN9 A5/A4 |
| USB | PA11 DM, PA12 DP | USB-C da Nucleo (CN15) |
| SWD | PA13 SWDIO, PA14 SWCLK, PB3 SWO | ST-LINK |
| UART de debug | USART1: PA9 TX, PA10 RX | Virtual COM do ST-LINK |
| Cristais | PH0/PH1 HSE 16 MHz, PC14/PC15 LSE 32,768 kHz | |
| LED status / erro | PC7 / PB7 | LD1 verde / LD2 azul (na PCB: azul / vermelho) |
| Botão | PC13 | B1 |
| Tensão da bateria | PA0 (ADC, com divisor) | |

## Hardware (PCB)

Projeto no Altium Designer em `hw-datalogger/datalogger/` (folhas hierárquicas `00_Top`, `01_Power`, `02_MCU`, `03_Sensors`, `04_microSD`, `05_Interface`). Esquemático completo em PDF: [`hw-datalogger/fabricacao/dataloggerOutJob.PDF`](hw-datalogger/fabricacao/dataloggerOutJob.PDF).

| Vista 3D — Top | Vista 3D — Bottom |
|---|---|
| ![PCB top](docs/img/pcb_top.png) | ![PCB bottom](docs/img/pcb_bottom.png) |

### Resumo da placa

| Item | Valor |
|---|---|
| Dimensões | 50,8 × 64,8 mm, cantos arredondados, 4 furos M3 (ligados ao GND) |
| Camadas | 4 — Top (sinal) / **GND** (plano) / **3V3** (plano) / Bottom (sinal) |
| Empilhamento | JLCPCB JLC04161H-2313, 1,6 mm, cobre 1 oz externo / 0,5 oz interno |
| Montagem | Todos os componentes no Top (montagem em um lado só) |
| Passivos | 0603 (capacitores dos cristais em 0402, C0G) |
| Regras (mil) | Trilha/folga 6/6 (sinal 10, alimentação ≥ 20), via 24/12 *tented*, anel ≥ 6, furo-furo ≥ 20, cobre-borda ≥ 12 |

### Decisões de projeto

- **Alimentação:** bateria LiPo 1S (JST-PH) ou USB-C, sem carregador na placa. Um MOSFET P (DMG2305UX) faz a função de diodo ideal para a bateria e o Schottky B5819W leva o VBUS, com prioridade para a USB quando ela está conectada. O conector USB-C tem 5,1 kΩ nos CC1/CC2 e proteção ESD (USBLC6-2SC6).
- **Regulação de 3,3 V com buck-boost (TPS63001):** a LiPo varia de 4,2 V a 3,0 V e passa abaixo de 3,3 V; um buck-boost mantém os 3,3 V em toda a descarga e também a partir dos 5 V da USB. A chave liga/desliga atua no EN do regulador.
- **Layout do regulador** segundo o exemplo do datasheet: indutor colado nos pinos L1/L2, nós de chaveamento com polígonos pequenos, capacitores de entrada e saída junto aos pinos, ligação direta (sem alívio térmico) e vias no PowerPAD.
- **SMPS interno do STM32U5:** indutor de 2,2 µH (LQM21PN2R2MGHL, o mesmo da Nucleo), 2 × 2,2 µF nos pinos VDD11 (ligados entre si) e 10 µF + 100 nF no VDDSMPS. Os laços VDDSMPS → VLX → indutor → VDD11 ficaram todos no Top, sem vias, com o 100 nF colado no pino.
- **Desacoplamento:** um 100 nF por pino de alimentação, com via de GND e de 3V3 no lado de fora do capacitor (pino → capacitor → via). VDDA filtrado por ferrite (BLM18PG221) com 1 µF + 100 nF.
- **Clock:** HSE de 16 MHz (ABM11W, CL 8 pF, 2 × 10 pF) e LSE de 32,768 kHz (ABS07, CL 7 pF, 2 × 8,2 pF), colados no MCU e sem trilhas passando por baixo.
- **Posicionamento:** o MCU foi girado para cada bloco ficar de frente para os seus pinos (cristais e I2C em cima, SPI/IMU à esquerda, USB e microSD embaixo/direita). Entrada de energia e regulador agrupados numa borda.
- **BME280 isolado do calor:** no lado oposto ao regulador, perto da borda, com recorte nos polígonos de cobre do Top e do Bottom em volta dele.
- **IMU (LSM6DSOX):** alinhada às bordas da placa, perto do MCU (SPI curto) e longe do regulador. A posição em relação ao centro da placa permite corrigir o braço de alavanca no pós-processamento.
- **USB:** D+/D− roteados como par diferencial (90 Ω nominais, Full Speed) do conector ao ESD e ao MCU, no Top e sobre o plano de GND. As duplicatas de D+/D− do USB-C são unidas junto ao conector (uma ponte curta no Bottom).
- **microSD (SDMMC 4 bits):** pull-ups de 47 kΩ em CMD e D0–D3, 33 Ω em série no CLK junto ao MCU, card detect em PA8, trilhas curtas sobre o GND.
- **GND:** planos internos contínuos (nenhuma trilha nas camadas internas), polígonos de GND no Top e no Bottom e costura de vias.

### Exceções de ERC e DRC (justificadas)

| Regra | Onde | Justificativa |
|---|---|---|
| ERC: *Nets with no driving source* → No Report | `ENB_REG`, `NRST` | São acionadas por componentes passivos (chave, botão e pull-up interno do MCU). |
| ERC: *Nets with multiple names* → No Report | rede 3V3 | Rótulos de desacoplamento por pino (`MCU_VDD_A/B/C…`) na mesma rede; *Power Port Names Take Priority* garante o nome 3V3 no PCB. |
| ERC: *I/O Pin × I/O Pin* → No Report | SPI, I2C, USB | Pinos bidirecionais por definição nos símbolos. |
| DRC: via 18/8 mil (anel 5 mil) | 2 vias de `USB_DM_NET` | Ponte do D− junto ao USB-C; dentro do mínimo da JLCPCB (0,2/0,45 mm). |
| DRC: *Minimum Solder Mask Sliver* = 0 | U1, U3, U4, U5, J1 | Componentes de passo 0,5 mm: abertura única de máscara na fileira de pads (padrão do fabricante). |
| DRC: folga pad-pad ignorada dentro do footprint | LSM6DSOX (LGA) | Pads do próprio fabricante com 0,15 mm de folga. |
| DRC: *Testpoint Usage* desligado | toda a placa | Sem teste elétrico automatizado. |

Resultado final: **ERC sem erros e DRC com 0 violações.**

### Fabricação (JLCPCB)

Os arquivos ficam em [`hw-datalogger/fabricacao/`](hw-datalogger/fabricacao/), gerados pelo `dataloggerOutJob.OutJob`:

- `JLCPCB/` e `JLCPCB.zip`: Gerbers (GTL, GP1, GP2, GBL, máscaras, serigrafias, pastas, contorno GM1) e furação NC Drill (PTH/NPTH e fendas).
- `BOM/`: lista de materiais com MPN (também em [`docs/datalogger.xlsx`](docs/datalogger.xlsx)).
- `Pick Place/`: posicionamento dos componentes.
- `ExportSTEP/`: modelo 3D da placa montada.

| Opção do pedido | Valor |
|---|---|
| Layers | 4 |
| Espessura | 1,6 mm |
| Stackup | JLC04161H-2313 — L1 GTL, L2 GP1 (GND), L3 GP2 (3V3), L4 GBL |
| Cobre externo / interno | 1 oz / 0,5 oz |
| Via covering | Tented |
| Acabamento | ENIG (recomendado pelos pads de passo fino) ou HASL sem chumbo |

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
- [x] Hardware: esquemático (ERC limpo)
- [x] Hardware: layout da PCB (DRC limpo, Gerbers, BOM, posicionamento)
- [ ] Validação na Nucleo
- [ ] Documentação e apresentação final

## Referências

Datasheets e manuais utilizados: [`docs/referencias.md`](docs/referencias.md).

## Autor

Carlos Eduardo Oliveira de Jesus
