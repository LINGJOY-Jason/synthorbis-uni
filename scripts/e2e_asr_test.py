#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SynthOrbis 端到端 ASR 推理验证脚本
===================================
完整流程：Fbank 特征提取 → ONNX 推理 → CTC Greedy 解码 → RTF 性能评估

对应 C++ 模块：
  engine/synthorbis-ai/include/synthorbis/ai/fbank.h
  engine/synthorbis-ai/src/asr/onnx_engine.cc

使用方法：
  python scripts/e2e_asr_test.py

输出：
  - 各模型 RTF 对比（推理耗时 / 音频时长）
  - 识别文本输出
  - 内存占用估算
"""

import sys
import io
import os
import time
import struct
import argparse
from pathlib import Path

# Windows 控制台编码修复
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

import numpy as np
import onnxruntime as ort


# ============================================================
# 路径配置
# ============================================================

DEFAULT_MODEL_DIR = Path(
    os.environ.get(
        'SYNTHORBIS_MODEL_DIR',
        'C:/Users/Administrator/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx'
    )
)
DEFAULT_WAV = DEFAULT_MODEL_DIR / '0.wav'
DEFAULT_TOKENS = DEFAULT_MODEL_DIR / 'tokens.txt'

# 模型变体定义（文件名 → 描述）
# 注：FP16 在 CPU ORT 上无加速效果（ORT CPU EP 无 FP16 GEMM kernel），
#     且部分 Cast 节点类型不兼容。建议仅用于 GPU EP（CUDA/Vulkan）或移动端。
MODEL_VARIANTS = {
    'fp32':    {'suffix': '',             'label': 'FP32（原始）',     'enabled': True},
    'fp16':    {'suffix': '_fp16',         'label': 'FP16（半精度，GPU用）', 'enabled': False},
    'int8_d':  {'suffix': '_int8_dynamic', 'label': 'INT8 动态量化',   'enabled': True},
}


# ============================================================
# Fbank 特征提取（与 fbank.cc 完全对齐）
# ============================================================

class FbankExtractor:
    """
    Fbank 特征提取器 — 与 C++ fbank.cc 对齐规格：

    FbankConfig:
      sample_rate      = 16000
      frame_length_ms   = 25.0      → 400 samples
      frame_shift_ms    = 10.0      → 160 samples
      num_mel_bins     = 80
      low_freq         = 20.0       Hz
      high_freq        = -400.0     → nyquist - 400 Hz = 7600 Hz
      preemph_coeff    = 0.97
      remove_dc_offset = True
      use_log_fbank    = True
      apply_cmvn       = False      （模型内部处理）
      stack_frames     = 7
      stack_stride     = 1

    输出：features [num_frames × 560]（7 帧堆叠）
    """

    def __init__(self,
                 sample_rate: int = 16000,
                 frame_length_ms: float = 25.0,
                 frame_shift_ms: float = 10.0,
                 num_mel_bins: int = 80,
                 low_freq: float = 20.0,
                 high_freq: float = -400.0,
                 preemph_coeff: float = 0.97,
                 stack_frames: int = 7,
                 stack_stride: int = 1):
        self.sample_rate = sample_rate
        self.frame_length = int(sample_rate * frame_length_ms / 1000)
        self.frame_shift  = int(sample_rate * frame_shift_ms  / 1000)
        self.num_mel_bins = num_mel_bins
        self.low_freq = low_freq
        self.high_freq = high_freq if high_freq > 0 else sample_rate // 2 + high_freq
        self.preemph_coeff = preemph_coeff
        self.stack_frames = stack_frames
        self.stack_stride = stack_stride

        # 预计算 Mel 滤波器组
        self._mel_filterbank = self._compute_mel_filterbank()

    def _hertz_to_mel(self, hz: float) -> float:
        """Hz → Mel 尺度"""
        return 2595 * np.log10(1 + hz / 700)

    def _mel_to_hertz(self, mel: float) -> float:
        """Mel → Hz 尺度"""
        return 700 * (10 ** (mel / 2595) - 1)

    def _compute_mel_filterbank(self) -> np.ndarray:
        """计算 Mel 滤波器组矩阵 [n_fft/2+1, n_mels]"""
        n_fft = 512
        n_mels = self.num_mel_bins
        sr = self.sample_rate
        low = self.low_freq
        high = self.high_freq

        # Mel 频率边界
        mel_low  = self._hertz_to_mel(low)
        mel_high = self._hertz_to_mel(high)
        mel_pts  = np.linspace(mel_low, mel_high, n_mels + 2)  # [n_mels+2]

        # Hz 频率点
        hz_pts = self._mel_to_hertz(mel_pts)

        # 映射到 FFT bins
        bin_pts = np.floor((n_fft + 1) * hz_pts / sr).astype(int)
        bin_pts = np.clip(bin_pts, 0, n_fft // 2)

        # 构建三角形滤波器
        fb = np.zeros((n_fft // 2 + 1, n_mels), dtype=np.float32)
        for m in range(n_mels):
            lo, cen, hi = bin_pts[m], bin_pts[m + 1], bin_pts[m + 2]
            for k in range(lo, cen):
                fb[k, m] = (k - lo) / max(cen - lo, 1)
            for k in range(cen, hi):
                fb[k, m] = (hi - k) / max(hi - cen, 1)

        return fb

    def compute(self, audio: np.ndarray) -> np.ndarray:
        """
        计算 Fbank 特征

        Args:
            audio: float32 PCM 数组，范围 [-1, 1]（归一化后）

        Returns:
            features: [num_stacked_frames, 560] 一维展开，
                      7 帧堆叠（stride=1），经对数压缩
        """
        n = len(audio)

        # 1. 预加重
        if self.preemph_coeff != 0:
            emphasized = np.append(audio[0], audio[1:] - self.preemph_coeff * audio[:-1])
        else:
            emphasized = audio

        # 2. 分帧
        num_frames = max(0, (n - self.frame_length) // self.frame_shift + 1)
        frames = np.zeros((num_frames, self.frame_length), dtype=np.float32)
        for i in range(num_frames):
            start = i * self.frame_shift
            frames[i] = emphasized[start: start + self.frame_length]

        # 3. 加 Hamming 窗
        hamming = 0.54 - 0.46 * np.cos(
            2 * np.pi * np.arange(self.frame_length) / (self.frame_length - 1)
        )
        frames *= hamming

        # 4. FFT → 功率谱
        n_fft = 512
        spec = np.abs(np.fft.rfft(frames, n=n_fft)) ** 2  # [T, n_fft/2+1]

        # 5. Mel 滤波器组 → 对数压缩
        log_fbank = np.log(np.dot(spec, self._mel_filterbank) + 1e-10)  # [T, 80]

        # 6. 7 帧堆叠（stride=1）→ [T-6, 560]
        stacked_frames = []
        T = log_fbank.shape[0]
        for t in range(T - self.stack_frames + 1):
            window = log_fbank[t: t + self.stack_frames]  # [7, 80]
            stacked_frames.append(window.flatten())       # [560]

        if not stacked_frames:
            return np.zeros((1, 560), dtype=np.float32)

        return np.array(stacked_frames, dtype=np.float32)  # [num_frames, 560]


def load_wav(wav_path: str) -> tuple[np.ndarray, int]:
    """加载 WAV 文件，返回 (audio_data, sample_rate)"""
    try:
        import scipy.io.wavfile as wf
        sr, raw = wf.read(wav_path)
        if raw.dtype == np.int16:
            audio = raw.astype(np.float32) / 32768.0
        elif raw.dtype == np.int32:
            audio = raw.astype(np.float32) / 2147483648.0
        else:
            audio = raw.astype(np.float32)
        return audio, sr
    except ImportError:
        # Fallback：手动解析 WAV
        with open(wav_path, 'rb') as f:
            data = f.read()
        if data[:4] != b'RIFF':
            raise ValueError(f'不是 WAV 文件: {wav_path}')
        # 找 fmt chunk
        idx = 12
        fmt_found = False
        channels = 1
        bits_per_sample = 16
        while idx < len(data) - 8:
            chunk_id = data[idx:idx+4]
            chunk_size = struct.unpack('<I', data[idx+4:idx+8])[0]
            if chunk_id == b'fmt ':
                channels = struct.unpack('<H', data[idx+10:idx+12])[0]
                bits_per_sample = struct.unpack('<H', data[idx+22:idx+24])[0]
                fmt_found = True
            elif chunk_id == b'data':
                if fmt_found:
                    pcm_bytes = data[idx+8: idx+8+chunk_size]
                    pcm = np.frombuffer(pcm_bytes,
                                        dtype=np.int16 if bits_per_sample <= 16 else np.int32)
                    audio = pcm.astype(np.float32) / (32768.0 if bits_per_sample <= 16 else 2147483648.0)
                    # 多声道 → 单声道
                    if channels > 1:
                        audio = audio.reshape(-1, channels).mean(axis=1)
                    return audio, 16000
            idx += 8 + chunk_size
        raise ValueError('cannot parse WAV: ' + str(wav_path))


def load_tokens(tokens_path: str) -> list[str]:
    """加载 tokens 词表"""
    tokens = []
    try:
        with open(tokens_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip('\r\n')
                if line:
                    tokens.append(line)
    except UnicodeDecodeError:
        with open(tokens_path, 'r', encoding='gbk') as f:
            for line in f:
                line = line.strip('\r\n')
                if line:
                    tokens.append(line)
    return tokens


def greedy_decode(logits: np.ndarray, vocab: list[str]) -> str:
    """
    CTC 贪婪解码

    Args:
        logits: [time, vocab] float32 log-prob
        vocab:  token 列表

    Returns:
        解码文本
    """
    indices = np.argmax(logits, axis=-1)  # [time]
    text = []
    prev = -1
    for idx in indices:
        idx = int(idx)
        if idx != prev and idx != 0 and idx < len(vocab):
            text.append(vocab[idx])
        prev = idx
    return ''.join(text)


# ============================================================
# ONNX 模型推理封装
# ============================================================

class ModelRunner:
    """单个 ONNX 模型的推理 Runner"""

    def __init__(self, model_path: str, label: str):
        self.model_path = model_path
        self.label = label
        self.size_mb = os.path.getsize(model_path) / (1024 * 1024)
        self.session = None
        self.input_names = []
        self.output_names = []
        self.load_time = 0.0
        self.inference_time = 0.0

    def load(self):
        """加载 ONNX Session"""
        t0 = time.perf_counter()
        sess_opts = ort.SessionOptions()
        sess_opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        sess_opts.intra_op_num_threads = 4
        sess_opts.inter_op_num_threads = 1
        self.session = ort.InferenceSession(self.model_path, sess_opts,
                                            providers=['CPUExecutionProvider'])
        self.load_time = time.perf_counter() - t0

        for inp in self.session.get_inputs():
            self.input_names.append(inp.name)
        for out in self.session.get_outputs():
            self.output_names.append(out.name)

    def infer(self, features: np.ndarray,
              language: int = 0, textnorm: int = 1) -> tuple[np.ndarray, float]:
        """
        运行推理

        Returns:
            (ctc_logits [1, T, V], inference_time_seconds)
        """
        T = features.shape[0]
        speech = features[np.newaxis, :, :]  # [1, T, 560]

        inputs = {
            self.input_names[0]: speech,
            self.input_names[1]: np.array([T], dtype=np.int32),
            self.input_names[2]: np.array([language], dtype=np.int32),
            self.input_names[3]: np.array([textnorm], dtype=np.int32),
        }

        t0 = time.perf_counter()
        outputs = self.session.run(None, inputs)
        elapsed = time.perf_counter() - t0

        # ctc_logits: [1, T, vocab]
        return outputs[0], elapsed


# ============================================================
# 主测试流程
# ============================================================

def run_tests(wav_path: str = None, tokens_path: str = None,
              model_dir: str = None,
              warmup: int = 1, runs: int = 3):
    """运行所有模型变体的端到端测试"""

    model_dir = Path(model_dir) if model_dir else DEFAULT_MODEL_DIR
    wav_path  = Path(wav_path) if wav_path  else DEFAULT_WAV
    tokens_path = Path(tokens_path) if tokens_path else DEFAULT_TOKENS

    print("=" * 70)
    print("SynthOrbis ASR 端到端推理验证")
    print("=" * 70)

    # ---- 音频加载 ----
    print(f"\n[1] 加载音频: {wav_path}")
    audio, sr = load_wav(str(wav_path))
    duration_s = len(audio) / sr
    print(f"    采样率: {sr} Hz  时长: {duration_s:.2f}s  采样点: {len(audio)}")

    # ---- Fbank 特征提取 ----
    print(f"\n[2] Fbank 特征提取（与 fbank.cc 对齐规格）")
    fbank = FbankExtractor(
        sample_rate=sr,
        frame_length_ms=25.0,
        frame_shift_ms=10.0,
        num_mel_bins=80,
        low_freq=20.0,
        high_freq=-400.0,
        preemph_coeff=0.97,
        stack_frames=7,
    )
    features = fbank.compute(audio)
    n_frames = features.shape[0]
    print(f"    帧数: {n_frames}  特征维度: {features.shape[1]}")

    # ---- 词表加载 ----
    print(f"\n[3] 加载词表: {tokens_path}")
    vocab = load_tokens(str(tokens_path))
    print(f"    词表大小: {len(vocab)}")

    # ---- 加载所有模型 ----
    print(f"\n[4] 加载模型变体（{model_dir}）")
    runners = {}
    base_model = str(model_dir / 'model.onnx')

    for key, info in MODEL_VARIANTS.items():
        if not info['enabled']:
            continue
        model_path = base_model.replace('.onnx', f"{info['suffix']}.onnx")
        if not os.path.exists(model_path):
            print(f"    [SKIP] {info['label']} 未找到: {Path(model_path).name}")
            continue
        runner = ModelRunner(model_path, info['label'])
        runner.load()
        runners[key] = runner
        print(f"    [OK] {info['label']}: {runner.size_mb:.1f} MB "
              f"加载耗时 {runner.load_time:.2f}s")

    if not runners:
        print("    [ERROR] 没有可用的模型！")
        return

    # ---- 推理测试 ----
    print(f"\n[5] 推理测试（warmup={warmup}, runs={runs}）")

    results = {}
    for key, runner in runners.items():
        print(f"\n    [{runner.label}]")

        # Warmup
        for _ in range(warmup):
            runner.infer(features)

        # 正式测试
        times = []
        ctc_logits = None
        for i in range(runs):
            logits, t = runner.infer(features)
            times.append(t)
            ctc_logits = logits[0]  # [T, V]

        avg_time = sum(times) / len(times)
        rtf = avg_time / duration_s

        runner.inference_time = avg_time

        # 贪婪解码
        text = greedy_decode(ctc_logits, vocab)
        results[key] = {
            'label': runner.label,
            'size_mb': runner.size_mb,
            'load_time': runner.load_time,
            'inference_time': avg_time,
            'rtf': rtf,
            'text': text,
        }

        print(f"      推理耗时: {avg_time*1000:.1f} ms")
        print(f"      RTF: {rtf:.4f}  ({'实时' if rtf < 1.0 else '非实时'})")
        print(f"      文本: {text[:80]}{'...' if len(text) > 80 else ''}")

    # ---- 结果汇总表 ----
    print("\n" + "=" * 70)
    print("结果汇总")
    print("=" * 70)
    print(f"{'模型':<18} {'大小':>8} {'加载':>8} {'推理':>8} {'RTF':>8}  {'实时性':>6}")
    print("-" * 70)
    for key, r in results.items():
        realtime = '✓ 实时' if r['rtf'] < 1.0 else '✗ 非实时'
        print(f"{r['label']:<18} {r['size_mb']:>7.1f}MB "
              f"{r['load_time']:>7.2f}s "
              f"{r['inference_time']*1000:>7.1f}ms "
              f"{r['rtf']:>8.4f}  {realtime:>6}")

    # RTF 分析
    fp32_rtf = results.get('fp32', {}).get('rtf', 1.0)
    print("\n" + "-" * 70)
    print("RTF 加速比（相对于 FP32）：")
    for key, r in results.items():
        if key == 'fp32':
            continue
        speedup = fp32_rtf / r['rtf'] if r['rtf'] > 0 else 0
        print(f"  {r['label']}: {speedup:.2f}x {'↑ 更快' if speedup > 1 else '↓ 更慢'}")

    # 大小分析
    fp32_size = results.get('fp32', {}).get('size_mb', 893)
    print("\n大小压缩比（相对于 FP32）：")
    for key, r in results.items():
        if key == 'fp32':
            continue
        ratio = fp32_size / r['size_mb'] if r['size_mb'] > 0 else 0
        saving = (1 - r['size_mb'] / fp32_size) * 100
        print(f"  {r['label']}: {ratio:.2f}x  (节省 {saving:.0f}%)")

    # 部署推荐
    print("\n" + "=" * 70)
    print("部署推荐")
    print("=" * 70)
    for key, r in results.items():
        if r['rtf'] < 1.0:
            note = ''
            if key == 'int8_d':
                note = ' ← 【输入法生产推荐】'
            elif key == 'fp16':
                note = ' ← 【GPU/鸿蒙加速推荐】'
            elif key == 'fp32':
                note = ' ← 【精度基准】'
            print(f"  {r['label']}: {r['size_mb']:.0f}MB RTF={r['rtf']:.4f}{note}")

    # 文本对比
    print("\n" + "=" * 70)
    print("识别文本对比")
    print("=" * 70)
    texts = {key: r['text'] for key, r in results.items()}
    ref = texts.get('fp32', '')
    for key, text in texts.items():
        match = '✓' if text == ref else '✗'
        print(f"  {match} {results[key]['label']}: {text or '(空)'}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='SynthOrbis ASR 端到端测试')
    parser.add_argument('--wav',      default=None, help='测试音频路径')
    parser.add_argument('--tokens',   default=None, help='词表路径')
    parser.add_argument('--model-dir',default=None, help='模型目录')
    parser.add_argument('--warmup',   type=int, default=1, help='Warmup 轮数')
    parser.add_argument('--runs',     type=int, default=3, help='正式推理轮数')
    args = parser.parse_args()

    run_tests(wav_path=args.wav, tokens_path=args.tokens,
              model_dir=args.model_dir,
              warmup=args.warmup, runs=args.runs)
