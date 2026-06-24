#!/usr/bin/env python3
"""PlatformIOテストを実行し、結果を日時別のファイルへ保存する。"""

import argparse
from datetime import datetime
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


TEST_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = TEST_ROOT.parent
DEFAULT_RESULTS_ROOT = TEST_ROOT / "results"
SUITES = ("test_server", "test_mic_hardware")
ESCAPED_UTF8_BYTE_SEQUENCE = re.compile(r"(?:\\x[0-9A-Fa-f]{2})+")


def decode_escaped_utf8_bytes(text):
    """PlatformIOがUnity失敗メッセージに残す\\xHH列をUTF-8へ戻す。"""
    def replace(match):
        byte_values = bytes(
            int(value, 16)
            for value in re.findall(r"\\x([0-9A-Fa-f]{2})", match.group(0))
        )
        return byte_values.decode("utf-8", errors="replace")

    return ESCAPED_UTF8_BYTE_SEQUENCE.sub(replace, text)


def normalize_escaped_utf8_file(path):
    """生成済みレポート内の\\xHH列をUTF-8へ戻す。"""
    if not path.exists():
        return

    original = path.read_text(encoding="utf-8")
    normalized = decode_escaped_utf8_bytes(original)
    if normalized != original:
        path.write_text(normalized, encoding="utf-8")


def find_platformio():
    """利用可能なPlatformIOコマンドを探す。"""
    for command_name in ("pio", "platformio"):
        command_path = shutil.which(command_name)
        if command_path:
            return command_path

    local_path = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if local_path.exists():
        return str(local_path)

    return None


def run_and_save(command, output_path, environment):
    """コマンドの表示内容を端末とテキストファイルの両方へ出力する。"""
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as output_file:
        output_file.write("実行日時: " + datetime.now().isoformat(timespec="seconds") + "\n")
        output_file.write("実行コマンド: " + " ".join(command) + "\n\n")
        output_file.flush()

        process = subprocess.Popen(
            command,
            cwd=PROJECT_ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )

        if process.stdout is not None:
            for line in process.stdout:
                line = decode_escaped_utf8_bytes(line)
                print(line, end="")
                output_file.write(line)

        return process.wait()


def run_plot_script(log_path, output_directory):
    """マイク試験ログからグラフとCSVを生成する。"""
    plot_script = Path(__file__).with_name("plot_mic_results.py")
    command = [
        sys.executable,
        str(plot_script),
        str(log_path),
        "--output-dir",
        str(output_directory),
    ]
    print("\nマイク測定グラフを生成します。")
    return subprocess.run(command, cwd=PROJECT_ROOT, check=False).returncode


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Arduino単体テストを実行し、ログ・JSON・JUnit XMLを保存します。"
    )
    parser.add_argument(
        "--suite",
        choices=("all",) + SUITES,
        default="all",
        help="実行するテストスイート（既定値: all）",
    )
    parser.add_argument(
        "--environment",
        default="uno_r_wifi",
        help="platformio.iniの環境名（既定値: uno_r_wifi）",
    )
    parser.add_argument("--upload-port", help="書き込み先ポート")
    parser.add_argument("--test-port", help="テスト結果を読むシリアルポート")
    parser.add_argument(
        "--results-dir",
        type=Path,
        help="結果保存先。省略時はtest/results/日時",
    )
    parser.add_argument(
        "--skip-plots",
        action="store_true",
        help="マイク試験後のグラフ生成を省略する",
    )
    return parser.parse_args()


def main():
    args = parse_arguments()
    if not (PROJECT_ROOT / "platformio.ini").exists():
        print(
            "エラー: PlatformIOプロジェクトが見つかりません: " + str(PROJECT_ROOT),
            file=sys.stderr,
        )
        return 2

    platformio = find_platformio()
    if platformio is None:
        print("エラー: PlatformIOが見つかりません。", file=sys.stderr)
        return 2

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    result_directory = (
        args.results_dir.expanduser().resolve()
        if args.results_dir
        else DEFAULT_RESULTS_ROOT / timestamp
    )
    result_directory.mkdir(parents=True, exist_ok=True)

    suites = SUITES if args.suite == "all" else (args.suite,)
    environment = os.environ.copy()
    environment["PLATFORMIO_SETTING_ENABLE_TELEMETRY"] = "no"
    overall_result = 0

    for suite in suites:
        print("\n" + "=" * 60)
        print("テスト開始: " + suite)
        print("結果保存先: " + str(result_directory))

        json_path = result_directory / (suite + ".json")
        junit_path = result_directory / (suite + ".xml")
        command = [
            platformio,
            "test",
            "-v",
            "-e",
            args.environment,
            "-f",
            suite,
            "--json-output-path",
            str(json_path),
            "--junit-output-path",
            str(junit_path),
        ]
        if args.upload_port:
            command.extend(["--upload-port", args.upload_port])
        if args.test_port:
            command.extend(["--test-port", args.test_port])

        log_path = result_directory / (suite + ".txt")
        return_code = run_and_save(command, log_path, environment)
        for report_path in (log_path, json_path, junit_path):
            normalize_escaped_utf8_file(report_path)
        if return_code != 0:
            overall_result = return_code
            print("テスト失敗: " + suite)
        else:
            print("テスト成功: " + suite)

        if suite == "test_mic_hardware" and not args.skip_plots:
            plot_result = run_plot_script(log_path, result_directory)
            if plot_result != 0 and overall_result == 0:
                overall_result = plot_result

    print("\n保存先: " + str(result_directory))
    return overall_result


if __name__ == "__main__":
    sys.exit(main())
