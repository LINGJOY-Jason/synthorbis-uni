# SynthOrbis UNI — 技术选型决策文档

> 版本：v1.0  
> 确认日期：2026-04-26  
> 状态：✅ 全部决策已锁定，可进入开发

---

## 决策总览

| # | 决策项 | 选型结果 | 状态 |
|---|--------|---------|------|
| 1 | UI 框架（配置中心） | **Qt** | ✅ 已确认 |
| 2 | 端侧 ASR 模型 | **FunASR** | ✅ 已确认 |
| 3 | 模型推理格式 | **ONNX Runtime** | ✅ 已确认 |
| 4 | 车机端平台 | **HarmonyOS（鸿蒙）** | ✅ 已确认 |
| 5 | 开源策略 | **librime MIT + 上层闭源** | ✅ 已确认 |

---

## 决策 1：UI 框架 → Qt

### 选型原因
- Qt 是目前唯一一个真正覆盖 Windows / macOS / Linux / OpenHarmony 的 C++ GUI 框架
- 配置中心 UI（候选窗、设置面板）可共用同一套 C++ 代码，大幅降低维护成本
- Qt 5.15 LTS + Qt for OpenHarmony Alpha v8 已支持鸿蒙平台（arm64-v8a，HarmonyOS API 12）
- 与 librime 同为 C++ 生态，无语言边界开销

### 具体版本
- **桌面端**：Qt 6.7 LTS（Windows / macOS / Linux）
- **鸿蒙车机端**：Qt for OpenHarmony 5.15 分支（`tqtc/harmonyos-5.15.16`）

### 注意事项
- Qt for OpenHarmony 目前仍是 Alpha，生产环境稳定性需持续跟进
- 鸿蒙端的输入法面板 UI 由 ArkTS（IME Kit 强制要求）承接，Qt 主要用于**配置中心**和**跨平台候选窗渲染**
- 候选窗在 Windows/macOS/Linux 上直接用 Qt 渲染；鸿蒙端通过 NAPI 桥接

---

## 决策 2：端侧 ASR → FunASR

### 选型原因
- 阿里达摩院开源（Apache 2.0），可商业使用，无版权风险
- 中文识别效果业界领先，CER（字符错误率）可低至 ~1%（Paraformer 模型）
- 原生支持 ONNX 导出，与决策 3 完美衔接
- 活跃社区，Windows/Linux/macOS 均有完整支持
- 提供完整的流式推理方案（Streaming Paraformer），满足"说完即出字"需求

### 核心模型选择
| 模型 | 参数量 | 场景 | 延迟 |
|------|--------|------|------|
| `paraformer-zh` | ~220M | 标准中文离线识别 | ~200ms |
| `paraformer-zh-streaming` | ~220M | 流式实时转写 | <100ms/chunk |
| `sensevoice-small` | ~234M | 多语言 + 情感检测 | 快速 |

**车机端**优先选 `paraformer-zh-streaming`（流式输出适合驾驶场景边说边显示）  
**桌面端**默认 `paraformer-zh`，可在设置中切换流式模式

### 模型文件格式
```
engine/ai/models/
├── paraformer-zh/
│   ├── model.onnx          # ONNX 格式（由 FunASR 官方导出）
│   ├── tokens.json
│   └── config.yaml
└── paraformer-zh-streaming/
    ├── encoder.onnx
    ├── decoder.onnx
    └── tokens.json
```

---

## 决策 3：推理格式 → ONNX Runtime

### 选型原因
- 微软开源（MIT），商业友好
- **真正跨平台**：Windows / macOS / Linux / Android / iOS / WASM 全覆盖
- 鸿蒙端通过 ONNX Runtime Android 包（arm64-v8a）可直接复用
- FunASR 官方维护 ONNX 导出工具链，无需手动转换
- 硬件加速：Windows 用 DirectML，macOS 用 CoreML，ARM 用 NNAPI，NPU 用对应 EP

### 各平台执行引擎（Execution Provider）
| 平台 | EP | 加速硬件 |
|------|----|---------|
| Windows x64 | DirectML EP | GPU / NPU |
| macOS arm64 | CoreML EP | Apple Neural Engine |
| Linux ARM（飞腾/鲲鹏） | NNAPI EP / CPU EP | ARM NPU |
| Linux LoongArch（龙芯）| CPU EP | CPU |
| HarmonyOS arm64 | NNAPI EP / CPU EP | 华为 NPU / CPU |

### 集成路径
```cpp
// engine/ai/asr_engine.cpp
#include <onnxruntime_cxx_api.h>

class ASREngine {
    Ort::Env env;
    Ort::Session encoder_session;
    Ort::Session decoder_session;
    // ...
};
```

---

## 决策 4：车机端 → HarmonyOS 鸿蒙架构

### 4.1 平台技术背景

HarmonyOS 输入法使用官方 **IME Kit** 框架：
- 核心：`InputMethodExtensionAbility`（ArkTS）
- UI 层：ArkUI（声明式 UI，类 SwiftUI）
- C++ 集成：通过 **NAPI**（Node-API）将 librime 引擎桥接给 ArkTS 层
- 版本要求：HarmonyOS API 12+（NEXT 系列）

### 4.2 鸿蒙车机输入法整体架构

```
┌──────────────────────────────────────────────────────────────┐
│              车机系统（HarmonyOS for Car）                     │
│  ┌─────────────────────────────────────────────────────┐     │
│  │           InputMethodExtensionAbility（ArkTS）        │     │
│  │   onCreate() → KeyboardController → ArkUI 面板        │     │
│  │   on('inputStart') → InputClient.insertText()         │     │
│  └──────────────────┬──────────────────────────────────┘     │
│                     │ NAPI 桥接                               │
│  ┌──────────────────▼──────────────────────────────────┐     │
│  │          librime_napi.so（C++ Native Library）        │     │
│  │   RimeNAPI::processKey() → librime → 候选词           │     │
│  └──────────────────┬──────────────────────────────────┘     │
│                     │                                         │
│  ┌──────────────────▼──────────────────────────────────┐     │
│  │          FunASR ONNX Runtime（语音识别）               │     │
│  │   麦克风 PCM → Paraformer-streaming → 文本 → RIME      │     │
│  └─────────────────────────────────────────────────────┘     │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐     │
│  │   Qt for OpenHarmony（配置中心 UI）                    │     │
│  │   词库管理 / 快捷键 / AI 服务设置                       │     │
│  └─────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────┘
```

### 4.3 车机专项设计原则

> 驾驶安全是车机端的第一约束，所有 UI/UX 决策服从于此。

| 约束项 | 设计方案 |
|--------|---------|
| **驾驶模式** | 行驶中禁用触屏键盘，仅允许语音输入（FunASR 流式） |
| **唤醒方式** | 方向盘语音按键 / 固定热词唤醒（替代 PC 端快捷键） |
| **候选词显示** | 最多展示 3 个候选词，字号 ≥ 28sp，驻车时才展示完整键盘 |
| **输入延迟** | 语音→文字目标 ≤ 500ms（驾驶场景容忍度比 PC 低） |
| **手势操作** | 减少小目标点击，使用滑动/长按替代精确点击 |
| **夜间模式** | 自动跟随车机系统黑暗主题，降低视觉干扰 |

### 4.4 开发工具链

```
开发环境：DevEco Studio 5.x（华为官方 IDE）
SDK：HarmonyOS NEXT API 12+
语言：ArkTS（UI 层）+ C++（librime/ONNX Native）
构建：NAPI 封装 → .so 库 → hap 包
调试：USB 调试 / 车机模拟器
签名：需申请华为开发者账号 + 车机应用签名证书
```

### 4.5 NAPI 桥接设计（关键工程）

```cpp
// platform/vehicle/harmonyos/napi/rime_napi.cpp
#include <napi/native_api.h>
#include "rime_api.h"

// 暴露给 ArkTS 的接口
napi_value RimeProcessKey(napi_env env, napi_callback_info info) {
    // 1. 从 ArkTS 获取 keycode
    // 2. 调用 RimeProcessKey(session_id, keycode, mask)
    // 3. 返回候选词列表给 ArkTS
}

// 注册模块
napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor props[] = {
        {"processKey",    nullptr, RimeProcessKey,    nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getCandidates", nullptr, RimeGetCandidates, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"commitText",    nullptr, RimeCommitText,    nullptr, nullptr, nullptr, napi_default, nullptr},
        {"startASR",      nullptr, RimeStartASR,      nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stopASR",       nullptr, RimeStopASR,       nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(props)/sizeof(props[0]), props);
    return exports;
}
EXTERN_C_START
static napi_module demoModule = {
    .nm_version = 1, .nm_filename = nullptr,
    .nm_register_func = Init, .nm_modname = "synthorbis_rime",
};
extern "C" __attribute__((constructor)) void RegisterModule() {
    napi_module_register(&demoModule);
}
EXTERN_C_END
```

### 4.6 关键风险与应对

| 风险 | 等级 | 应对方案 |
|------|------|---------|
| HarmonyOS NEXT 部分版本不开放 InputMethod Extension | 🔴 高 | 提前向华为申请车机开放能力；在 API 12+ 设备验证 |
| Qt for OpenHarmony 仍是 Alpha，稳定性待验证 | 🟡 中 | 车机配置中心可先用纯 ArkTS 实现，Qt 版本作为后期迁移目标 |
| 车机端签名/资质申请周期长 | 🟡 中 | 第一阶段先用 PC 端推进，车机端并行申请开发者资质 |
| FunASR ONNX 在 HarmonyOS arm64 上性能未验证 | 🟡 中 | 第二阶段在鸿蒙模拟器上跑 benchmark，必要时降级为 CPU EP |

---

## 决策 5：开源策略

### 策略：分层开源 + 商业闭源

```
开源层（MIT / Apache 2.0）         闭源层（商业许可）
─────────────────────────          ──────────────────────────────
librime 核心引擎                    AI 引擎集成层（FunASR 胶水层）
  └── 继承 RIME MIT 协议            云端 AI API 接口模块
跨平台抽象层接口定义（头文件）         图形化配置中心（Qt UI）
                                   SynthOrbis 专有词库和词典
                                   商业输入方案（皮肤/主题）
                                   车机端 NAPI 桥接层
```

### 具体操作
1. **GitHub 仓库结构**：
   - `github.com/SynthOrbis/librime-fork`：公开，MIT，仅含 RIME 核心修改
   - `github.com/SynthOrbis/synthorbis-uni`：私有，商业许可，完整产品代码
   
2. **对外开放**（社区生态建设）：
   - 开放**输入方案 YAML 格式规范**（鼓励社区贡献词库）
   - 开放**插件 SDK API 文档**（允许第三方开发主题/皮肤）
   - 开放**云端 API 接口规范**（允许企业客户对接私有模型）

3. **知识产权保护**：
   - AI 集成算法申请发明专利
   - UI 设计申请外观专利
   - 完整软件产品申请软件著作权登记

---

_文档由阿达西生成，最后更新：2026-04-26_
