# SynthOrbis UNI — 全平台（信创）PC+车机 AI 输入法

> 国产化 · 全平台 · 智能化  
> 连接用户与数字世界的下一代智能输入平台

---

## 项目定位

SynthOrbis UNI 是合弈寰宇旗下面向未来的智能输入平台，以"国产化、全平台、智能化"为核心特征。  
作为公司"AI眼镜+输入法"生态在桌面端的关键落子，覆盖 Windows、macOS、信创 Linux（统信 UOS / 麒麟）及车机平台。

---

## 核心技术架构

```
┌─────────────────────────────────────────────────────┐
│                  应用层 / 配置中心 UI                │
│         (ArkTS / Qt 跨平台图形化设置界面)            │
├───────────┬─────────────┬───────────┬───────────────┤
│  Windows  │    macOS    │ 信创 Linux │    车机端      │
│  小狼毫   │   鼠须管    │  中州韵   │  (车机适配层) │
│ (Weasel)  │  (Squirrel) │(IBus/Fcitx│               │
├───────────┴─────────────┴───────────┴───────────────┤
│              跨平台抽象层 (Skia UI + 音频抽象)        │
│   输入事件抽象 │ 渲染引擎抽象 │ 音频处理抽象          │
├─────────────────────────────────────────────────────┤
│              AI 智能引擎（端云协同）                  │
│  端侧: FunASR / GLM-ASR-Nano (ONNX / MindSpore Lite)│
│  云端: 智谱 GLM-ASR / 火山引擎豆包                   │
├─────────────────────────────────────────────────────┤
│               RIME 核心引擎 (librime)                │
│          C++ · MIT 开源 · 自主可控                    │
└─────────────────────────────────────────────────────┘
```

---

## 平台覆盖

| 平台 | 客户端 | 技术要点 |
|------|--------|---------|
| Windows 10/11 | 小狼毫 (Weasel) | Windows API + WASAPI/DirectSound |
| macOS 12+ | 鼠须管 (Squirrel) | SwiftUI + Core Audio |
| 统信 UOS / 麒麟 | 中州韵 (ibus-rime/fcitx-rime) | IBus/Fcitx + ALSA/PulseAudio |
| 车机（鸿蒙） | UNI Vehicle | HarmonyOS IME Kit + ArkTS + NAPI（C++桥接）|
| 国产 CPU | 全平台 | 龙芯(LoongArch)、飞腾/鲲鹏(ARM)、海光/兆芯(x86) |

## 技术选型（已确认）

| 层级 | 选型 |
|------|------|
| 配置中心 UI | **Qt 6**（桌面）/ **Qt for OpenHarmony 5.15**（车机）|
| 端侧 ASR | **FunASR** Paraformer-zh / Paraformer-zh-streaming |
| 模型推理 | **ONNX Runtime**（各平台 EP 硬件加速）|
| 车机框架 | **HarmonyOS IME Kit** + ArkTS + NAPI |
| 开源策略 | librime 层 MIT 开源；AI/配置/云端层商业闭源 |

---

## 目录结构

```
SynthOrbisUNI/
├── docs/                   # 原始文档（技术开发文档、实施蓝图）
├── engine/
│   ├── librime/            # RIME 核心引擎（git submodule）
│   ├── synthorbis-engine/  # RIME 集成层（新建！）
│   │   ├── include/        # C/C++ 统一 API
│   │   ├── src/            # 平台实现
│   │   └── test/           # 单元测试
│   └── ai/                 # AI 引擎（端侧模型集成）
├── platform/               # 跨平台抽象层核心（新建！）
│   ├── include/            # 统一头文件（9个核心接口）
│   │   └── platform/
│   │       ├── platform.h   # 平台检测 + 核心API
│   │       ├── types.h      # 统一类型定义
│   │       ├── macros.h     # 编译器/平台宏
│   │       ├── compiler.h   # 编译器特性
│   │       ├── panic.h      # 跨平台异常处理
│   │       ├── audio.h      # 统一音频子系统
│   │       ├── context.h    # 输入法上下文接口
│   │       ├── config.h     # 配置存储抽象
│   │       └── thread.h      # 线程/并发抽象
│   ├── src/               # 平台实现（Linux/Win/macOS存根）
│   │   ├── linux/         # ALSA + XDG 配置实现
│   │   ├── windows/       # WASAPI + 注册表配置实现
│   │   └── macos/         # CoreAudio 实现（待建）
│   ├── windows/            # Windows 前端 (Weasel)
│   ├── macos/              # macOS 前端 (Squirrel)
│   ├── linux-xinxin/       # 信创 Linux 适配 (ibus-rime/fcitx-rime)
│   └── vehicle/            # 车机端适配
├── ui/                     # 跨平台统一 UI 组件 (Skia/Qt)
├── cloud-api/              # 云端 AI 服务接口
├── config-center/          # 统一配置中心
├── scripts/                # 构建脚本
│   └── build-local.ps1     # Windows 本地编译脚本
└── tests/                  # 测试套件
```

### 跨平台抽象层设计

```
┌──────────────────────────────────────────────────────────┐
│            Engine / AI 层（platform/ 接口隔离）            │
├──────────┬──────────┬──────────┬──────────┬─────────────┤
│ audio.h │ context.h│ config.h │ thread.h │  panic.h    │
├──────────┴──────────┴──────────┴──────────┴─────────────┤
│               platform.h（统一入口）                       │
├──────────┬──────────┬────────────────────────────────────┤
│ Linux    │ Windows  │  macOS / HarmonyOS                 │
│ (存根)   │ (存根)   │  (存根，待完整实现)                 │
│ ALSA     │ WASAPI   │  CoreAudio / OHOS Audio           │
│ XDG      │ Registry │  UserDefaults / Preferences      │
└──────────┴──────────┴────────────────────────────────────┘
```

**核心接口文件：**
- `platform.h` — 平台检测宏、版本信息、路径函数
- `audio.h` — 统一音频抽象（WASAPI / CoreAudio / ALSA）
- `context.h` — 输入法上下文 + RIME 引擎统一接口
- `config.h` — YAML/JSON 配置读写 + 预定义配置键
- `thread.h` — 跨平台线程、互斥锁、原子操作、线程池
- `types.h` — 固定宽度类型、状态码、字符串视图
```

---

## 开发路线图

### 第一阶段：核心基座与架构搭建（2-3个月）
- [ ] Fork RIME 官方仓库，建立 SynthOrbis UNI 专属分支
- [ ] librime 精简审查，移除无关模块
- [ ] 搭建 CI/CD（自动化编译 + 测试）
- [ ] 构建跨平台抽象层（输入事件 / 渲染 / 音频）
- [ ] 信创环境基础适配（统信 UOS + 麒麟 VM 验证）
- [ ] ARM + LoongArch 架构编译可行性验证

### 第二阶段：AI 引擎集成与功能开发（3-4个月）
- [ ] 集成端侧 ASR 模型（FunASR 或 GLM-ASR-Nano → ONNX/MindSpore Lite）
- [ ] 构建云端 AI RESTful 接口（智谱 GLM / 豆包）
- [ ] 端云协同调度逻辑
- [ ] 快捷键唤醒 + 流式转写 + 自动上屏
- [ ] AI 文本改写（翻译/润色/扩写/总结）
- [ ] 垂直场景词库动态加载

### 第三阶段：产品化、测试与发布（2-3个月）
- [ ] 图形化配置中心（ArkTS / Qt）
- [ ] 全平台兼容性测试（Win10/11 / macOS12+ / UOS V20 / 麒麟 V10）
- [ ] 性能优化（内存 / CPU / 输入延迟）
- [ ] 安全审计（代码审计 + 渗透测试）
- [ ] 安装包打包（.exe / .dmg / .deb / .rpm）

---

## 商业模式

| 版本 | 定价 | 功能 |
|------|------|------|
| 基础版 | 免费 | 核心输入 + 端侧 AI |
| 专业版 | 订阅制 | 云端 AI + 专属词库 + 优先支持 |
| 企业版 | 定制 | 私有化部署 + 数据合规 + 专属技术支持 |

---

## 相关资源

- 官方域名：uni.synthorbis.com
- 技术文档：`docs/AI空间输入法技术开发文档.docx`
- 实施蓝图：`docs/技术实施蓝图.docx`
- **Linux 构建指南**：`docs/BUILD-LINUX-WSL.md` ✅

## 构建状态

| 平台 | 架构 | 静态库 | 动态库 | 状态 |
|------|------|--------|--------|------|
| Linux (WSL Ubuntu 24.04) | x86_64 | ✅ librime.a (11MB) | ⚠️ 需-fPIC | ✅ 验证通过 |
| Windows (VS2022) | x64 | 待测试 | 待测试 | 🔧 待安装 VS2022 |
| macOS | arm64/x64 | 待测试 | 待测试 | 🔧 待实现 |
| HarmonyOS | ARM64 | 待测试 | 待测试 | 🔧 预研阶段 |
