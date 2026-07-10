#!/usr/bin/env python3
"""マイク実機試験のMIC_DATAログをCSVとグラフへ変換する。"""

import argparse
import csv
import os
from pathlib import Path
import sys


DATA_PREFIX = "MIC_DATA,"
CSV_HEADER = [
    "time_ms",
    "adc",
    "peak_track",
    "bpm_updated",
    "bpm",
    "clap_count",
    "detected_clap_total",
]


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="マイク試験ログから音波形、BPM推移、拍手検出グラフを生成します。"
    )
    parser.add_argument("log_file", type=Path, help="test_mic_hardwareのテキストログ")
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="出力先。省略時はログファイルと同じディレクトリ",
    )
    return parser.parse_args()


def read_mic_data(log_path):
    """ログ中のMIC_DATA行だけを抽出する。"""
    rows = []
    with log_path.open("r", encoding="utf-8", errors="replace") as log_file:
        for line_number, line in enumerate(log_file, start=1):
            prefix_position = line.find(DATA_PREFIX)
            if prefix_position < 0:
                continue

            values = line[prefix_position + len(DATA_PREFIX) :].strip().split(",")
            if len(values) != 7:
                print(
                    "警告: " + str(line_number) + "行目のMIC_DATAを読み飛ばしました。",
                    file=sys.stderr,
                )
                continue

            try:
                rows.append(
                    {
                        "time_ms": int(values[0]),
                        "adc": int(values[1]),
                        "peak_track": int(values[2]),
                        "bpm_updated": int(values[3]),
                        "bpm": float(values[4]),
                        "clap_count": int(values[5]),
                        "detected_clap_total": int(values[6]),
                    }
                )
            except ValueError:
                print(
                    "警告: " + str(line_number) + "行目に数値以外が含まれます。",
                    file=sys.stderr,
                )

    return rows


def write_csv(rows, output_path):
    """抽出結果を後から表計算ソフトでも扱えるCSVへ保存する。"""
    with output_path.open("w", encoding="utf-8", newline="") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_HEADER)
        writer.writeheader()
        writer.writerows(rows)


def load_matplotlib(cache_directory):
    """画面を開かずPNGを生成できるmatplotlibを読み込む。"""
    cache_directory.mkdir(parents=True, exist_ok=True)
    os.environ["MPLCONFIGDIR"] = str(cache_directory)

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        requirements_path = Path(__file__).with_name("requirements.txt")
        print(
            "エラー: matplotlibが必要です。次のコマンドを実行してください。\n"
            "python3 -m pip install -r " + str(requirements_path),
            file=sys.stderr,
        )
        return None

    return plt


def save_waveform_graph(plt, rows, output_path):
    """ADC値の時間変化とBPM更新位置を描画する。"""
    time_seconds = [row["time_ms"] / 1000.0 for row in rows]
    adc_values = [row["adc"] for row in rows]
    update_times = [
        row["time_ms"] / 1000.0 for row in rows if row["bpm_updated"] != 0
    ]
    update_values = [row["adc"] for row in rows if row["bpm_updated"] != 0]

    figure, axis = plt.subplots(figsize=(12, 5))
    axis.plot(time_seconds, adc_values, linewidth=0.8, label="Microphone ADC")
    if update_times:
        axis.scatter(
            update_times,
            update_values,
            color="red",
            s=24,
            label="BPM update",
            zorder=3,
        )
    axis.set_title("Microphone Waveform")
    axis.set_xlabel("Time [s]")
    axis.set_ylabel("ADC value")
    axis.grid(True, alpha=0.3)
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def save_bpm_graph(plt, rows, output_path):
    """BPM値の時間変化を階段状の線で描画する。"""
    time_seconds = [row["time_ms"] / 1000.0 for row in rows]
    bpm_values = [row["bpm"] for row in rows]
    update_rows = [row for row in rows if row["bpm_updated"] != 0]

    figure, axis = plt.subplots(figsize=(12, 5))
    axis.step(time_seconds, bpm_values, where="post", label="BPM")
    if update_rows:
        axis.scatter(
            [row["time_ms"] / 1000.0 for row in update_rows],
            [row["bpm"] for row in update_rows],
            color="red",
            s=24,
            label="Updated BPM",
            zorder=3,
        )
    axis.set_title("BPM Transition")
    axis.set_xlabel("Time [s]")
    axis.set_ylabel("BPM")
    axis.grid(True, alpha=0.3)
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def save_clap_graph(plt, rows, output_path):
    """ピーク追跡状態と累積拍手回数を描画する。"""
    time_seconds = [row["time_ms"] / 1000.0 for row in rows]
    peak_values = [row["peak_track"] for row in rows]
    clap_values = [row["detected_clap_total"] for row in rows]

    figure, first_axis = plt.subplots(figsize=(12, 5))
    first_axis.step(
        time_seconds,
        peak_values,
        where="post",
        color="tab:orange",
        label="Peak tracking",
    )
    first_axis.set_xlabel("Time [s]")
    first_axis.set_ylabel("Peak tracking state")
    first_axis.set_yticks([0, 1])
    first_axis.grid(True, alpha=0.3)

    second_axis = first_axis.twinx()
    second_axis.step(
        time_seconds,
        clap_values,
        where="post",
        color="tab:blue",
        label="Clap count",
    )
    second_axis.set_ylabel("Cumulative clap count")

    lines = first_axis.get_lines() + second_axis.get_lines()
    first_axis.legend(lines, [line.get_label() for line in lines], loc="upper left")
    first_axis.set_title("Clap Detection")
    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def find_clap_times(rows):
    """detected_clap_total が増加した時刻を秒単位で返す。"""
    clap_times = []
    previous_count = rows[0]["detected_clap_total"]

    for row in rows[1:]:
        if row["detected_clap_total"] > previous_count:
            clap_times.append(row["time_ms"] / 1000.0)
        previous_count = row["detected_clap_total"]

    return clap_times


def save_comparison_graph(plt, rows, output_path):
    """マイク波形と拍手検出状態を共通の時間軸で上下に描画する。"""
    time_seconds = [row["time_ms"] / 1000.0 for row in rows]
    adc_values = [row["adc"] for row in rows]
    peak_values = [row["peak_track"] for row in rows]
    clap_values = [row["detected_clap_total"] for row in rows]
    clap_times = find_clap_times(rows)
    update_times = [
        row["time_ms"] / 1000.0 for row in rows if row["bpm_updated"] != 0
    ]

    figure, (wave_axis, detection_axis) = plt.subplots(
        2,
        1,
        figsize=(12, 8),
        sharex=True,
        gridspec_kw={"height_ratios": [2, 1]},
    )

    wave_axis.plot(
        time_seconds,
        adc_values,
        linewidth=0.8,
        color="tab:blue",
        label="Microphone ADC",
    )
    wave_axis.set_title("Microphone Waveform and Clap Detection")
    wave_axis.set_ylabel("ADC value")
    wave_axis.grid(True, alpha=0.3)

    detection_axis.step(
        time_seconds,
        peak_values,
        where="post",
        color="tab:orange",
        label="Peak tracking",
    )
    detection_axis.set_xlabel("Time [s]")
    detection_axis.set_ylabel("Peak state")
    detection_axis.set_yticks([0, 1])
    detection_axis.grid(True, alpha=0.3)

    clap_axis = detection_axis.twinx()
    clap_axis.step(
        time_seconds,
        clap_values,
        where="post",
        color="tab:blue",
        label="Clap count",
    )
    clap_axis.set_ylabel("Cumulative clap count")

    for index, clap_time in enumerate(clap_times):
        label = "Detected clap" if index == 0 else None
        wave_axis.axvline(
            clap_time,
            color="tab:green",
            linewidth=1.0,
            alpha=0.7,
            label=label,
        )
        detection_axis.axvline(
            clap_time,
            color="tab:green",
            linewidth=1.0,
            alpha=0.7,
        )

    for index, update_time in enumerate(update_times):
        label = "BPM update" if index == 0 else None
        wave_axis.axvline(
            update_time,
            color="red",
            linewidth=1.2,
            linestyle="--",
            alpha=0.8,
            label=label,
        )
        detection_axis.axvline(
            update_time,
            color="red",
            linewidth=1.2,
            linestyle="--",
            alpha=0.8,
        )

    wave_axis.legend(loc="upper right")
    detection_lines = detection_axis.get_lines()[:1] + clap_axis.get_lines()
    detection_axis.legend(
        detection_lines,
        [line.get_label() for line in detection_lines],
        loc="upper left",
    )

    figure.tight_layout()
    figure.savefig(output_path, dpi=150)
    plt.close(figure)


def main():
    args = parse_arguments()
    log_path = args.log_file.expanduser().resolve()
    if not log_path.is_file():
        print("エラー: ログファイルが見つかりません: " + str(log_path), file=sys.stderr)
        return 2

    output_directory = (
        args.output_dir.expanduser().resolve()
        if args.output_dir
        else log_path.parent
    )
    output_directory.mkdir(parents=True, exist_ok=True)

    rows = read_mic_data(log_path)
    if not rows:
        print(
            "エラー: MIC_DATA行がありません。マイク実機試験ログを確認してください。",
            file=sys.stderr,
        )
        return 1

    csv_path = output_directory / "mic_data.csv"
    write_csv(rows, csv_path)

    plt = load_matplotlib(output_directory / ".matplotlib")
    if plt is None:
        print("CSVは保存しました: " + str(csv_path))
        return 2

    save_waveform_graph(plt, rows, output_directory / "mic_waveform.png")
    save_bpm_graph(plt, rows, output_directory / "bpm_transition.png")
    save_clap_graph(plt, rows, output_directory / "clap_detection.png")
    save_comparison_graph(plt, rows, output_directory / "mic_clap_comparison.png")

    print("CSVを保存しました: " + str(csv_path))
    print("音波形を保存しました: " + str(output_directory / "mic_waveform.png"))
    print("BPM推移を保存しました: " + str(output_directory / "bpm_transition.png"))
    print("拍手検出を保存しました: " + str(output_directory / "clap_detection.png"))
    print(
        "比較グラフを保存しました: "
        + str(output_directory / "mic_clap_comparison.png")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
