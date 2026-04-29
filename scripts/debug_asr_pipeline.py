#!/usr/bin/env python3
"""
快速诊断：C++ ASR 管线 vs Python 参考实现对比
检查 Fbank 帧数、ORT 推理耗时异常原因
"""

import numpy as np
import time
import wave
import os
import sys

# 平台适配路径
if sys.platform == "win32":
    MODEL_DIR = r"C:\Users\Administrator\.cache\modelscope\hub\manyeyes\sensevoice-small-onnx"
else:
    MODEL_DIR = "/mnt/c/Users/Administrator/.cache/modelscope/hub/manyeyes/sensevoice-small-onnx"
WAV_PATH = os.path.join(MODEL_DIR, "0.wav")
MODEL_PATH = os.path.join(MODEL_DIR, "model.onnx")
TOKENS_PATH = os.path.join(MODEL_DIR, "tokens.txt")

# 1. 读取音频
def read_wav(path):
    with wave.open(path, 'rb') as wf:
        n = wf.getnframes()
        sr = wf.getframerate()
        raw = wf.readframes(n)
    pcm = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    return pcm, sr

pcm, sr = read_wav(WAV_PATH)
print(f"Audio: {len(pcm)} samples @ {sr} Hz = {len(pcm)/sr:.2f}s")

# 2. Fbank 特征提取（Python 参考）
def compute_fbank(pcm, sr=16000, n_mels=80, frame_len=400, frame_shift=160):
    """简化版 Fbank，与 C++ 实现对齐"""
    import numpy as np
    
    # 预加重
    preemph = 0.97
    pcm_em = np.append(pcm[0], pcm[1:] - preemph * pcm[:-1])
    
    # 分帧
    frames = []
    for start in range(0, len(pcm_em) - frame_len + 1, frame_shift):
        frames.append(pcm_em[start:start + frame_len])
    
    print(f"Frames: {len(frames)} (frame_len={frame_len}, shift={frame_shift})")
    
    # 加窗 + FFT
    win = np.hamming(frame_len)
    n_fft = 512
    
    # Mel 滤波器
    low_hz = 20.0
    high_hz = sr / 2.0 - 400
    low_mel = 2595 * np.log10(1 + low_hz / 700)
    high_mel = 2595 * np.log10(1 + high_hz / 700)
    mel_points = np.linspace(low_mel, high_mel, n_mels + 2)
    hz_points = 700 * (10 ** (mel_points / 2595) - 1)
    bin_points = np.floor((n_fft + 1) * hz_points / sr).astype(int)
    
    fbank = np.zeros((len(frames), n_mels), dtype=np.float32)
    for i, frame in enumerate(frames):
        spec = np.abs(np.fft.rfft(frame * win, n=n_fft)) ** 2
        for j in range(n_mels):
            left = bin_points[j]
            center = bin_points[j+1]
            right = bin_points[j+2]
            for k in range(left, center):
                if center > left:
                    fbank[i, j] += spec[k] * (k - left) / (center - left)
            for k in range(center, right):
                if right > center:
                    fbank[i, j] += spec[k] * (right - k) / (right - center)
        fbank[i] = np.log(np.maximum(fbank[i], 1e-7))
    
    return fbank

print("\n--- Fbank 特征提取 ---")
t0 = time.time()
fbank = compute_fbank(pcm, sr)
t1 = time.time()
print(f"Fbank shape: {fbank.shape} ({(t1-t0)*1000:.1f}ms)")

# 3. 7帧堆叠 → 560维
def stack_frames(fbank, stack=7, stride=1):
    out = []
    pad = stack // 2
    padded = np.pad(fbank, ((pad, pad), (0, 0)), mode='edge')
    for i in range(len(fbank)):
        chunk = padded[i:i+stack].reshape(-1)
        out.append(chunk)
    return np.array(out, dtype=np.float32)

stacked = stack_frames(fbank)
print(f"Stacked: {stacked.shape}  (expected: [N, 560])")

# 4. ORT 推理
import onnxruntime as ort
sess = ort.InferenceSession(MODEL_PATH, providers=['CPUExecutionProvider'])

speech = stacked[np.newaxis, :, :]  # [1, T, 560]
speech_lengths = np.array([stacked.shape[0]], dtype=np.int32)
language = np.array([0], dtype=np.int32)  # auto
textnorm = np.array([0], dtype=np.int32)  # no norm

print(f"\n--- ORT 推理 ---")
print(f"Input shape: {speech.shape}")
t0 = time.time()
out = sess.run(None, {
    'speech': speech,
    'speech_lengths': speech_lengths,
    'language': language,
    'textnorm': textnorm
})
t1 = time.time()
logits = out[0]  # [1, T, vocab]
print(f"Output shape: {logits.shape}  ({(t1-t0)*1000:.1f}ms)")

# 5. CTC Greedy 解码
tokens = open(TOKENS_PATH).read().strip().split('\n')
print(f"Vocab size: {len(tokens)}")

best = np.argmax(logits[0], axis=-1)
blank_id = 0
# 找 blank token
for i, t in enumerate(tokens):
    if t == '<blank>' or t == '_' or t == '<blk>':
        blank_id = i
        break

print(f"Blank id: {blank_id}")

# CTC collapse
prev = -1
result = []
for t in best:
    if t != blank_id and t != prev:
        result.append(t)
    prev = t

decoded = ''.join(tokens[i] for i in result if i < len(tokens))
print(f"\n识别结果: '{decoded}'")
print(f"Token ids: {result[:20]}")
print(f"Top 5 tokens at frame 0: {np.argsort(logits[0,0])[-5:][::-1]} -> {[tokens[i] for i in np.argsort(logits[0,0])[-5:][::-1]]}")
