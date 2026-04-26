# SynthOrbis UNI — 第一阶段开发启动清单

> 目标：核心基座与架构搭建（预计 2-3 个月）  
> 当前状态：✅ 工作区初始化完成，准备启动

---

## 一、环境准备

### 1.1 开发环境（Windows 主机）
- [ ] 安装 Visual Studio 2022（含 C++ 工作负载）
- [ ] 安装 CMake ≥ 3.27
- [ ] 安装 Git + Git LFS
- [ ] 安装 Python 3.11+（用于构建脚本和 AI 工具链）
- [ ] 安装 LLVM/Clang（用于跨平台编译测试）

### 1.2 虚拟机环境（信创适配）
- [ ] 下载统信 UOS V20 ISO，搭建 VMware/VirtualBox VM
- [ ] 下载麒麟 V10 ISO，搭建 VM
- [ ] 在 VM 中安装 IBus 和 Fcitx 开发包

### 1.3 RIME 源码准备
- [ ] `git submodule add https://github.com/rime/librime engine/librime`
- [ ] 验证 librime 能在 Windows 上本地编译（参考 Weasel 构建文档）

---

## 二、RIME 核心引擎分支建立

### 2.1 创建专属分支
```bash
cd engine/librime
git checkout -b synthorbis-uni-main
```

### 2.2 librime 精简评估
需要审查并标记以下模块是否保留：
- `src/rime/algo/` — 算法层（**保留**）
- `src/rime/dict/` — 词典层（**保留**）
- `src/rime/gear/` — 处理器/过滤器（**保留核心，裁剪非必要**）
- `src/rime/lever/` — 配置管理（**保留**）
- 插件系统 — 评估是否保留（用于车机/信创扩展）

### 2.3 CI/CD 搭建（GitHub Actions）
```yaml
# .github/workflows/build.yml 框架
on: [push, pull_request]
jobs:
  build-windows:    # MSVC + x64
  build-macos:      # Xcode + arm64/x86_64
  build-linux:      # GCC + IBus/Fcitx
  build-arm:        # 交叉编译 ARM（飞腾/鲲鹏）
  build-loongarch:  # 交叉编译 LoongArch（龙芯）
```

---

## 三、跨平台抽象层设计

### 3.1 输入事件抽象（优先级：高）
文件路径：`engine/ai/input_event.h`

```cpp
// 统一输入事件接口草案
class IInputEventAdapter {
public:
    virtual void onKeyDown(KeyCode key, Modifiers mod) = 0;
    virtual void onKeyUp(KeyCode key, Modifiers mod) = 0;
    virtual void onCommitText(const std::string& text) = 0;
    virtual void onUpdateCandidates(const CandidateList& candidates) = 0;
};
```
平台实现：
- Windows: `platform/windows/WeaselInputAdapter.cpp`
- macOS: `platform/macos/SquirrelInputAdapter.mm`
- Linux: `platform/linux-xinxin/IBusRimeAdapter.cpp`

### 3.2 渲染引擎抽象（基于 Skia）
- [ ] 集成 Skia 图形库（或 Qt）
- [ ] 候选窗统一渲染接口
- [ ] 主题系统（颜色/字体/圆角/透明度）

### 3.3 音频处理抽象
```
audio/
├── IAudioCapture.h          # 统一接口
├── WASAPICapture.cpp        # Windows
├── CoreAudioCapture.mm      # macOS
└── PulseAudioCapture.cpp    # Linux
```
目标 API 规格：
- 采样率：16000 Hz（ASR 标准）
- 位深：16-bit PCM
- 声道：单声道

---

## 四、信创适配验证

### 4.1 统信 UOS（优先）
```bash
# 在 UOS VM 中执行
sudo apt install libibus-1.0-dev librime-dev
# 验证 ibus-rime 能正常编译运行
```

### 4.2 国产 CPU 编译验证
| CPU 架构 | 交叉编译工具链 | 验证重点 |
|---------|-------------|--------|
| ARM (飞腾/鲲鹏) | aarch64-linux-gnu-gcc | NPU 推理加速接口 |
| LoongArch (龙芯) | loongarch64-linux-gnu-gcc | 指令集兼容性 |
| x86 (海光/兆芯) | 原生 gcc | AVX2/SSE4.2 指令优化 |

---

## 五、车机端扩展预研

> 文档中提及车机平台，需在第一阶段完成以下预研：

- [ ] 确认车机操作系统类型（Android Auto / QNX / AGL / 定制 Linux）
- [ ] 评估 RIME 在车机环境的裁剪方案（无 IBus/Fcitx 依赖版本）
- [ ] 语音输入在车机场景的唤醒词方案（取代快捷键）
- [ ] 安全驾驶模式下的交互约束（精简候选词 / 语音优先）

---

## 六、关键技术决策（✅ 全部已确认 2026-04-26）

| # | 决策项 | 选型结果 |
|---|--------|---------|
| 1 | UI 框架 | **Qt**（桌面用 Qt6，鸿蒙用 Qt for OpenHarmony 5.15） |
| 2 | 端侧 ASR | **FunASR**（Paraformer-zh / Paraformer-zh-streaming） |
| 3 | 推理格式 | **ONNX Runtime**（各平台对应 EP 加速） |
| 4 | 车机端 | **HarmonyOS** — IME Kit + ArkTS UI + NAPI（C++ 桥接） |
| 5 | 开源策略 | **librime 层 MIT 开源 + AI/配置中心/云端接口层商业闭源** |

> 详细决策说明见 `docs/TECH_DECISIONS.md`

---

## 七、车机端（HarmonyOS）专项启动任务

- [ ] 注册华为开发者账号，申请车机应用开发资质
- [ ] 安装 DevEco Studio 5.x，配置 HarmonyOS API 12 SDK
- [ ] 研究 IME Kit 文档，搭建最小 `InputMethodExtensionAbility` Demo
- [ ] 设计 NAPI 桥接接口（ArkTS ↔ librime C++）
- [ ] 验证 ONNX Runtime（arm64-v8a）在鸿蒙模拟器上的运行
- [ ] 车机安全驾驶模式 UX 设计（行驶中纯语音、停车显示键盘）

---

## 七、本阶段交付物

- [ ] 能在 Windows/macOS/统信 UOS 上编译运行的 RIME 基础客户端
- [ ] 跨平台抽象层接口定义（头文件）
- [ ] CI/CD 流水线（至少覆盖 Windows + Linux 两平台）
- [ ] 信创 VM 环境验证报告
- [ ] ARM/LoongArch 交叉编译可行性报告
- [ ] 车机端技术选型建议书

---

_最后更新：2026-04-26_
