# 主机侧硬件配置信息

## 更新时间
2026-05-06

## 硬件配置详情

### CPU (处理器)

| 项目 | 配置参数 |
|------|---------|
| **型号** | Intel Xeon Platinum 8163 |
| **基础频率** | 2.50 GHz |
| **频率范围** | 1.00 - 2.50 GHz |
| **架构** | 2 × Socket |
| **物理核心** | 48 核心（每 Socket 24 核心） |
| **逻辑 CPU** | 96 线程（每核心 2 线程） |
| **L1d 缓存** | 1.5 MiB (48 instances) |
| **L1i 缓存** | 1.5 MiB (48 instances) |
| **L2 缓存** | 48 MiB (48 instances) |
| **L3 缓存** | 66 MiB (2 instances) |

### Memory (内存)

| 项目 | 配置参数 |
|------|---------|
| **总容量** | 93 GB |
| **类型** | DDR4 |
| **可用内存** | ~68 GB |

### Motherboard (主板)

| 项目 | 配置参数 |
|------|---------|
| **制造商** | Inspur (浪潮) |
| **型号** | SA5212M5 |
| **板卡型号** | YZMB-00882-10E |

## 软件环境

| 项目 | 版本/配置 |
|------|----------|
| **操作系统** | Ubuntu 24.04 LTS |
| **内核版本** | Linux 6.8 |
| **编译器** | GCC 13.3.0 |
| **编译选项** | -O2 |
| **C++ 标准** | c++17 |
| **数据精度** | 单精度浮点 (float) |

## 依赖库

| 库名称 | 版本 |
|--------|------|
| FFTW | 3.x |
| LAPACK | system |
| BLAS | system |

## 性能特征

### CPU 性能
- **高性能服务器级处理器**：Intel Xeon Platinum 系列
- **多核并行能力**：96 个逻辑 CPU，适合并行计算
- **大容量缓存**：总计 117 MiB 缓存，减少内存访问延迟

### 内存性能
- **大容量内存**：93 GB，适合大规模数据处理
- **充足余量**：68 GB 可用内存，避免内存瓶颈

### 适用场景
- ✅ 高性能计算（HPC）
- ✅ 大规模数据处理
- ✅ 并行算法验证
- ✅ 科学计算仿真

## 与 FPGA 对比

| 指标 | CPU (本平台) | FPGA (xcku5p) |
|------|-------------|---------------|
| **处理器核心** | 96 线程 | 1 (专用流水线) |
| **时钟频率** | 2.50 GHz | 250 MHz |
| **内存** | 93 GB DDR4 | DDR4 (外部) |
| **功耗** | ~80 W | ~4 W |
| **并行方式** | 多线程 | 流水线并行 |
| **适用场景** | 通用计算 | 专用加速 |

## 数据采集命令

### CPU 信息
```bash
# CPU 型号和频率
lscpu | grep -E "Model name|MHz"

# CPU 缓存
lscpu | grep -E "L1d cache|L1i cache|L2 cache|L3 cache"

# CPU 架构
lscpu | grep -E "CPU\(s\)|Thread|Core|Socket"
```

### 内存信息
```bash
# 内存总量
free -h | grep Mem

# 详细内存信息
cat /proc/meminfo | grep MemTotal
```

### 主板信息
```bash
# 主板制造商
cat /sys/class/dmi/id/board_vendor

# 主板型号
cat /sys/class/dmi/id/board_name

# 系统信息
hostnamectl | grep -E "Hardware|Machine"
```

## 数据文件位置

- **详细配置**: `data/pc_cpp/platform_info.json`
- **性能数据**: `data/pc_cpp/performance_data.json`
- **论文引用**: `doc/论文/第五章.md` (表 5-1-1)

## 更新记录

- **2026-05-06**: 初始版本，采集完整硬件配置信息

## 参考资料

- [Intel Xeon Platinum 8163 Specifications](https://ark.intel.com/)
- [Inspur SA5212M5 Server](https://www.inspur.com/)
- [Ubuntu 24.04 LTS Documentation](https://ubuntu.com/)
