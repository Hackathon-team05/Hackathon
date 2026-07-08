#!/usr/bin/env python3
"""メトロノームの読み取り精度を解析するスクリプト。

使い方:
    python3 analyze_accuracy.py <ログファイル> [--ref-bpm 90]

ログファイルは record_live.py で取得した live_log.txt を指定してください。
"""

import argparse
import sys
from pathlib import Path

DATA_PREFIX = "MIC_DATA,"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="マイク録音ログからBPM検出精度を計算します。"
    )
    parser.add_argument("log_file", type=Path, help="record_live.py で取得したログファイル")
    parser.add_argument(
        "--ref-bpm",
        type=float,
        default=90.0,
        help="基準BPM（メトロノームの設定値）。デフォルト: 90",
    )
    return parser.parse_args()


def read_mic_data(log_path):
    """ログ中のMIC_DATA行だけを抽出する。"""
    rows = []
    with log_path.open("r", encoding="utf-8", errors="replace") as f:
        for line_number, line in enumerate(f, start=1):
            pos = line.find(DATA_PREFIX)
            if pos < 0:
                continue
            values = line[pos + len(DATA_PREFIX):].strip().split(",")
            if len(values) != 6:
                print(f"警告: {line_number}行目を読み飛ばしました。", file=sys.stderr)
                continue
            try:
                rows.append({
                    "time_ms": int(values[0]),
                    "adc":     int(values[1]),
                    "peak_track":  int(values[2]),
                    "bpm_updated": int(values[3]),
                    "bpm":         float(values[4]),
                    "clap_count":  int(values[5]),
                })
            except ValueError:
                print(f"警告: {line_number}行目に数値以外が含まれます。", file=sys.stderr)
    return rows


def find_clap_events(rows):
    """clap_count が増加した瞬間の時刻リスト（ms）を返す。"""
    events = []
    prev = rows[0]["clap_count"]
    for row in rows[1:]:
        if row["clap_count"] > prev:
            events.append(row["time_ms"])
        prev = row["clap_count"]
    return events


def calc_interval_bpm(events):
    """拍手イベントの時刻リストから各間隔のBPMを計算する。"""
    if len(events) < 2:
        return []
    intervals_ms = [events[i+1] - events[i] for i in range(len(events) - 1)]
    return [60000.0 / ms for ms in intervals_ms if ms > 0]


def calc_stats(values):
    """平均・標準偏差・最小・最大を返す。"""
    if not values:
        return None
    n = len(values)
    mean = sum(values) / n
    variance = sum((v - mean) ** 2 for v in values) / n
    std = variance ** 0.5
    return {"mean": mean, "std": std, "min": min(values), "max": max(values), "n": n}


def print_report(ref_bpm, system_bpm_list, interval_bpm_list):
    """精度レポートを表示する。"""
    sep = "=" * 60

    print(f"\n{sep}")
    print(f"  BPM読み取り精度レポート  (基準BPM: {ref_bpm:.1f})")
    print(sep)

    # --- システムが出力したBPM（bpm_updated=1 の行の bpm 値）---
    if system_bpm_list:
        s = calc_stats(system_bpm_list)
        errors = [abs(v - ref_bpm) for v in system_bpm_list]
        rel_errors = [e / ref_bpm * 100 for e in errors]
        print(f"\n【システム出力BPM（拍手検知後に更新した値）】  n={s['n']}")
        print(f"  平均値    : {s['mean']:.2f} BPM  (誤差: {s['mean']-ref_bpm:+.2f})")
        print(f"  標準偏差  : {s['std']:.2f} BPM")
        print(f"  最小 / 最大: {s['min']:.1f} / {s['max']:.1f} BPM")
        print(f"  平均絶対誤差(MAE): {sum(errors)/len(errors):.2f} BPM")
        print(f"  平均相対誤差     : {sum(rel_errors)/len(rel_errors):.2f} %")
    else:
        print("\n【システム出力BPM】 データなし（拍手が検知されませんでした）")

    # --- 拍手イベントの間隔から算出した実測BPM ---
    if interval_bpm_list:
        s = calc_stats(interval_bpm_list)
        errors = [abs(v - ref_bpm) for v in interval_bpm_list]
        rel_errors = [e / ref_bpm * 100 for e in errors]
        print(f"\n【拍手間隔から算出した実測BPM】  n={s['n']}")
        print(f"  平均値    : {s['mean']:.2f} BPM  (誤差: {s['mean']-ref_bpm:+.2f})")
        print(f"  標準偏差  : {s['std']:.2f} BPM")
        print(f"  最小 / 最大: {s['min']:.1f} / {s['max']:.1f} BPM")
        print(f"  平均絶対誤差(MAE): {sum(errors)/len(errors):.2f} BPM")
        print(f"  平均相対誤差     : {sum(rel_errors)/len(rel_errors):.2f} %")
        print(f"\n  各拍手間のBPM: {[f'{v:.1f}' for v in interval_bpm_list]}")
    else:
        print("\n【拍手間隔BPM】 データなし（拍手が2回以上検知されませんでした）")

    print(f"\n{sep}\n")


def main():
    args = parse_arguments()
    log_path = args.log_file.expanduser().resolve()
    if not log_path.is_file():
        print(f"エラー: ログファイルが見つかりません: {log_path}", file=sys.stderr)
        return 2

    rows = read_mic_data(log_path)
    if not rows:
        print("エラー: MIC_DATA行がありません。", file=sys.stderr)
        return 1

    # システムが更新したBPMリスト
    system_bpm_list = [r["bpm"] for r in rows if r["bpm_updated"] != 0]

    # 拍手イベントの間隔から算出したBPMリスト
    clap_events = find_clap_events(rows)
    interval_bpm_list = calc_interval_bpm(clap_events)

    print_report(args.ref_bpm, system_bpm_list, interval_bpm_list)
    return 0


if __name__ == "__main__":
    sys.exit(main())
