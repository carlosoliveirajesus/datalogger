#!/usr/bin/env python3
"""
Plota os CSVs gravados pelo Datalogger EPTA (LOG_nnnn.CSV).

Formato esperado (ver README, "Formato do CSV"):
    t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,press_hpa,hum_rh
- Uma linha por amostra da IMU (~415 Hz).
- Colunas do BME280 vazias quando o sensor estava perdido.
- Salto em t_ms quando a IMU estava perdida ou houve lacuna.

Para cada arquivo gera um PNG com 5 painéis (aceleração, |a|, giro,
temperatura, pressão/umidade) e imprime um resumo no terminal.

Uso:
    python plot_log.py ARQUIVO.CSV [ARQUIVO2.CSV ...] [-o PASTA_SAIDA]
    python plot_log.py ../docs/testes/csv/*.CSV -o ../docs/testes/graficos

Dependências: numpy e matplotlib (pip install -r requirements.txt).
"""
import argparse
import glob
import os
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")  # gera PNG sem precisar de janela
import matplotlib.pyplot as plt

COLUMNS = ["t_ms", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps", "temp_c", "press_hpa", "hum_rh"]
ODR_HZ = 416.0
GAP_MS = 10.0  # intervalo maior que ~4 períodos da IMU = lacuna (não liga os pontos)
REST_GYRO_DPS = 2.0  # |giro| abaixo disso = placa parada


def load_csv(path):
    """Lê o CSV; colunas vazias viram NaN. Retorna dict coluna -> array."""
    with open(path, "r", encoding="ascii", errors="replace") as f:
        header = f.readline().strip().split(",")
        has_data = bool(f.readline().strip())
    if header != COLUMNS:
        raise ValueError(f"cabeçalho inesperado em {path}: {header}")
    if not has_data:
        return None

    data = np.genfromtxt(path, delimiter=",", skip_header=1, missing_values="", filling_values=np.nan)
    if data.size == 0:
        return None
    data = np.atleast_2d(data)
    return {name: data[:, i] for i, name in enumerate(COLUMNS)}


def break_gaps(t_s, *series):
    """Insere NaN onde há lacuna no tempo, para o gráfico não ligar os pontos."""
    dt_ms = np.diff(t_s) * 1000.0
    idx = np.where(dt_ms > GAP_MS)[0] + 1
    t_out = np.insert(t_s, idx, np.nan)
    out = [np.insert(s, idx, np.nan) for s in series]
    return t_out, out


def summarize(name, d):
    t = d["t_ms"]
    dt = np.diff(t)
    n = len(t)
    dur_s = (t[-1] - t[0]) / 1000.0
    rate = (n - 1) / dur_s if dur_s > 0 else float("nan")
    gaps = dt[dt > GAP_MS]
    env_missing = int(np.isnan(d["temp_c"]).sum())

    acc = np.vstack([d["ax_g"], d["ay_g"], d["az_g"]])
    gyr = np.vstack([d["gx_dps"], d["gy_dps"], d["gz_dps"]])
    anorm = np.linalg.norm(acc, axis=0)
    rest = np.linalg.norm(gyr, axis=0) < REST_GYRO_DPS

    lines = [
        f"== {name}",
        f"   linhas: {n}   duração: {dur_s:.1f} s   taxa média: {rate:.2f} Hz",
        f"   intervalo entre linhas: min {dt.min():.3f} ms, máx {dt.max():.3f} ms",
        f"   lacunas (> {GAP_MS:.0f} ms): {len(gaps)}" + (f", maior {gaps.max() / 1000.0:.2f} s" if len(gaps) else ""),
        f"   linhas sem BME280: {env_missing}",
    ]
    if rest.sum() > 100:
        lines.append(
            f"   em repouso ({rest.sum()} amostras): |a| = {anorm[rest].mean():.4f} g "
            f"(desvio {anorm[rest].std():.4f} g), bias do giro = "
            + " / ".join(f"{gyr[i, rest].mean():+.2f}" for i in range(3))
            + " °/s"
        )
    env_ok = ~np.isnan(d["temp_c"])
    if env_ok.any():
        lines.append(
            f"   T {np.nanmin(d['temp_c']):.2f}–{np.nanmax(d['temp_c']):.2f} °C   "
            f"P {np.nanmin(d['press_hpa']):.2f}–{np.nanmax(d['press_hpa']):.2f} hPa   "
            f"UR {np.nanmin(d['hum_rh']):.1f}–{np.nanmax(d['hum_rh']):.1f} %"
        )
    return "\n".join(lines)


def plot(name, d, out_png):
    t_s = (d["t_ms"] - d["t_ms"][0]) / 1000.0
    anorm = np.sqrt(d["ax_g"] ** 2 + d["ay_g"] ** 2 + d["az_g"] ** 2)
    t, (ax_, ay_, az_, an_, gx_, gy_, gz_, tc_, p_, h_) = break_gaps(
        t_s, d["ax_g"], d["ay_g"], d["az_g"], anorm, d["gx_dps"], d["gy_dps"], d["gz_dps"],
        d["temp_c"], d["press_hpa"], d["hum_rh"])

    colors = {"x": "#d62728", "y": "#2ca02c", "z": "#1f77b4"}
    fig, axs = plt.subplots(5, 1, figsize=(12, 13), sharex=True, constrained_layout=True)
    dur = t_s[-1]
    rate = (len(t_s) - 1) / dur if dur > 0 else 0
    fig.suptitle(f"{name}  —  {len(t_s)} linhas, {dur:.1f} s, {rate:.1f} Hz", fontsize=13)

    a = axs[0]
    a.plot(t, ax_, lw=0.6, color=colors["x"], label="ax")
    a.plot(t, ay_, lw=0.6, color=colors["y"], label="ay")
    a.plot(t, az_, lw=0.6, color=colors["z"], label="az")
    a.set_ylabel("aceleração (g)")
    a.legend(loc="upper right", ncol=3, fontsize=8)

    a = axs[1]
    a.plot(t, an_, lw=0.6, color="black", label="|a|")
    a.axhline(1.0, color="gray", ls="--", lw=0.8, label="1 g")
    a.set_ylabel("|a| (g)")
    a.legend(loc="upper right", ncol=2, fontsize=8)

    a = axs[2]
    a.plot(t, gx_, lw=0.6, color=colors["x"], label="gx")
    a.plot(t, gy_, lw=0.6, color=colors["y"], label="gy")
    a.plot(t, gz_, lw=0.6, color=colors["z"], label="gz")
    a.set_ylabel("vel. angular (°/s)")
    a.legend(loc="upper right", ncol=3, fontsize=8)

    a = axs[3]
    a.plot(t, tc_, lw=1.0, color="#ff7f0e")
    a.set_ylabel("temperatura (°C)")

    a = axs[4]
    a.plot(t, p_, lw=1.0, color="#9467bd", label="pressão")
    a.set_ylabel("pressão (hPa)", color="#9467bd")
    a.ticklabel_format(axis="y", useOffset=False)
    a2 = a.twinx()
    a2.plot(t, h_, lw=1.0, color="#17becf", label="umidade")
    a2.set_ylabel("umidade (% UR)", color="#17becf")
    a.set_xlabel("tempo desde o início da gravação (s)")

    # Trechos sem BME280 (colunas vazias) sombreados nos painéis dele.
    miss = np.isnan(d["temp_c"])
    if miss.any() and not miss.all():
        edges = np.diff(np.concatenate(([0], miss.astype(int), [0])))
        for s, e in zip(np.where(edges == 1)[0], np.where(edges == -1)[0]):
            for k in (3, 4):
                axs[k].axvspan(t_s[s], t_s[e - 1], color="red", alpha=0.12, lw=0)
        axs[3].text(0.01, 0.92, "sombreado: BME280 perdido", transform=axs[3].transAxes, fontsize=8, color="red")

    # Lacunas longas (IMU perdida, data-ready perdido) sombreadas em cinza.
    dt_s = np.diff(t_s)
    long_gaps = np.where(dt_s > 0.5)[0]
    for i in long_gaps:
        for a in axs:
            a.axvspan(t_s[i], t_s[i + 1], color="gray", alpha=0.15, lw=0)
    if len(long_gaps):
        axs[0].text(0.01, 0.92, "cinza: sem amostras da IMU (lacuna > 0,5 s)", transform=axs[0].transAxes,
                    fontsize=8, color="dimgray")

    for a in axs:
        a.grid(True, alpha=0.3)

    fig.savefig(out_png, dpi=110)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description="Plota os CSVs do Datalogger EPTA.")
    ap.add_argument("csv", nargs="+", help="arquivos LOG_nnnn.CSV")
    ap.add_argument("-o", "--out", default=None, help="pasta dos PNGs (padrão: ao lado de cada CSV)")
    args = ap.parse_args()

    # Expande curingas aqui também (o PowerShell/cmd não expande *).
    paths = []
    for arg in args.csv:
        paths.extend(sorted(glob.glob(arg)) or [arg])

    for path in paths:
        name = os.path.basename(path)
        try:
            d = load_csv(path)
        except (OSError, ValueError) as e:
            print(f"== {name}: erro: {e}", file=sys.stderr)
            continue
        if d is None or len(d["t_ms"]) < 2:
            print(f"== {name}: só o cabeçalho, nada para plotar")
            continue

        out_dir = args.out or os.path.dirname(os.path.abspath(path))
        os.makedirs(out_dir, exist_ok=True)
        out_png = os.path.join(out_dir, os.path.splitext(name)[0] + ".png")

        print(summarize(name, d))
        plot(name, d, out_png)
        print(f"   gráfico: {out_png}")


if __name__ == "__main__":
    main()
