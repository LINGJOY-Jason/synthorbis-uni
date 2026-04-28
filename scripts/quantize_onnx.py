#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SynthOrbis ONNX 模型量化工具
=============================
支持：
  - FP16 转换（float32 → float16，约减小 50%）
  - INT8 动态量化（无需校准数据，约减小 75%，精度损失 <1%）
  - INT8 静态量化（需要校准数据集，精度最高）

使用方法：
  # FP16 转换
  python scripts/quantize_onnx.py --input model.onnx --output model_fp16.onnx --mode fp16

  # INT8 动态量化（推荐，无需校准数据）
  python scripts/quantize_onnx.py --input model.onnx --output model_int8.onnx --mode int8_dynamic

  # INT8 静态量化（需要校准 wav 文件目录）
  python scripts/quantize_onnx.py --input model.onnx --output model_int8s.onnx --mode int8_static --calib-dir ./calib_audio

  # 全部量化（生成三种版本）
  python scripts/quantize_onnx.py --input model.onnx --all

依赖：
  pip install onnxruntime onnxruntime-extensions onnx onnxconverter-common numpy
"""

import sys
import io
import os
import time
import argparse
import struct
import shutil
from pathlib import Path

# Windows 控制台编码修复
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


# ============================================================
# 依赖检查
# ============================================================

def check_and_install_deps(mode: str):
    """检查并提示安装依赖"""
    import importlib

    required_base = ['onnx', 'onnxruntime', 'numpy']
    required_fp16 = ['onnxconverter_common']
    required_int8 = ['onnxruntime.quantization']

    missing = []
    for pkg in required_base:
        try:
            importlib.import_module(pkg)
        except ImportError:
            missing.append(pkg)

    if mode == 'fp16':
        try:
            importlib.import_module('onnxconverter_common')
        except ImportError:
            missing.append('onnxconverter-common')

    if missing:
        print(f"[ERROR] 缺少依赖: {', '.join(missing)}")
        print(f"[INFO]  请运行: pip install {' '.join(missing)}")
        sys.exit(1)

    print(f"[OK] 依赖检查通过")


# ============================================================
# 工具函数
# ============================================================

def get_model_size_mb(path: str) -> float:
    return os.path.getsize(path) / (1024 * 1024)


def print_model_info(path: str, label: str = ""):
    """打印模型基本信息"""
    import onnx
    model = onnx.load(path)
    size_mb = get_model_size_mb(path)
    opset = model.opset_import[0].version if model.opset_import else '?'
    n_nodes = len(model.graph.node)
    n_init  = len(model.graph.initializer)

    tag = f"[{label}] " if label else ""
    print(f"{tag}文件: {os.path.basename(path)}")
    print(f"{tag}  大小: {size_mb:.1f} MB")
    print(f"{tag}  opset: {opset}  节点数: {n_nodes}  权重张量: {n_init}")

    # 统计 float32 / float16 参数量
    n_fp32 = n_fp16 = n_int8 = n_int16 = 0
    for init in model.graph.initializer:
        import onnx.TensorProto as tp
        # data_type: 1=float32, 10=float16, 2=uint8, 3=int8
        dt = init.data_type
        if dt == 1:   n_fp32 += 1
        elif dt == 10: n_fp16 += 1
        elif dt in (2, 3): n_int8 += 1
        elif dt in (4, 5): n_int16 += 1

    print(f"{tag}  权重精度: FP32={n_fp32}  FP16={n_fp16}  INT8={n_int8}  INT16={n_int16}")
    return size_mb


def benchmark_onnx(model_path: str, n_runs: int = 3) -> float:
    """对模型做简单推理 benchmark（使用合成输入）"""
    import onnxruntime as ort
    import numpy as np

    print(f"\n[BENCH] 加载 {os.path.basename(model_path)} ...")
    sess_opts = ort.SessionOptions()
    sess_opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    sess_opts.intra_op_num_threads = 4

    try:
        sess = ort.InferenceSession(model_path, sess_options=sess_opts,
                                    providers=['CPUExecutionProvider'])
    except Exception as e:
        print(f"[WARN] 无法加载模型进行 benchmark: {e}")
        return -1.0

    # 构造合成输入（SenseVoice：speech=[1,300,560] int32/float16/float32）
    inputs = {}
    for inp in sess.get_inputs():
        shape = [1 if s in ('batch_size', None, 'B') else
                 300 if s in ('T', 'seq_len', None) or (isinstance(s, int) and s < 0) else
                 int(s) if isinstance(s, int) else 1
                 for s in inp.shape]
        # shape 修正：确保特征维度 560
        if 'speech' in inp.name and len(shape) == 3:
            shape = [1, 300, 560]
        elif 'length' in inp.name or inp.name in ('speech_lengths',):
            shape = [1]
        elif inp.name in ('language', 'textnorm'):
            shape = [1]

        if inp.type == 'tensor(float)':
            inputs[inp.name] = np.zeros(shape, dtype=np.float32)
        elif inp.type == 'tensor(float16)':
            inputs[inp.name] = np.zeros(shape, dtype=np.float16)
        elif inp.type == 'tensor(int32)':
            inputs[inp.name] = np.zeros(shape, dtype=np.int32)
        elif inp.type == 'tensor(int64)':
            inputs[inp.name] = np.zeros(shape, dtype=np.int64)
        else:
            inputs[inp.name] = np.zeros(shape, dtype=np.float32)

    # 预热
    try:
        sess.run(None, inputs)
    except Exception:
        pass

    times = []
    for i in range(n_runs):
        t0 = time.time()
        try:
            sess.run(None, inputs)
            times.append(time.time() - t0)
        except Exception as e:
            print(f"[WARN] 推理第 {i+1} 次失败: {e}")

    if times:
        avg_ms = sum(times) / len(times) * 1000
        print(f"[BENCH] 平均推理耗时: {avg_ms:.1f}ms ({n_runs} 次, 合成输入 300帧)")
        return avg_ms
    return -1.0


# ============================================================
# FP16 量化
# ============================================================

def quantize_fp16(input_path: str, output_path: str, keep_io_types: bool = True) -> bool:
    """将 FP32 ONNX 模型转换为 FP16

    Args:
        keep_io_types: True=保持 I/O 为 float32（推荐，避免外部调用方改签名）
    """
    print("\n" + "=" * 60)
    print("模式: FP16 转换")
    print("=" * 60)

    try:
        import onnx
        from onnxconverter_common import float16
    except ImportError as e:
        print(f"[ERROR] 缺少依赖: {e}")
        print("       pip install onnxconverter-common")
        return False

    print(f"[INFO] 加载原始模型: {input_path}")
    model_fp32 = onnx.load(input_path)

    print(f"[INFO] 转换为 FP16 (keep_io_types={keep_io_types}) ...")
    t0 = time.time()
    model_fp16 = float16.convert_float_to_float16(
        model_fp32,
        keep_io_types=keep_io_types,
        disable_shape_infer=False,
        op_block_list=None,  # 所有算子均转换
    )
    elapsed = time.time() - t0
    print(f"[OK] 转换完成，耗时 {elapsed:.2f}s")

    # 验证模型有效性
    try:
        onnx.checker.check_model(model_fp16)
        print(f"[OK] 模型验证通过")
    except Exception as e:
        print(f"[WARN] 模型验证警告（非致命）: {e}")

    onnx.save(model_fp16, output_path)
    print(f"[OK] FP16 模型已保存: {output_path}")
    return True


# ============================================================
# INT8 动态量化
# ============================================================

def quantize_int8_dynamic(input_path: str, output_path: str,
                           weight_type: str = 'uint8') -> bool:
    """INT8 动态量化（权重量化，激活值在运行时动态量化）

    特点：
    - 无需校准数据
    - 权重从 FP32 → INT8，减少约 75% 大小
    - 激活值运行时动态量化，精度损失极小
    - 推理速度提升 2-4x（在支持 VNNI 的 CPU 上）
    """
    print("\n" + "=" * 60)
    print("模式: INT8 动态量化")
    print("=" * 60)

    try:
        from onnxruntime.quantization import quantize_dynamic, QuantType
    except ImportError as e:
        print(f"[ERROR] 缺少依赖: {e}")
        print("       pip install onnxruntime")
        return False

    wt = QuantType.QUInt8 if weight_type == 'uint8' else QuantType.QInt8

    print(f"[INFO] 权重类型: {weight_type.upper()}")
    print(f"[INFO] 开始量化: {input_path}")

    t0 = time.time()
    try:
        quantize_dynamic(
            model_input=input_path,
            model_output=output_path,
            weight_type=wt,
            per_channel=True,          # per-channel 精度更高
            reduce_range=False,        # x86 普通 CPU 用 False
            extra_options={
                'WeightSymmetric': True,
                'ActivationSymmetric': False,
                'EnableSubgraph': True,
            }
        )
    except Exception as e:
        print(f"[ERROR] 量化失败: {e}")
        print("[INFO] 尝试基础模式（关闭 per_channel）...")
        try:
            quantize_dynamic(
                model_input=input_path,
                model_output=output_path,
                weight_type=wt,
                per_channel=False,
            )
        except Exception as e2:
            print(f"[ERROR] 量化仍然失败: {e2}")
            return False

    elapsed = time.time() - t0
    print(f"[OK] 量化完成，耗时 {elapsed:.2f}s")
    print(f"[OK] INT8 模型已保存: {output_path}")
    return True


# ============================================================
# INT8 静态量化（需要校准数据）
# ============================================================

class SenseVoiceCalibDataReader:
    """SenseVoice ONNX 静态量化校准数据读取器"""

    def __init__(self, model_path: str, calib_dir: str, max_samples: int = 50):
        import onnxruntime as ort
        import numpy as np

        self.model_path = model_path
        self.calib_dir = calib_dir
        self.max_samples = max_samples
        self._data_list = []
        self._idx = 0

        # 获取模型输入信息
        sess = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
        self._input_names = [inp.name for inp in sess.get_inputs()]

        # 加载校准音频
        self._load_audio_files()

    def _load_audio_files(self):
        """加载 WAV 文件作为校准数据"""
        import numpy as np

        wav_files = list(Path(self.calib_dir).glob('**/*.wav'))
        if not wav_files:
            print(f"[WARN] 校准目录 {self.calib_dir} 中未找到 WAV 文件，使用合成数据")
            wav_files = []

        count = min(len(wav_files), self.max_samples)
        print(f"[INFO] 找到 {len(wav_files)} 个 WAV 文件，使用 {count} 个进行校准")

        for i in range(self.max_samples):
            if i < len(wav_files):
                # 读取真实 WAV 文件
                feats = self._extract_fbank_from_wav(str(wav_files[i]))
            else:
                # 合成数据补充
                feats = self._make_synthetic_features()

            if feats is not None:
                self._data_list.append(feats)

        print(f"[INFO] 校准数据集大小: {len(self._data_list)} 条")

    def _extract_fbank_from_wav(self, wav_path: str):
        """从 WAV 文件提取 Fbank 特征（近似）"""
        import numpy as np

        try:
            # 读取 WAV（无需 scipy，手动解析 PCM）
            with open(wav_path, 'rb') as f:
                data = f.read()

            # 简单 WAV 解析（PCM 16-bit）
            if data[:4] != b'RIFF':
                return None

            # 找 data chunk
            idx = 12
            pcm = None
            while idx < len(data) - 8:
                chunk_id = data[idx:idx+4]
                chunk_size = struct.unpack_from('<I', data, idx+4)[0]
                if chunk_id == b'data':
                    pcm_bytes = data[idx+8: idx+8+chunk_size]
                    pcm = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32) / 32768.0
                    break
                idx += 8 + chunk_size

            if pcm is None or len(pcm) < 1600:
                return None

            return self._pcm_to_fbank_features(pcm)
        except Exception:
            return None

    def _pcm_to_fbank_features(self, pcm):
        """PCM → Fbank 特征（简化版，用于校准数据）"""
        import numpy as np

        sr = 16000
        frame_shift = 160   # 10ms
        frame_len   = 400   # 25ms
        n_mels      = 80
        stack_size  = 7     # SenseVoice: 7帧堆叠 → 560维

        # 预加重
        pcm = np.concatenate([[pcm[0]], pcm[1:] - 0.97 * pcm[:-1]])

        # 分帧
        n_frames = (len(pcm) - frame_len) // frame_shift + 1
        if n_frames < stack_size:
            return None

        frames = np.stack([pcm[t*frame_shift: t*frame_shift+frame_len]
                           for t in range(n_frames)])

        # Hamming 窗
        window = np.hamming(frame_len)
        frames = frames * window[np.newaxis, :]

        # FFT 功率谱
        n_fft = 512
        spec = np.abs(np.fft.rfft(frames, n=n_fft)) ** 2  # [T, n_fft//2+1]

        # Mel 滤波器组（三角形）
        low_hz, high_hz = 0.0, sr / 2.0
        low_mel  = 2595 * np.log10(1 + low_hz / 700)
        high_mel = 2595 * np.log10(1 + high_hz / 700)
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

        fbank = np.maximum(np.dot(spec, mel_fb), 1e-10)
        log_fbank = np.log(fbank).astype(np.float32)  # [T, 80]

        # 全局归一化（近似）
        log_fbank = (log_fbank - log_fbank.mean()) / (log_fbank.std() + 1e-8)

        # 7 帧堆叠 → [T', 560]（SenseVoice 格式）
        n_stacked = n_frames - stack_size + 1
        stacked = np.concatenate([log_fbank[t: t + n_stacked] for t in range(stack_size)], axis=1)
        # stacked: [n_stacked, 80*7=560]

        # 截断到合理长度（防止过长占用内存）
        max_frames = 600
        stacked = stacked[:max_frames]
        T = stacked.shape[0]

        return {
            'speech':         stacked[np.newaxis, :, :].astype(np.float32),
            'speech_lengths': np.array([T], dtype=np.int32),
            'language':       np.array([0], dtype=np.int32),
            'textnorm':       np.array([1], dtype=np.int32),
        }

    def _make_synthetic_features(self, T: int = 300):
        """生成合成 Fbank 特征（标准正态分布）"""
        import numpy as np
        return {
            'speech':         np.random.randn(1, T, 560).astype(np.float32) * 0.5,
            'speech_lengths': np.array([T], dtype=np.int32),
            'language':       np.array([0], dtype=np.int32),
            'textnorm':       np.array([1], dtype=np.int32),
        }

    # ---------- onnxruntime.quantization 接口 ----------

    def get_next(self):
        if self._idx >= len(self._data_list):
            return None
        item = self._data_list[self._idx]
        self._idx += 1
        # 只返回模型实际需要的输入
        return {k: v for k, v in item.items() if k in self._input_names}

    def rewind(self):
        self._idx = 0


def quantize_int8_static(input_path: str, output_path: str,
                          calib_dir: str, max_samples: int = 50) -> bool:
    """INT8 静态量化（需要校准数据）

    特点：
    - 权重 + 激活值均量化为 INT8
    - 精度比动态量化更高（激活值有精确的量化范围）
    - 需要代表性校准数据（50-200条即可）
    """
    print("\n" + "=" * 60)
    print("模式: INT8 静态量化")
    print("=" * 60)

    try:
        from onnxruntime.quantization import (
            quantize_static, QuantType, QuantFormat,
            CalibrationMethod
        )
        import onnx
    except ImportError as e:
        print(f"[ERROR] 缺少依赖: {e}")
        return False

    # 预处理模型（添加量化注释节点）
    prep_path = input_path.replace('.onnx', '_prep.onnx')

    print(f"[INFO] 预处理模型（添加 QuantizeLinear/DequantizeLinear）...")
    try:
        from onnxruntime.quantization import shape_inference
        shape_inference.quant_pre_process(
            input_model_path=input_path,
            output_model_path=prep_path,
            skip_optimization=False,
        )
    except Exception as e:
        print(f"[WARN] 预处理失败，直接使用原始模型: {e}")
        shutil.copy(input_path, prep_path)

    print(f"[INFO] 创建校准数据读取器 (max_samples={max_samples}) ...")
    calib_reader = SenseVoiceCalibDataReader(prep_path, calib_dir, max_samples)

    print(f"[INFO] 开始静态量化...")
    t0 = time.time()
    try:
        quantize_static(
            model_input=prep_path,
            model_output=output_path,
            calibration_data_reader=calib_reader,
            quant_format=QuantFormat.QDQ,              # QDQ 格式，兼容性最好
            activation_type=QuantType.QInt8,
            weight_type=QuantType.QInt8,
            per_channel=True,
            reduce_range=False,
            calibrate_method=CalibrationMethod.MinMax,  # MinMax 比 Entropy 快
            extra_options={
                'WeightSymmetric': True,
                'ActivationSymmetric': False,
                'EnableSubgraph': True,
                'ForceQuantizeNoInputCheck': False,
            }
        )
    except Exception as e:
        print(f"[ERROR] 静态量化失败: {e}")
        if os.path.exists(prep_path):
            os.remove(prep_path)
        return False

    elapsed = time.time() - t0
    print(f"[OK] 静态量化完成，耗时 {elapsed:.2f}s")
    print(f"[OK] INT8s 模型已保存: {output_path}")

    # 清理中间文件
    if os.path.exists(prep_path) and prep_path != input_path:
        os.remove(prep_path)

    return True


# ============================================================
# 量化效果对比报告
# ============================================================

def generate_comparison_report(original_path: str, quantized_paths: dict,
                                run_bench: bool = True):
    """生成量化前后对比报告"""
    print("\n" + "=" * 60)
    print("量化对比报告")
    print("=" * 60)

    orig_size = print_model_info(original_path, "原始 FP32")
    if run_bench:
        orig_time = benchmark_onnx(original_path)
    else:
        orig_time = -1

    print()
    results = [("原始 FP32", orig_size, orig_time, 1.0, 1.0)]

    for label, path in quantized_paths.items():
        if not os.path.exists(path):
            continue
        print()
        qsize = print_model_info(path, label)
        if run_bench:
            qtime = benchmark_onnx(path)
        else:
            qtime = -1

        size_ratio = qsize / orig_size
        time_ratio = (qtime / orig_time) if orig_time > 0 and qtime > 0 else -1
        results.append((label, qsize, qtime, size_ratio, time_ratio))

    print("\n" + "=" * 60)
    print(f"{'模式':<18} {'大小(MB)':>10} {'推理(ms)':>10} {'大小比率':>10} {'速度比率':>10}")
    print("-" * 60)
    for label, size, t, sr, tr in results:
        t_str  = f"{t:.1f}" if t > 0 else "N/A"
        tr_str = f"{1/tr:.2f}x 快" if tr > 0 else "N/A"
        print(f"{label:<18} {size:>10.1f} {t_str:>10} {sr:>9.1%} {tr_str:>10}")

    print("=" * 60)
    print()
    print("建议：")
    print("  - 生产环境（实时输入法）: INT8 动态量化 → ~230MB，速度提升 2-3x")
    print("  - 离线高精度模式：       INT8 静态量化  → ~220MB，精度最优")
    print("  - 鸿蒙/嵌入式受限设备：  FP16 量化     → ~450MB，兼容性最好")
    print()


# ============================================================
# 主程序
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="SynthOrbis ONNX 模型量化工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('--input', '-i', required=False,
                        default=None,
                        help='输入 ONNX 模型路径 (默认: 自动搜索 ModelScope 缓存)')
    parser.add_argument('--output', '-o', default=None,
                        help='输出路径（--all 模式下作为输出目录）')
    parser.add_argument('--mode', '-m',
                        choices=['fp16', 'int8_dynamic', 'int8_static'],
                        default='int8_dynamic',
                        help='量化模式（默认: int8_dynamic）')
    parser.add_argument('--all', action='store_true',
                        help='生成全部三种量化版本')
    parser.add_argument('--calib-dir', default='./calib_audio',
                        help='INT8 静态量化校准数据目录（含 WAV 文件）')
    parser.add_argument('--calib-samples', type=int, default=50,
                        help='校准样本数（默认 50）')
    parser.add_argument('--keep-io-types', action='store_true', default=True,
                        help='FP16 模式保持 I/O 为 float32（默认开启）')
    parser.add_argument('--no-bench', action='store_true',
                        help='跳过推理 benchmark')
    parser.add_argument('--weight-type', choices=['uint8', 'int8'], default='uint8',
                        help='INT8 动态量化权重类型（默认 uint8）')
    args = parser.parse_args()

    # ---- 自动查找输入模型 ----
    if args.input is None:
        candidates = [
            os.path.expanduser('~/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx/model.onnx'),
            os.path.expanduser('~/.cache/modelscope/hub/iic/SenseVoiceSmall/model.onnx'),
        ]
        for c in candidates:
            if os.path.exists(c):
                args.input = c
                print(f"[INFO] 自动检测到模型: {args.input}")
                break

        if args.input is None:
            print("[ERROR] 未找到 ONNX 模型，请通过 --input 指定路径")
            print("        或先运行: python scripts/test_sensevoice_onnx.py 下载模型")
            sys.exit(1)

    if not os.path.exists(args.input):
        print(f"[ERROR] 输入模型不存在: {args.input}")
        sys.exit(1)

    # ---- 确定输出路径 ----
    input_dir  = os.path.dirname(args.input)
    input_stem = Path(args.input).stem  # 例如 "model"

    if args.all:
        out_dir = args.output if args.output else input_dir
        os.makedirs(out_dir, exist_ok=True)
        fp16_path = os.path.join(out_dir, f"{input_stem}_fp16.onnx")
        int8d_path = os.path.join(out_dir, f"{input_stem}_int8_dynamic.onnx")
        int8s_path = os.path.join(out_dir, f"{input_stem}_int8_static.onnx")
    else:
        if args.output is None:
            suffix_map = {
                'fp16': '_fp16.onnx',
                'int8_dynamic': '_int8_dynamic.onnx',
                'int8_static': '_int8_static.onnx',
            }
            args.output = os.path.join(
                input_dir,
                input_stem + suffix_map.get(args.mode, '_quant.onnx')
            )

    # ---- 打印原始模型信息 ----
    print("\n" + "=" * 60)
    print("SynthOrbis ONNX 量化工具 v1.0")
    print("=" * 60)
    check_and_install_deps(args.mode if not args.all else 'int8_dynamic')
    print()
    print_model_info(args.input, "输入模型")

    # ---- 量化 ----
    if args.all:
        print(f"\n[INFO] 全量化模式，输出目录: {out_dir}")
        results = {}

        # FP16
        if quantize_fp16(args.input, fp16_path, args.keep_io_types):
            results['FP16'] = fp16_path

        # INT8 Dynamic
        if quantize_int8_dynamic(args.input, int8d_path, args.weight_type):
            results['INT8 Dynamic'] = int8d_path

        # INT8 Static
        if quantize_int8_static(args.input, int8s_path, args.calib_dir, args.calib_samples):
            results['INT8 Static'] = int8s_path

        generate_comparison_report(args.input, results, run_bench=not args.no_bench)

    elif args.mode == 'fp16':
        check_and_install_deps('fp16')
        ok = quantize_fp16(args.input, args.output, args.keep_io_types)
        if ok:
            generate_comparison_report(args.input, {'FP16': args.output},
                                       run_bench=not args.no_bench)

    elif args.mode == 'int8_dynamic':
        ok = quantize_int8_dynamic(args.input, args.output, args.weight_type)
        if ok:
            generate_comparison_report(args.input, {'INT8 Dynamic': args.output},
                                       run_bench=not args.no_bench)

    elif args.mode == 'int8_static':
        if not os.path.isdir(args.calib_dir):
            print(f"[WARN] 校准目录不存在: {args.calib_dir}，将使用合成数据")
            os.makedirs(args.calib_dir, exist_ok=True)
        ok = quantize_int8_static(args.input, args.output, args.calib_dir,
                                  args.calib_samples)
        if ok:
            generate_comparison_report(args.input, {'INT8 Static': args.output},
                                       run_bench=not args.no_bench)


if __name__ == '__main__':
    main()
