# Scripts de análise

## `plot_log.py`

Lê os `LOG_nnnn.CSV` gravados pelo datalogger. Para cada arquivo:
- **imprime um resumo:** taxa de amostragem, intervalos entre linhas, lacunas, linhas sem BME280, e |a| e bias do giro em repouso;
- **salva um PNG com 5 painéis:** aceleração (3 eixos), |a| com a referência de 1 g, velocidade angular, temperatura, e pressão com umidade.

Os trechos com o BME280 perdido ficam sombreados em vermelho. As lacunas sem amostras da IMU ficam em cinza.

```bash
pip install -r requirements.txt
python plot_log.py ../docs/testes/csv/*.CSV -o ../docs/testes/graficos
```

O script expande o `*` sozinho, então o mesmo comando funciona no PowerShell e no cmd do Windows.

Sem `-o`, o PNG é salvo ao lado de cada CSV. Os gráficos dos testes já gerados estão em [`docs/testes/graficos/`](../docs/testes/graficos/).
