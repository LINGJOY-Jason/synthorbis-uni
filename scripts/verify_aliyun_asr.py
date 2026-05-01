#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_aliyun_asr.py — 阿里云 NLS 一句话识别 API 验证脚本
============================================================

用法：
  # 直接传参
  python verify_aliyun_asr.py --appkey <APPKEY> --token <NLS_TOKEN>

  # 或用环境变量
  $Env:NLS_APPKEY = "your_appkey"
  $Env:NLS_TOKEN  = "your_token"
  python verify_aliyun_asr.py

  # 完整可选参数
  python verify_aliyun_asr.py \\
      --appkey <APPKEY> \\
      --token  <NLS_TOKEN> \\
      --region cn-shanghai \\       # 或 cn-beijing / cn-shenzhen
      --audio  path/to/audio.wav    # 可选，默认生成合成音

NLS Token 获取方式：
  方案 A（控制台一次性 Token，有效期 24 小时，测试用）：
    https://help.aliyun.com/zh/isi/obtain-a-token

  方案 B（SDK 接口，生产用，使用 AccessKeyId + AccessKeySecret 换取）：
    pip install alibabacloud_nls20180628
    # 或直接 REST 调用：
    #   POST https://nls-meta.cn-shanghai.aliyuncs.com/
    #        ?Action=CreateToken
    #   Header: Authorization: ...（AK签名）
    # 返回 {"Token":{"Id":"xxx","ExpireTime":...}}
"""

import sys
import io

# 强制 UTF-8 输出（兼容 Windows GBK 控制台）
if sys.stdout.encoding and sys.stdout.encoding.lower() not in ('utf-8', 'utf8'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
if sys.stderr.encoding and sys.stderr.encoding.lower() not in ('utf-8', 'utf8'):
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

import argparse
import os
import struct
import math
import wave
import json
import time
import urllib.request
import urllib.error
import ssl

# =============================================================================
# 颜色输出
# =============================================================================

class C:
    OK   = "\033[92m"
    FAIL = "\033[91m"
    WARN = "\033[93m"
    INFO = "\033[94m"
    BOLD = "\033[1m"
    END  = "\033[0m"

def ok(msg):   print(f"{C.OK}  [OK] {msg}{C.END}")
def fail(msg): print(f"{C.FAIL}  [X] {msg}{C.END}")
def warn(msg): print(f"{C.WARN}  [!] {msg}{C.END}")
def info(msg): print(f"{C.INFO}  --> {msg}{C.END}")
def bold(msg): print(f"{C.BOLD}{msg}{C.END}")

# =============================================================================
# 合成测试 WAV（PCM 16kHz 16bit Mono）
# =============================================================================

def generate_test_wav(duration_s: float = 3.0,
                      sample_rate: int = 16000,
                      text_hint: str = "你好") -> bytes:
    """
    生成一段 WAV 格式的合成音频（多频叠加正弦波）。
    注意：这是纯音调，ASR 识别结果可能为空或乱码，
    主要用于验证 API 可达性和鉴权是否正确。
    """
    num_samples = int(duration_s * sample_rate)
    # 叠加几个谐波模拟语音频率 (200-800 Hz)
    freqs = [200.0, 400.0, 600.0, 800.0]
    samples = []
    for i in range(num_samples):
        t = i / sample_rate
        v = sum(math.sin(2 * math.pi * f * t) for f in freqs)
        v /= len(freqs)          # 归一化到 [-1, 1]
        v = max(-1.0, min(1.0, v))
        samples.append(int(v * 32767))

    # 写 WAV
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)       # int16
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f"<{num_samples}h", *samples))
    return buf.getvalue()


def load_wav_as_pcm_bytes(filepath: str) -> bytes:
    """
    从文件加载 WAV，自动转换为 PCM int16 16kHz 单声道。
    简单实现：直接读 WAV bytes（阿里云 NLS 支持直接传 WAV）。
    """
    with open(filepath, 'rb') as f:
        return f.read()

# =============================================================================
# 阿里云 NLS REST API 调用
# =============================================================================

REGION_ENDPOINTS = {
    "cn-shanghai": "https://nls-gateway-cn-shanghai.aliyuncs.com/stream/v1/asr",
    "cn-beijing":  "https://nls-gateway-cn-beijing.aliyuncs.com/stream/v1/asr",
    "cn-shenzhen": "https://nls-gateway-cn-shenzhen.aliyuncs.com/stream/v1/asr",
}


def call_aliyun_nls(
    appkey: str,
    token: str,
    audio_bytes: bytes,
    sample_rate: int = 16000,
    region: str = "cn-shanghai",
    timeout: int = 15,
) -> dict:
    """
    调用阿里云 NLS 一句话识别 REST API。

    Returns:
        dict with keys:
          - status_code (int): HTTP 状态码
          - body (str): 原始响应体
          - result (str): 识别文本（成功时）
          - elapsed_ms (float): 耗时毫秒
          - error (str): 错误信息（失败时）
    """
    base_url = REGION_ENDPOINTS.get(region, REGION_ENDPOINTS["cn-shanghai"])
    url = (
        f"{base_url}"
        f"?appkey={appkey}"
        f"&format=pcm"
        f"&sample_rate={sample_rate}"
        f"&enable_punctuation_prediction=true"
        f"&enable_inverse_text_normalization=true"
    )

    headers = {
        "X-NLS-Token": token,
        "Content-Type": "application/octet-stream",
        "Content-Length": str(len(audio_bytes)),
    }

    req = urllib.request.Request(url, data=audio_bytes, headers=headers, method="POST")

    t0 = time.monotonic()
    result = {
        "status_code": 0,
        "body": "",
        "result": "",
        "elapsed_ms": 0.0,
        "error": "",
    }

    try:
        # 忽略 SSL 证书错误（企业内网代理场景）
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
            body_bytes = resp.read()
            result["status_code"] = resp.status
            result["body"] = body_bytes.decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        result["status_code"] = e.code
        try:
            result["body"] = e.read().decode("utf-8", errors="replace")
        except Exception:
            result["body"] = str(e)
        result["error"] = f"HTTP {e.code}: {e.reason}"
    except urllib.error.URLError as e:
        result["error"] = f"URLError: {e.reason}"
    except Exception as e:
        result["error"] = f"Exception: {e}"

    result["elapsed_ms"] = (time.monotonic() - t0) * 1000.0

    # 解析 JSON 响应
    if result["body"]:
        try:
            parsed = json.loads(result["body"])
            result["result"]     = parsed.get("result", "")
            result["nls_status"] = parsed.get("status", -1)
            result["message"]    = parsed.get("message", "")
            result["task_id"]    = parsed.get("task_id", "")
        except json.JSONDecodeError:
            result["error"] = "JSON 解析失败: " + result["body"][:200]

    return result

# =============================================================================
# 主逻辑
# =============================================================================

def print_banner():
    bold("\n╔══════════════════════════════════════════════════════╗")
    bold("║   SynthOrbis UNI — 阿里云 NLS ASR 验证工具           ║")
    bold("╚══════════════════════════════════════════════════════╝\n")

def print_step(n, total, title):
    print(f"\n{C.BOLD}[{n}/{total}] {title}{C.END}")
    print("─" * 55)


def main():
    print_banner()

    parser = argparse.ArgumentParser(
        description="阿里云 NLS 一句话识别 API 验证",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例：
  python verify_aliyun_asr.py --appkey abc123 --token xxx

NLS Token 获取（控制台临时 Token，24h 有效期）：
  https://help.aliyun.com/zh/isi/obtain-a-token
""",
    )
    parser.add_argument("--appkey", default=os.environ.get("NLS_APPKEY", ""),
                        help="阿里云 NLS Appkey（控制台项目 Key）")
    parser.add_argument("--token", default=os.environ.get("NLS_TOKEN", ""),
                        help="阿里云 NLS Token（24h 临时 Token）")
    parser.add_argument("--region", default="cn-shanghai",
                        choices=["cn-shanghai", "cn-beijing", "cn-shenzhen"],
                        help="NLS 节点地区（默认 cn-shanghai）")
    parser.add_argument("--audio", default="",
                        help="本地 WAV 文件路径（可选，不提供则使用合成音）")
    parser.add_argument("--sample-rate", type=int, default=16000,
                        help="采样率（默认 16000）")
    args = parser.parse_args()

    # ─── Step 1: 检查凭证 ─────────────────────────────────────────────
    print_step(1, 4, "检查凭证")

    missing = []
    if not args.appkey:
        missing.append("--appkey / $NLS_APPKEY")
    if not args.token:
        missing.append("--token / $NLS_TOKEN")

    if missing:
        fail("缺少必要凭证：")
        for m in missing:
            print(f"       {C.WARN}{m}{C.END}")
        print()
        print(f"  {C.BOLD}获取方式：{C.END}")
        print("  1. 登录 https://nls-portal.console.aliyun.com/")
        print("  2. 创建项目 → 获取 Appkey")
        print("  3. 控制台页面获取临时 Token（24h 有效）")
        print("     或用 SDK: aliyun-python-sdk-nls-meta → CreateToken")
        print()
        print(f"  {C.INFO}然后运行：{C.END}")
        print(f"  python verify_aliyun_asr.py --appkey <KEY> --token <TOKEN>\n")
        sys.exit(1)

    ok(f"Appkey: {args.appkey[:6]}{'*' * max(0, len(args.appkey)-6)}")
    ok(f"Token:  {args.token[:8]}{'*' * max(0, len(args.token)-8)}")
    ok(f"Region: {args.region}")

    # ─── Step 2: 准备音频 ─────────────────────────────────────────────
    print_step(2, 4, "准备测试音频")

    if args.audio:
        if not os.path.exists(args.audio):
            fail(f"文件不存在: {args.audio}")
            sys.exit(1)
        audio_bytes = load_wav_as_pcm_bytes(args.audio)
        duration_s = len(audio_bytes) / (args.sample_rate * 2)  # int16 = 2 bytes
        ok(f"加载文件: {args.audio} ({len(audio_bytes)/1024:.1f} KB, ~{duration_s:.1f}s)")
    else:
        warn("未指定音频文件，使用合成音调（识别结果可能为空，主要验证 API 可达性）")
        audio_bytes = generate_test_wav(duration_s=3.0, sample_rate=args.sample_rate)
        ok(f"合成音频: 3.0s, {args.sample_rate}Hz PCM, {len(audio_bytes)/1024:.1f} KB")

        # 也可以保存用于本地检查
        out_path = os.path.join(os.path.dirname(__file__), "..", "test_audio_aliyun.wav")
        with open(out_path, 'wb') as f:
            f.write(audio_bytes)
        info(f"已保存测试音频: {os.path.abspath(out_path)}")

    # ─── Step 3: 调用 API ─────────────────────────────────────────────
    print_step(3, 4, f"调用阿里云 NLS API ({args.region})")
    endpoint = REGION_ENDPOINTS.get(args.region, REGION_ENDPOINTS["cn-shanghai"])
    info(f"Endpoint: {endpoint}")
    info(f"发送 {len(audio_bytes)/1024:.1f} KB WAV → ASR...")

    resp = call_aliyun_nls(
        appkey=args.appkey,
        token=args.token,
        audio_bytes=audio_bytes,
        sample_rate=args.sample_rate,
        region=args.region,
    )

    # ─── Step 4: 分析结果 ─────────────────────────────────────────────
    print_step(4, 4, "验证结果")

    print(f"  HTTP 状态码:    {resp['status_code']}")
    print(f"  耗时:           {resp['elapsed_ms']:.0f} ms")

    if resp.get("task_id"):
        print(f"  Task ID:        {resp['task_id']}")
    if resp.get("message"):
        print(f"  NLS Message:    {resp['message']}")
    if resp.get("nls_status") is not None:
        print(f"  NLS Status:     {resp['nls_status']}")

    print()
    print(f"  原始响应体:")
    raw = resp["body"]
    if raw:
        try:
            parsed = json.loads(raw)
            print("  " + json.dumps(parsed, ensure_ascii=False, indent=4).replace("\n", "\n  "))
        except Exception:
            print(f"  {raw[:500]}")
    else:
        print(f"  (空)")

    print()

    # 判断结果
    if resp["error"] and not resp["status_code"]:
        fail(f"网络/连接错误: {resp['error']}")
        print()
        print("  可能原因:")
        print("  - 网络不可达（检查防火墙/代理）")
        print("  - SSL 证书问题（内网环境）")
        sys.exit(2)

    if resp["status_code"] == 200 and resp.get("nls_status") == 20000000:
        result_text = resp["result"]
        if result_text:
            ok(f"识别成功！文本: 「{result_text}」")
        else:
            warn("API 调用成功（HTTP 200 + NLS 20000000），但识别结果为空")
            warn("→ 合成音调无法被识别为语音，属于正常现象")
            warn("→ 请使用真实语音 WAV 文件：--audio your_speech.wav")
            ok("✅ API 鉴权和连通性验证通过！")

    elif resp["status_code"] == 200 and resp.get("nls_status") == 40000001:
        fail("鉴权失败: Token 无效或已过期")
        print("  解决方案:")
        print("  1. 重新获取 Token（有效期仅 24 小时）")
        print("  2. 检查 Appkey 与 Token 是否匹配同一项目")
        sys.exit(3)

    elif resp["status_code"] == 403:
        fail("HTTP 403 Forbidden — 访问被拒绝")
        print("  检查 NLS Token 是否正确、是否过期")
        sys.exit(3)

    elif resp["status_code"] == 400:
        fail(f"HTTP 400 Bad Request — 请求参数错误")
        print(f"  响应: {resp['body'][:300]}")
        sys.exit(4)

    elif resp["status_code"] != 0:
        fail(f"未预期的 HTTP 状态码: {resp['status_code']}")
        if resp.get("error"):
            fail(f"错误: {resp['error']}")
        sys.exit(5)

    print()
    bold("═" * 55)
    ok("验证完成！阿里云 NLS ASR API 可用。")
    print()
    print(f"  {C.BOLD}C++ 配置方式：{C.END}")
    print(f"  CloudAsrConfig cfg;")
    print(f"  cfg.provider    = CloudProvider::AliyunNLS;")
    print(f"  cfg.nls_appkey  = \"{args.appkey}\";")
    print(f"  cfg.nls_token   = \"<your_token>\";  // 需定期刷新")
    print(f"  cfg.nls_region  = \"{args.region}\";")
    print(f"  cfg.sample_rate = {args.sample_rate};")
    print()


if __name__ == "__main__":
    main()
