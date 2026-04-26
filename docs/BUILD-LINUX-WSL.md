# Librime Linux 构建指南（WSL / Linux）

本文档记录 librime 在 Linux 环境下的完整构建流程（适用于 WSL Ubuntu 24.04）。

## 前置条件

### Windows 用户（WSL）

```powershell
# 1. 安装 Ubuntu WSL
wsl --install -d Ubuntu --no-launch

# 2. 启动 Ubuntu
wsl -d Ubuntu
```

### Linux 依赖安装

```bash
# 在 WSL Ubuntu 中执行（只需一次）
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake ninja-build pkg-config git curl wget ca-certificates \
  libboost-dev libboost-filesystem-dev libboost-system-dev libboost-locale-dev \
  libboost-regex-dev \
  libopencc-dev libmarisa-dev \
  libgtest-dev libgoogle-glog-dev \
  libyaml-cpp-dev libxkbfile-dev \
  libibus-1.0-dev
```

## 构建步骤

### 1. 初始化 librime 子模块

```bash
cd /path/to/SynthOrbisUNI/engine/librime
git submodule update --init --recursive
```

### 2. 构建依赖库（glog / leveldb / marisa / opencc / yaml-cpp）

```bash
cd engine/librime

# 方法一：使用 Makefile（推荐）
make -f deps.mk prefix=/usr/local all

# 方法二：手动 cmake
# (已在上一步自动完成)
```

**注意**：依赖库安装到 `/usr/local/`，需要 sudo 权限。

### 3. 配置 CMake

```bash
cd engine/librime
mkdir -p build
cd build

# 静态库方式（推荐，避免 -fPIC 问题）
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TEST=OFF \
  -DENABLE_LOGGING=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_STATIC=ON
```

### 4. 编译

```bash
# 查看可用目标
make help | grep -E 'rime|all'

# 编译静态库
make rime-static

# 或编译共享库（需先修复 leveldb -fPIC 问题）
# make rime-shared
```

### 5. 验证构建产物

```bash
ls -lh build/lib/librime.a
# 输出应为类似: -rwxr-xr-x 1 root root 11M  librime.a

# 查看包含的目标文件数量
ar -t build/lib/librime.a | wc -l
```

## 已知问题与解决方案

### 1. libleveldb.a 链接错误（-fPIC 问题）

**症状**：
```
/usr/bin/ld: /usr/local/lib/libleveldb.a(db_impl.cc.o):
  relocation R_X86_64_PC32 against symbol ... can not be used
  when making a shared object; recompile with -fPIC
```

**原因**：librime 动态库链接 leveldb 静态库时，leveldb 未编译为 position-independent code。

**解法**：使用静态库方式（`-DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC=ON`）。

### 2. Docker Hub 网络不通

**解法**：配置 Docker 镜像加速器（如阿里云），
或使用 WSL 编译（不依赖 Docker）。

### 3. CMake 找不到 boost_regex

**解法**：
```bash
sudo apt-get install libboost-regex-dev
```

## Windows 原生编译（VS2022）

```batch
# 在 Developer Command Prompt 中执行
cd engine\librime

# 1. 复制并编辑环境配置
copy env.bat.template env.bat
# 编辑 env.bat，设置 BOOST_ROOT 路径

# 2. 安装 Boost（下载到默认目录）
install-boost.bat

# 3. 编译依赖
build.bat deps

# 4. 编译 librime
build.bat librime
```

**前置条件**：
- Visual Studio 2022（含 C++ 桌面开发 workload）
- CMake >= 3.10
- Boost >= 1.83

## 构建产物说明

| 产物 | 路径 | 说明 |
|------|------|------|
| librime.a | build/lib/ | 静态库（~11MB） |
| librime.so | build/lib/ | 动态库（需额外配置） |
| 数据文件 | build/bin/ | YAML 词典配置 |
| CMake 配置 | build/rime.pc | pkg-config 文件 |

## 验证安装

```bash
# 链接到你的项目
target_link_libraries(your_target PRIVATE /path/to/librime.a)

# 包含头文件
#include <rime.h>
```
