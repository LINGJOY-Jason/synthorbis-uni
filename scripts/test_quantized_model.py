#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SynthOrbis 量化模型验证工具
============================
功能：
  1. 自动发现原始模型及所有量化变体（fp16 / int8_dynamic / int8_static）
  2. 对比大小、内存占用、推理速度（合成输入 + 真实音频）
  3. 输出详细的精度/速度/大小 对比表

使用方法：
  # 验证默认 ModelScope 缓存目录中的所有模型
  python scripts/test_quantized_model.py

  # 指定模型目录
  python scripts/test_quantized_model.py --model-dir path/to/models

  # 指定真实音频进行精度对比
  python scripts/test_quantized_model.py --wav test_audio.wav

  # 快速验证（只做大小和单次推理）
  python scripts/test_quantized_model.py --quick
"""

import sys
import io
import os
import time
import argparse
import struct
from pathlib import Path

# Windows 控制台编码修复
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


# ============================================================
# 依赖检查
# ============================================================

def check_deps():
    missing = []
    for pkg in ['numpy', 'onnxruntime']:
        try:
            __import__(pkg)
        except ImportError:
            missing.append(pkg)
    if missing:
        print(f"[ERROR] 缺少依赖: {', '.join(missing)}")
        print(f"        pip install {' '.join(missing)}")
        sys.exit(1)


# ============================================================
# 工具函数
# ============================================================

def find_models(model_dir: str = None) -> dict:
    """在目录中查找所有 ONNX 模型文件"""
    candidates = []

    if model_dir:
        candidates = [model_dir]
    else:
        # 默认搜索位置
        candidates = [
            os.path.expanduser('~/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx'),
            os.path.expanduser('~/.cache/modelscope/hub/iic/SenseVoiceSmall'),
            'd:/models/sensevoice',
            'd:/SynthOrbisUNI/models',
        ]

    models = {}
    label_map = {
        '_fp16':          'FP16',
        '_float16':       'FP16',
        '_int8_static':   'INT8 Static',
        '_int8_dynamic':  'INT8 Dynamic',
        '_int8':          'INT8 Dynamic',
    }

    for base in candidates:
        if not os.path.isdir(base):
            continue

        for f in Path(base).glob('*.onnx'):
            name = f.stem  # e.g. "model", "model_fp16", "model_int8_dynamic"
            label = 'FP32 Original'
            for suffix, lbl in label_map.items():
                if name.endswith(suffix):
                    label = lbl
                    break
            models[label] = str(f)
            print(f"[FOUND] {label:<20} → {f}")

    return models


def get_model_size(path: str) -> float:
    """返回文件大小（MB）"""
    return os.path.getsize(path) / (1024 * 1024)


def load_model(path: str):
    """加载 ONNX 会话"""
    import onnxruntime as ort
    opts = ort.SessionOptions()
    opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    opts.intra_op_num_threads = 4
    try:
        sess = ort.InferenceSession(path, sess_options=opts,
                                    providers=['CPUExecutionProvider'])
        return sess
    except Exception as e:
        print(f"[WARN] 加载失败: {os.path.basename(path)}: {e}")
        return None


# ============================================================
# 特征提取（真实 Fbank）
# ============================================================

def extract_fbank_features(pcm: 'np.ndarray', sr: int = 16000) -> 'np.ndarray':
    """PCM float32 → Fbank [T, 560]（与 fbank.cc 对齐）"""
    import numpy as np

    frame_shift = int(0.010 * sr)   # 160
    frame_len   = int(0.025 * sr)   # 400
    n_mels      = 80
    stack_size  = 7
    preemph     = 0.97

    # 预加重
    pcm = np.concatenate([[pcm[0]], pcm[1:] - preemph * pcm[:-1]])

    n_frames = (len(pcm) - frame_len) // frame_shift + 1
    if n_frames < stack_size:
        return None

    # 分帧 + Hamming 窗
    frames = np.stack([pcm[t*frame_shift: t*frame_shift+frame_len]
                       for t in range(n_frames)]) * np.hamming(frame_len)

    # FFT 功率谱
    n_fft = 512
    spec = np.abs(np.fft.rfft(frames, n=n_fft)) ** 2  # [T, 257]

    # Mel 滤波器组
    low_mel  = 2595 * np.log10(1 + 0.0 / 700)
    high_mel = 2595 * np.log10(1 + (sr / 2.0) / 700)
    mel_pts  = np.linspace(low_mel, high_mel, n_mels + 2)
    hz_pts   = 700 * (10 ** (mel_pts / 2595) - 1)
    bin_pts  = np.floor((n_fft + 1) * hz_pts / sr).astype(int)

    mel_fb = np.zeros((n_fft // 2 + 1, n_mels))
    for m in range(1, n_mels + 1):
        lo, cen, hi = bin_pts[m-1], bin_pts[m], bin_pts[m+1]
        for k in range(lo, cen):
            mel_fb[k, m-1] = (k - lo) / max(cen - lo, 1)
        for k in range(cen, hi):
            mel_fb[k, m-1] = (hi - k) / max(hi - cen, 1)

    log_fbank = np.log(np.maximum(np.dot(spec, mel_fb), 1e-10)).astype(np.float32)
    # 全局 CMVN（近似）
    log_fbank = (log_fbank - log_fbank.mean(0)) / (log_fbank.std(0) + 1e-8)

    # 7 帧堆叠
    n_stacked = n_frames - stack_size + 1
    stacked = np.concatenate([log_fbank[t: t + n_stacked]
                               for t in range(stack_size)], axis=1)
    return stacked.astype(np.float32)  # [T', 560]


def load_wav(wav_path: str):
    """加载 WAV 文件为 PCM float32"""
    import numpy as np
    with open(wav_path, 'rb') as f:
        data = f.read()
    if data[:4] != b'RIFF':
        return None, 16000
    # 解析采样率
    sr = struct.unpack_from('<I', data, 24)[0]
    # 找 data chunk
    idx = 12
    while idx < len(data) - 8:
        chunk_id   = data[idx:idx+4]
        chunk_size = struct.unpack_from('<I', data, idx+4)[0]
        if chunk_id == b'data':
            pcm_bytes = data[idx+8: idx+8+chunk_size]
            pcm = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32) / 32768.0
            return pcm, sr
        idx += 8 + chunk_size
    return None, sr


def make_synthetic_input(T: int = 300) -> dict:
    """生成合成输入（标准正态 Fbank）"""
    import numpy as np
    return {
        'speech':         np.random.randn(1, T, 560).astype(np.float32) * 0.5,
        'speech_lengths': np.array([T], dtype=np.int32),
        'language':       np.array([0], dtype=np.int32),
        'textnorm':       np.array([1], dtype=np.int32),
    }


def make_real_input(wav_path: str) -> dict:
    """从真实 WAV 文件生成输入"""
    import numpy as np
    pcm, sr = load_wav(wav_path)
    if pcm is None:
        print(f"[WARN] 无法解析 WAV: {wav_path}，使用合成输入")
        return make_synthetic_input()

    feats = extract_fbank_features(pcm, sr)
    if feats is None or len(feats) == 0:
        return make_synthetic_input()

    T = min(feats.shape[0], 600)
    feats = feats[:T]
    return {
        'speech':         feats[np.newaxis, :, :],
        'speech_lengths': np.array([T], dtype=np.int32),
        'language':       np.array([0], dtype=np.int32),
        'textnorm':       np.array([1], dtype=np.int32),
    }


# ============================================================
# 推理与评估
# ============================================================

def run_inference(sess, inputs: dict, n_warmup: int = 2, n_bench: int = 5):
    """运行推理并返回 (输出, 平均耗时ms, 峰值内存MB)"""
    import numpy as np

    # 过滤输入
    input_names = [inp.name for inp in sess.get_inputs()]

    # 适配输入类型（FP16 模型的 speech 可能需要 float16）
    run_inputs = {}
    for inp in sess.get_inputs():
        val = inputs.get(inp.name)
        if val is None:
            continue
        if inp.type == 'tensor(float16)' and val.dtype == np.float32:
            val = val.astype(np.float16)
        elif inp.type == 'tensor(int32)' and val.dtype != np.int32:
            val = val.astype(np.int32)
        run_inputs[inp.name] = val

    # 预热
    for _ in range(n_warmup):
        try:
            sess.run(None, run_inputs)
        except Exception:
            pass

    # Benchmark
    times = []
    outputs = None
    for _ in range(n_bench):
        t0 = time.perf_counter()
        try:
            outputs = sess.run(None, run_inputs)
            times.append((time.perf_counter() - t0) * 1000)
        except Exception as e:
            print(f"  [WARN] 推理异常: {e}")

    avg_ms = sum(times) / len(times) if times else -1
    return outputs, avg_ms


def compute_output_diff(out_ref, out_cmp) -> float:
    """计算两个输出之间的最大相对误差（精度指标）"""
    import numpy as np
    if out_ref is None or out_cmp is None:
        return float('nan')
    try:
        r = out_ref[0].astype(np.float32)
        c = out_cmp[0].astype(np.float32)
        if r.shape != c.shape:
            return float('nan')
        diff = np.abs(r - c)
        denom = np.abs(r) + 1e-8
        return float(np.percentile(diff / denom, 99))  # P99 相对误差
    except Exception:
        return float('nan')


# ============================================================
# 主逻辑
# ============================================================

def run_evaluation(models: dict, wav_path: str = None,
                   n_bench: int = 5, quick: bool = False):
    """运行全量评估并打印报告"""
    import numpy as np

    print("\n" + "=" * 70)
    print("SynthOrbis 量化模型评估报告")
    print("=" * 70)

    if not models:
        print("[ERROR] 未找到任何 ONNX 模型，请先运行 quantize_onnx.py 生成量化模型")
        print("        或通过 --model-dir 指定包含模型文件的目录")
        return

    # 准备输入
    if wav_path and os.path.exists(wav_path):
        print(f"[INFO] 使用真实音频: {wav_path}")
        inputs = make_real_input(wav_path)
        T_actual = int(inputs['speech_lengths'][0])
        audio_dur = T_actual * 0.01  # 每帧 10ms
        print(f"[INFO] 音频帧数: {T_actual}  (~{audio_dur:.2f}s)")
    else:
        print("[INFO] 使用合成输入 (300帧 = 3s)")
        inputs = make_synthetic_input(300)
        audio_dur = 3.0

    if quick:
        n_bench = 1

    # 评估每个模型
    rows = []
    ref_outputs = None   # FP32 原始模型输出（用于精度对比）

    for label, path in sorted(models.items(), key=lambda x: x[0]):
        print(f"\n[EVAL] {label}")
        print(f"       路径: {os.path.basename(path)}")

        size_mb = get_model_size(path)
        print(f"       大小: {size_mb:.1f} MB")

        sess = load_model(path)
        if sess is None:
            rows.append((label, size_mb, -1, -1, 'LOAD_FAILED'))
            continue

        outputs, avg_ms = run_inference(sess, inputs, n_warmup=2, n_bench=n_bench)

        # RTF（实时率）
        rtf = (avg_ms / 1000.0) / audio_dur if avg_ms > 0 else -1

        # 精度对比（相对误差）
        if label == 'FP32 Original' and outputs:
            ref_outputs = outputs
            rel_err = 0.0
        else:
            rel_err = compute_output_diff(ref_outputs, outputs)

        if avg_ms > 0:
            print(f"       推理: {avg_ms:.1f}ms  RTF={rtf:.4f}  "
                  f"P99相对误差={'N/A' if ref_outputs is None else f'{rel_err:.4f}'}")
        else:
            print(f"       推理: 失败")

        rows.append((label, size_mb, avg_ms, rtf, rel_err))

    # ---- 打印对比表 ----
    print("\n" + "=" * 70)
    print(f"{'模型':<22} {'大小(MB)':>10} {'推理(ms)':>10} {'RTF':>8} {'P99误差':>10} {'大小比':>8}")
    print("-" * 70)

    fp32_size = None
    fp32_time = None
    for label, size, t, rtf, err in rows:
        if label == 'FP32 Original':
            fp32_size = size
            fp32_time = t

    for label, size, t, rtf, err in rows:
        t_str    = f"{t:.1f}"  if t   > 0   else "FAIL"
        rtf_str  = f"{rtf:.4f}" if rtf > 0  else "N/A"
        err_str  = f"{err:.4f}" if isinstance(err, float) and err >= 0 else "baseline"
        size_str = f"{size / fp32_size:.0%}" if fp32_size else "-"
        print(f"{label:<22} {size:>10.1f} {t_str:>10} {rtf_str:>8} {err_str:>10} {size_str:>8}")

    print("=" * 70)

    # ---- 推荐 ----
    print("\n部署推荐：")
    best_by = {r[0]: r for r in rows if r[2] > 0}
    if 'INT8 Dynamic' in best_by:
        r = best_by['INT8 Dynamic']
        x = f"{fp32_size/r[1]:.1f}x" if fp32_size else '?'
        print(f"  • 实时输入法（CPU）    → INT8 Dynamic  大小 {r[1]:.0f}MB ({x} 压缩)")
    if 'INT8 Static' in best_by:
        r = best_by['INT8 Static']
        print(f"  • 高精度离线识别       → INT8 Static   大小 {r[1]:.0f}MB, P99误差 {r[4]:.4f}")
    if 'FP16' in best_by:
        r = best_by['FP16']
        print(f"  • 鸿蒙/GPU 加速设备    → FP16          大小 {r[1]:.0f}MB, 兼容性最好")
    print()


# ============================================================
# 命令行
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="SynthOrbis 量化模型验证工具")
    parser.add_argument('--model-dir', default=None,
                        help='模型目录（默认搜索 ModelScope 缓存）')
    parser.add_argument('--wav', default=None,
                        help='真实 WAV 文件路径（可选，用于精度对比）')
    parser.add_argument('--bench', type=int, default=5,
                        help='推理次数（默认 5）')
    parser.add_argument('--quick', action='store_true',
                        help='快速模式（仅验证模型可加载性 + 单次推理）')
    args = parser.parse_args()

    check_deps()

    print("\n[INFO] 搜索 ONNX 模型...")
    models = find_models(args.model_dir)

    if not models:
        print("\n[HINT] 未找到模型。请先：")
        print("  1. 运行 python scripts/test_sensevoice_onnx.py  （下载原始模型）")
        print("  2. 运行 python scripts/quantize_onnx.py --all   （生成量化版本）")
        print("  3. 再运行此脚本")
        return

    run_evaluation(models, wav_path=args.wav, n_bench=args.bench, quick=args.quick)


if __name__ == '__main__':
    main()
