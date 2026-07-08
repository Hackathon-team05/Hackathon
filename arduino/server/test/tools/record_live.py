#!/usr/bin/env python3
"""本番環境の輪唱を動かしながら、マイクの波形を自動記録・グラフ化するツール"""

import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
import shutil

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
    results_dir = project_root / "test" / "results" / ("live_" + timestamp)
    results_dir.mkdir(parents=True, exist_ok=True)
    log_path = results_dir / "live_log.txt"

    print("=" * 60)
    print("🎵 本番環境（輪唱）のリアルタイム波形記録を開始します")
    print("=" * 60)
    print(f"保存先: {log_path}")
    print("\n【操作手順】")
    print("1. Arduinoに向かって拍手などで波形を発生させてください。")
    print("2. 30秒間自動で記録し、その後にグラフを生成します。")
    print("   （途中で強制終了したい場合は [ Ctrl + C ] を押してください）\n")

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
                
                # 30秒経過したら自動でループを抜ける
                if time.time() - start_time >= 30:
                    print("\n\n⏹️ 30秒経過しました。自動で記録を停止します...")
                    break
                    
    except KeyboardInterrupt:
        print("\n\n⏹️ 記録を強制停止しました。グラフを生成しています...")
        
    finally:
        if process:
            process.terminate()
            process.wait()

    # 既存のグラフ生成スクリプトを自動で呼び出す
    plot_script = Path(__file__).resolve().with_name("plot_mic_results.py")
    subprocess.run([sys.executable, str(plot_script), str(log_path)], cwd=project_root)

    print("\n✨ 完了しました！以下のフォルダにグラフが保存されています:")
    print(str(results_dir))

if __name__ == "__main__":
    sys.exit(main())
