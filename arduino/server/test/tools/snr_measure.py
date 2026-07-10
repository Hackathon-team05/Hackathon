#!/usr/bin/env python3
"""PCマイクでノイズ区間と拍手区間を録音し、SNR相当値(dB)を算出するツール。

計画書 §2.1.3 / §1.3.3 の定義に基づく。
    SNR相当値(dB) = 拍手区間のRMS(dBFS) - 無拍手ノイズ区間のRMS(dBFS)

使い方:
    python3 snr_measure.py
    python3 snr_measure.py --noise-sec 5 --clap-sec 5 --device 1

事前準備:
    pip install -r requirements.txt
"""

import argparse
import sys
from datetime import datetime
from pathlib import Path

import numpy as np
import sounddevice as sd

SAMPLE_RATE = 44100

# 計画書 §1.1.5 の想定使用環境の閾値
LOW_NOISE_SNR_DB = 10.0
HIGH_NOISE_SNR_DB = -5.0


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="PCマイク入力からSNR相当値(dB)を計測します。"
    )
    parser.add_argument(
        "--noise-sec", type=float, default=5.0, help="無拍手ノイズ区間の録音秒数（既定: 5）"
    )
    parser.add_argument(
        "--clap-sec", type=float, default=5.0, help="拍手区間の録音秒数（既定: 5）"
    )
    parser.add_argument(
        "--device", type=int, default=None, help="録音に使うマイクデバイス番号（省略時は既定デバイス）"
    )
    parser.add_argument(
        "--save-dir",
        type=Path,
        default=None,
        help="録音したWAVを保存するディレクトリ（省略時は保存しない）",
    )
    return parser.parse_args()


def record(seconds, label, device):
    """指定秒数だけ録音する。開始前にEnterキー待ちを挟む。"""
    input(f"\n[{label}] 準備ができたらEnterを押してください。{seconds:.1f}秒間録音します...")
    print("録音中...")
    audio = sd.rec(
        int(seconds * SAMPLE_RATE),
        samplerate=SAMPLE_RATE,
        channels=1,
        dtype="float32",
        device=device,
    )
    sd.wait()
    print("録音終了。")
    return audio.flatten()


def rms_dbfs(samples):
    """振幅-1.0〜1.0のサンプル列からdBFSを算出する。無音の場合は-infを返す。"""
    rms = np.sqrt(np.mean(samples.astype(np.float64) ** 2))
    if rms <= 0:
        return -float("inf")
    return 20.0 * np.log10(rms)


def save_wav(path, samples):
    """WAVとして保存する（16bit PCM）。scipyが無い環境向けに標準waveモジュールを使う。"""
    import wave

    path.parent.mkdir(parents=True, exist_ok=True)
    clipped = np.clip(samples, -1.0, 1.0)
    pcm16 = (clipped * 32767.0).astype(np.int16)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm16.tobytes())


def classify(snr_db):
    if snr_db >= LOW_NOISE_SNR_DB:
        return "低ノイズ条件（+10dB以上）を満たしています。"
    if snr_db >= HIGH_NOISE_SNR_DB:
        return "高ノイズ条件相当（-5dB程度以上）です。"
    return "想定使用環境（計画書§1.1.5）の範囲外の可能性があります。"


def main():
    args = parse_arguments()

    print("=" * 60)
    print("SNR相当値 計測ツール（計画書 §2.1.3 / §1.3.3 準拠）")
    print("=" * 60)

    try:
        noise = record(args.noise_sec, "無拍手ノイズ区間", args.device)
        claps = record(args.clap_sec, "拍手区間（メトロノームに合わせて連続して手を叩いてください）", args.device)
    except KeyboardInterrupt:
        print("\n中断しました。")
        return 1

    noise_dbfs = rms_dbfs(noise)
    clap_dbfs = rms_dbfs(claps)
    snr_db = clap_dbfs - noise_dbfs

    print("\n" + "=" * 60)
    print("計測結果")
    print("=" * 60)
    print(f"無拍手ノイズ区間 RMS : {noise_dbfs:.2f} dBFS")
    print(f"拍手区間 RMS         : {clap_dbfs:.2f} dBFS")
    print(f"SNR相当値            : {snr_db:.2f} dB")
    print(f"判定                 : {classify(snr_db)}")
    print("=" * 60 + "\n")

    if args.save_dir:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        noise_path = args.save_dir / f"snr_{timestamp}_noise.wav"
        clap_path = args.save_dir / f"snr_{timestamp}_clap.wav"
        save_wav(noise_path, noise)
        save_wav(clap_path, claps)
        print(f"録音データを保存しました:\n  {noise_path}\n  {clap_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
