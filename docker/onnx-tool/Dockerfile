# =============================================
# SynthOrbis UNI — ONNX 模型转换工具镜像
# 用途：将 FunASR PyTorch 模型导出为 ONNX 格式
# 模型：Paraformer-zh / Paraformer-zh-streaming
# =============================================

FROM python:3.10-slim

WORKDIR /workspace

# ── ONNX Runtime + 工具链 ─────────────────────
RUN pip install --no-cache-dir \
    onnxruntime==1.19.2 \
    onnx==1.17.0 \
    onnxconverter-common==1.14.0 \
    onnxsim==0.4.36 \
    onnxruntime-tools==1.15.1 \
    apache-beam>=2.50.0 \
    wget>=4.0.0

# ── FunASR 模型导出工具 ─────────────────────────
# FunASR 官方提供 export_onnx.py，运行时挂载模型目录后执行
# 用法：docker run --rm -v ./models:/workspace/models synthorbis/uni-onnx-tool:latest python export_paraformer_onnx.py

CMD ["python", "-c", "print('SynthOrbis UNI ONNX Tool ready')"]
