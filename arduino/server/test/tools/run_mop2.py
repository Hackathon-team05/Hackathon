#!/usr/bin/env python3
"""MOP2-1, MOP2-2 実施用の自動記録・グラフ化・成功率算出ツール"""

# ============================================================
# ★ 設定変数（ここだけ変更すればOK）
RECORD_DURATION_SEC = 80   # 100回の拍手(90BPMで約67秒)をカバーするため80秒に設定
REF_BPM             = 90   # 精度計算の基準BPM（メトロノームの設定値）
# ============================================================

import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
import shutil

def upload_to_arduino(pio, project_root):
    """コンパイルしてArduinoへ書き込む。"""
    print("🔨 コンパイル・書き込みを実行しています...(1分程度かかります)")
    result = subprocess.run(
        [pio, "run", "-e", "uno_r_wifi", "-t", "upload"],
        cwd=project_root
    )
    if result.returncode != 0:
        print("❌ 書き込みに失敗しました。Arduinoが接続されているか確認してください。")
        return False
    print("✅ 書き込み完了。Arduinoが起動しています...")
    time.sleep(2)  # 起動完了を待つ
    return True

def find_platformio():
    for command_name in ("pio", "platformio"):
        command_path = shutil.which(command_name)
        if command_path:
            return command_path
    local_path = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if local_path.exists():
        return str(local_path)
    return None

def main():
    pio = find_platformio()
    if not pio:
        print("エラー: PlatformIOが見つかりません。")
        return 1

    project_root = Path(__file__).resolve().parents[2]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    results_dir = project_root / "test" / "results" / ("mop2_" + timestamp)
    results_dir.mkdir(parents=True, exist_ok=True)
    log_path = results_dir / "live_log.txt"

    print("=" * 60)
    print("🎵 MOP2 評価用: リアルタイム波形記録を開始します")
    print("=" * 60)
    print(f"保存先: {log_path}")
    print("\n【操作手順】")
    print(f"1. メトロノームを {REF_BPM} BPM に設定してください。")
    print("2. 測定環境（MOP2-1: 低ノイズ／MOP2-2: 高ノイズ）を整えてください。")
    print("3. Arduinoに向かって、メトロノームに合わせて 100回 拍手してください。")
    print(f"4. {RECORD_DURATION_SEC}秒間自動で記録し、その後にグラフと成功率を出力します。")
    print("   （途中で強制終了したい場合は [ Ctrl + C ] を押してください）\n")

    # コンパイル＆書き込み
    if not upload_to_arduino(pio, project_root):
        return 1

    process = None
    try:
        # pio device monitorを実行してシリアルを受信
        process = subprocess.Popen(
            [pio, "device", "monitor"],
            cwd=project_root,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1
        )
        
        start_time = time.time()
        with log_path.open("w", encoding="utf-8") as f:
            while True:
                line = process.stdout.readline()
                if not line:
                    break
                
                print(line, end="")
                f.write(line)
                
                # RECORD_DURATION_SEC秒経過したら自動でループを抜ける
                if time.time() - start_time >= RECORD_DURATION_SEC:
                    print(f"\n\n⏹️ {RECORD_DURATION_SEC}秒経過しました。自動で記録を停止します...")
                    break
                    
    except KeyboardInterrupt:
        print("\n\n⏹️ 記録を強制停止しました。グラフを生成しています...")
        
    finally:
        if process:
            process.terminate()
            process.wait()

    # グラフ生成スクリプトを自動で呼び出す
    plot_script = Path(__file__).resolve().with_name("plot_mic_results.py")
    subprocess.run([sys.executable, str(plot_script), str(log_path), "--output-dir", str(results_dir)], cwd=project_root)

    # 精度解析スクリプトを自動で呼び出す
    accuracy_script = Path(__file__).resolve().with_name("analyze_accuracy.py")
    acc_result = subprocess.run(
        [sys.executable, str(accuracy_script), str(log_path), "--ref-bpm", str(REF_BPM)],
        cwd=project_root,
        capture_output=True,
        text=True
    )

    # 成功率を簡易計算して追記
    max_clap_count = 0
    with log_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if "MIC_DATA," in line:
                try:
                    parts = line.strip().split(",")
                    max_clap_count = int(parts[-1])
                except Exception:
                    pass
    
    success_rate = (max_clap_count / 100.0) * 100

    print("\n" + "="*60)
    print(f"MOP2 評価用 簡易結果")
    print(f"検出された拍手回数: {max_clap_count} 回 / 100 回中")
    print(f"成功率: {success_rate:.1f} %")
    print("="*60 + "\n")

    # 解析結果をテキストファイルに追記する
    with log_path.open("a", encoding="utf-8") as f:
        f.write("\n\n" + "="*60 + "\n")
        f.write("MOP2 評価用 簡易結果\n")
        f.write("="*60 + "\n")
        f.write(f"想定拍手回数: 100 回\n")
        f.write(f"検出された拍手回数: {max_clap_count} 回\n")
        f.write(f"成功率: {success_rate:.1f} %\n")
        f.write("\n\n" + "="*60 + "\n")
        f.write("詳細な解析結果 (analyze_accuracy.py)\n")
        f.write("="*60 + "\n")
        f.write(acc_result.stdout)

    print("\n✨ 完了しました！以下のフォルダにデータ、グラフ、成功率が保存されています:")
    print(str(results_dir))

if __name__ == "__main__":
    sys.exit(main())
