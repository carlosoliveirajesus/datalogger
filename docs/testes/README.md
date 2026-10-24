# Testes na Nucleo

Arquivos reais gravados pelo datalogger no protótipo: NUCLEO-U5A5ZJ-Q, LSM6DSOX em I2C, BME280 em I2C e cartão microSD de 16 GB (FAT32) num Adafruit microSD Card BFF em SDMMC 1 bit. A análise está resumida na seção **Testes** e em **Problemas conhecidos** do [README principal](../../README.md).

- `csv/`: os arquivos `LOG_nnnn.CSV` copiados do cartão, sem nenhuma edição.
- `serial/`: o log da UART de debug (USART1, 115200 8N1) gravado junto com cada CSV.
- `bancada/`: logs da UART da fase de bring-up, sem CSV associado.
- `graficos/`: um PNG por CSV e o `resumo.txt`, gerados por [`scripts/plot_log.py`](../../scripts/plot_log.py) (`python plot_log.py ../docs/testes/csv/*.CSV -o ../docs/testes/graficos`).

As versões de firmware mudaram entre os testes. Por isso o formato das linhas do terminal e alguns comportamentos (por exemplo, o data-ready pulsado do `LOG_0002`) não são os mesmos em todos os arquivos.

## Gravações (`csv/` + `serial/`)

| CSV | Log da UART | Linhas | Duração | Objetivo / resultado |
|---|---|---|---|---|
| `LOG_0001.CSV` | — | 0 | — | Primeiro teste do FatFs: só o cabeçalho, para validar a criação do arquivo. |
| `LOG_0002.CSV` | `LOG_0002_serial.txt` | 18 678 | 110 s | Primeira gravação completa. O data-ready ainda era pulsado: média de ~170 amostras/s, com lacunas de até 1,3 s. Corrigido com o data-ready travado e o resgate do INT1. |
| `LOG_0003.CSV` | `LOG_0003_0004_serial.txt` | 40 645 | 98 s | IMU perfeita (414,9 Hz), mas o BME280 congelou aos 54,6 s sem aviso e ficou parado nos últimos 43 s, repetindo o último valor. Motivou a detecção de perda do BME280. |
| `LOG_0004.CSV` | `LOG_0003_0004_serial.txt` | 26 632 | 64 s | Gravação de referência: 414,9 Hz, intervalos de 2,410–2,411 ms, 0 lacunas, 0 amostras perdidas. |
| `LOG_0005.CSV` | `LOG_0005_serial.txt` | 21 703 | 52 s | O BME280 caiu e voltou (2 513 linhas com T/P/UR vazias). A IMU travou aos ~58 s e não voltou. Motivou a recuperação da IMU. |
| `LOG_0006.CSV` | `LOG_0006_serial.txt` | 19 198 | 54 s | Perda e recuperação dos dois sensores: o BME280 caiu 2×, a IMU 1× (salto de 8,1 s em `t_ms`), e todos voltaram sem reset. IMU em repouso: \|a\| ≈ 1,02 g. |
| `LOG_0007.CSV`–`LOG_0009.CSV` | `LOG_0007_0009_serial.txt` | 5 062 / 2 944 / 3 364 | 12 / 7 / 8 s | Cartão removido durante a gravação (3×): os arquivos ficaram íntegros até o último `f_sync` (0,2 a 1,9 s perdidos). Ao recolocar o cartão, o firmware travava (timeouts infinitos da HAL do SD). |
| `LOG_0010.CSV`–`LOG_0012.CSV` | `LOG_0010_0012_serial.txt` | 4 642 / 10 129 / 5 414 | 11 / 24 / 13 s | Depois da correção: cartão removido e recolocado 2× na mesma execução, e a gravação voltou sozinha num arquivo novo nas duas vezes. |

## Bring-up (`bancada/`)

| Arquivo | O que mostra |
|---|---|
| `2026-10-23_01_sd_init_falhando.txt` | Cartão no BFF sem iniciar: diagnóstico CMD0/CMD8/CMD55 a cada 2 s (problema de ligação e GND do módulo, depois resolvido). |
| `2026-10-23_02_sd_init_falhando_reset.txt` | Mesmo problema, com resets; os sensores já respondiam (WHO_AM_I 0x6C, chip ID 0x60). |
| `2026-10-23_03_bme280_e_cartao_ok.txt` | Primeiro boot com o cartão OK e as leituras do BME280 (T/P/UR). |
| `2026-10-24_04_imu_drdy_pulsado_100hz.txt` | Data-ready pulsado perdendo bordas: ~100 amostras/s em vez de 416. |
| `2026-10-24_05_imu_drdy_travado_resgates.txt` | Primeira versão do data-ready travado: ainda ~100 amostras/s, com cada amostra dependendo do resgate por software. |
| `2026-10-24_06_imu_init_alternado.txt` | 14 resets seguidos: a inicialização da IMU alterna entre falha e sucesso. |
| `2026-10-24_07_imu_init_com_tentativas.txt` | 17 resets com a correção: a 1ª tentativa falha sempre no passo 3 e a 2ª funciona. |
