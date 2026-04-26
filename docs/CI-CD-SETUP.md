# SynthOrbis UNI CI/CD 流水线配置指南

> 本文档说明 SynthOrbis UNI 项目的 CI/CD 流水线配置和使用方法

---

## 目录

1. [概述](#概述)
2. [GitHub Actions 配置](#github-actions-配置)
3. [Gitee Go 配置](#gitee-go-配置)
4. [触发条件](#触发条件)
5. [构建产物](#构建产物)
6. [本地测试](#本地测试)

---

## 概述

### 支持的平台

| 平台 | 架构 | 工具链 | 状态 |
|------|------|--------|------|
| Linux (Ubuntu 24.04) | x86_64 | GCC/Clang | ✅ |
| Linux (信创) | ARM64, LoongArch64 | 交叉编译 | ✅ |
| Windows | x64, ARM64 | MSVC 2022 | ✅ |
| macOS | Universal (Intel + Apple Silicon) | Xcode/Clang | ✅ |

### 流水线架构

```
┌─────────────────────────────────────────────────────────┐
│                    触发源                                │
│   Push / PR / Tag / Manual                              │
└─────────────────┬───────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────────────┐
│                   代码质量检查                           │
│   clang-format / cmake-format / lint                   │
└─────────────────┬───────────────────────────────────────┘
                  │
        ┌─────────┴─────────┬───────────────┐
        ▼                     ▼               ▼
┌───────────────┐   ┌───────────────┐  ┌───────────────┐
│   Linux       │   │   Windows     │  │   macOS       │
│   x64/ARM64   │   │   x64/ARM64   │  │   Universal   │
└───────┬───────┘   └───────┬───────┘  └───────┬───────┘
        │                   │                   │
        └───────────────────┴───────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   产物打包 & 上传      │
              │   GitHub Release      │
              └───────────────────────┘
```

---

## GitHub Actions 配置

### 文件结构

```
.github/workflows/
├── ci.yml          # 主流水线 (PR/Push 触发)
├── windows.yml     # Windows 专用构建
├── linux.yml       # Linux 多架构构建
├── macos.yml       # macOS 多架构构建
├── xinxin.yml      # 信创平台专项构建
└── release.yml     # Release 发布流水线
```

### 核心工作流

#### 1. 主 CI (`ci.yml`)

```yaml
on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
```

**包含任务:**
- 代码质量检查 (lint)
- Linux x64 构建
- Windows x64 构建
- macOS Universal 构建
- 构建汇总报告

#### 2. 信创平台构建 (`xinxin.yml`)

手动触发，支持平台:
- 统信 UOS V20
- 麒麟 V10
- 龙芯 LoongArch64
- 飞腾/鲲鹏 ARM64
- 海光/兆芯 x86

```yaml
on:
  workflow_dispatch:
    inputs:
      platform:
        type: choice
        options:
          - uos-20
          - kylin-v10
          - loongarch
          - phytium
          - all
```

#### 3. Release 发布 (`release.yml`)

```yaml
on:
  workflow_dispatch:
    inputs:
      version:
        required: true
      prerelease:
        default: false
```

**发布流程:**
1. 触发全平台构建
2. 打包所有构建产物
3. 创建 GitHub Draft Release
4. 上传产物到 Release

---

## Gitee Go 配置

### 文件结构

```
.gitee/workflows/
└── ci.yml    # Gitee Go 流水线配置
```

### Gitee Go 特点

| 特性 | 说明 |
|------|------|
| 语法 | 与 GitHub Actions 类似但不完全兼容 |
| Runner | 支持自定义构建机器 |
| 缓存 | 支持 artifacts 持久化 |
| 触发器 | Push/PR/Tag/手动 |

### Gitee Actions 使用

1. 在 Gitee 仓库启用 "Gitee Go" 功能
2. 将 `.gitee/workflows/ci.yml` 推送到仓库
3. 在 Gitee 流水线页面查看执行状态

---

## 触发条件

### 自动触发

| 事件 | 分支 | 行为 |
|------|------|------|
| Push | main, develop | 运行全平台 CI |
| Push | 其他 | 运行 lint + Linux 构建 |
| PR | main | 运行全平台 CI |
| Tag (v*) | - | 创建 Release |

### 手动触发

| 工作流 | 参数 | 说明 |
|--------|------|------|
| ci.yml | - | 常规 CI |
| xinxin.yml | platform | 信创平台构建 |
| release.yml | version, prerelease | 发布新版本 |

---

## 构建产物

### 产物命名规则

```
librime-{platform}-{variant}-{version}.{ext}
```

### 产物位置

| 平台 | 路径 | 文件 |
|------|------|------|
| Linux x64 | `build/linux-x64/lib/` | librime.a, librime.so* |
| Windows x64 | `build/windows-x64/lib/Release/` | librime.lib, rime.dll |
| macOS | `build/macos-universal/lib/` | librime.a, librime.dylib |
| 信创平台 | `dist/{platform}/` | *.a, *.so |

### GitHub Actions Artifacts

产物通过 `actions/upload-artifact@v4` 上传，保留 7-30 天。

---

## 本地测试

### 使用 CI 脚本

```bash
# Linux / macOS
bash scripts/ci/build.sh linux
bash scripts/ci/build.sh macos
bash scripts/ci/build.sh all

# Windows (PowerShell)
.\scripts\build-local.ps1 -Platform linux -BuildType release
```

### 使用 CMake

```bash
# 配置
mkdir build && cd build
cmake ../engine/librime -G Ninja -DCMAKE_BUILD_TYPE=Release

# 构建
ninja

# 测试
ctest --output-on-failure
```

---

## 故障排查

### 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| CMake 找不到依赖 | 子模块未初始化 | `git submodule update --init --recursive` |
| MSVC 构建失败 | 工具链未安装 | 安装 Visual Studio 2022 |
| ARM64 交叉编译失败 | 工具链路径错误 | 检查 `CMAKE_SYSROOT` |
| 测试失败 | 缺少 gtest | `apt-get install libgtest-dev` |

### 调试技巧

1. **启用详细输出:**
   ```yaml
   env:
     CMAKE_VERBOSE_MAKEFILE: ON
   ```

2. **缓存依赖:**
   GitHub Actions 已配置 ccache 和 CMake 缓存

3. **本地模拟 CI:**
   ```bash
   # 使用 Docker 模拟 Ubuntu 环境
   docker run -it ubuntu:24.04
   apt-get update && apt-get install -y cmake ninja-build build-essential
   ```

---

## 下一步

- [ ] 配置私有构建 runner
- [ ] 集成代码覆盖率
- [ ] 添加性能基准测试
- [ ] 配置安全扫描 (CodeQL)
