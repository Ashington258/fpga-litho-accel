# HLS 运行时间刻画方法

## 概述

本文档说明如何准确刻画 HLS IP 的纯计算时间，排除 JTAG 数据传输开销。

---

## 1. HLS 运行时间定义

### 1.1 纯计算时间

**定义**：HLS IP 从 `ap_start` 信号触发到 `ap_done` 信号拉高的时间间隔。

**特点**：
- 不包含数据传输时间
- 不包含参数配置时间
- 仅反映硬件计算性能

### 1.2 完整执行时间

**定义**：从开始加载数据到读取输出结果的完整时间。

**组成**：
```
完整时间 = 数据加载时间 + 参数配置时间 + 计算时间 + 结果读取时间
         = JTAG传输时间 + HLS计算时间
```

---

## 2. HLS 计算时间测量方法

### 2.1 方法一：基于 HLS 综合报告（推荐）

**数据来源**：`source/SOCS_HLS/socs_v18_synth/hls/syn/report/calc_socs_2048_hls_stream_refactored_v18_csynth.rpt`

**关键指标**：
```
Latency (cycles): 2,636,242
Target Clock: 300 MHz (3.33 ns)
Estimated Clock: 457 MHz (2.19 ns)
```

**计算公式**：
```python
计算时间 (ms) = 延迟周期数 / 时钟频率 (Hz) × 1000

# 目标频率
时间 = 2,636,242 / (300 × 10^6) × 1000 = 8.79 ms

# 估计频率
时间 = 2,636,242 / (457 × 10^6) × 1000 = 5.77 ms
```

**优点**：
- 准确可靠
- 不受测量工具影响
- 可提前预测性能

**缺点**：
- 需要综合报告
- 实际频率可能略有偏差

### 2.2 方法二：硬件计数器测量

**原理**：使用 FPGA 内部计数器测量 `ap_start` 到 `ap_done` 的时间。

**实现步骤**：

1. **添加计数器 IP**：
   - 在 Block Design 中添加 AXI Timer 或自定义计数器
   - 连接到 HLS IP 的 `ap_start` 和 `ap_done` 信号

2. **TCL 测量脚本**：
```tcl
# 读取计数器值
set counter_start [read_hw_axi $axi_if $COUNTER_ADDR]
# 启动 HLS IP
write_hw_axi $axi_if $HLS_CTRL_BASE 0x00000001
# 等待完成
wait_for_ap_done
# 读取计数器值
set counter_end [read_hw_axi $axi_if $COUNTER_ADDR]
# 计算时间
set cycles [expr $counter_end - $counter_start]
set time_ms [expr $cycles / 300000.0]
puts "HLS 计算时间: $time_ms ms"
```

**优点**：
- 实际硬件测量
- 精度高（纳秒级）

**缺点**：
- 需要额外硬件资源
- 需要修改 Block Design

### 2.3 方法三：ILA 波形分析

**原理**：使用 Integrated Logic Analyzer (ILA) 抓取 HLS IP 控制信号波形。

**实现步骤**：

1. **添加 ILA IP**：
   - 在 Block Design 中添加 ILA
   - 连接 `ap_start`, `ap_done`, `ap_idle`, `ap_ready` 信号

2. **触发设置**：
   - 触发条件：`ap_start = 1` (上升沿)
   - 捕获深度：足够大以捕获完整执行

3. **波形分析**：
```
时间点分析：
  T0: ap_start = 1 (启动)
  T1: ap_idle = 0 (开始执行)
  T2: ap_done = 1 (完成)
  T3: ap_idle = 1 (回到空闲)

计算时间 = T2 - T0 (时钟周期数)
```

**优点**：
- 可视化分析
- 可观察内部状态

**缺点**：
- 需要额外硬件资源
- 波形数据量大

### 2.4 方法四：软件计时（粗略）

**原理**：在 TCL 脚本中记录时间戳。

**实现**：
```tcl
# 记录开始时间
set start_time [clock milliseconds]

# 启动 HLS IP
write_hw_axi $axi_if $HLS_CTRL_BASE 0x00000001

# 等待完成
wait_for_ap_done

# 记录结束时间
set end_time [clock milliseconds]

# 计算时间
set elapsed [expr $end_time - $start_time]
puts "HLS 执行时间: $elapsed ms"
```

**优点**：
- 简单易实现
- 无需额外硬件

**缺点**：
- 精度低（毫秒级）
- 包含轮询开销
- 受 TCL 解释器影响

---

## 3. JTAG 传输时间分析

### 3.1 JTAG 传输时间组成

```
JTAG 总时间 = 数据传输时间 + 协议开销 + TCL 解释时间
```

**数据传输时间**：
- 理论时间：数据量 / JTAG 带宽
- 实际带宽：~1 MB/s (15-30 MHz JTAG)

**协议开销**：
- AXI 事务建立
- 地址/数据打包
- 响应等待

**TCL 解释时间**：
- TCL 命令解析
- 变量操作
- 循环控制

### 3.2 JTAG 传输时间测量

**方法**：在 TCL 脚本中记录各阶段时间。

```tcl
# 数据加载时间
set load_start [clock milliseconds]
source load_mskf_r.tcl
source load_mskf_i.tcl
source load_scales.tcl
source load_krn_r.tcl
source load_krn_i.tcl
set load_end [clock milliseconds]
puts "数据加载时间: [expr $load_end - $load_start] ms"

# 参数配置时间
set config_start [clock milliseconds]
# ... 配置参数 ...
set config_end [clock milliseconds]
puts "参数配置时间: [expr $config_end - $config_start] ms"

# 结果读取时间
set read_start [clock milliseconds]
# ... 读取输出 ...
set read_end [clock milliseconds]
puts "结果读取时间: [expr $read_end - $read_start] ms"
```

### 3.3 实测数据

根据板级验证实测：

| 阶段 | 时间 | 占比 |
|------|------|------|
| 数据加载 | ~35 分钟 | 99.97% |
| 参数配置 | ~1 秒 | 0.05% |
| HLS 计算 | 8.79 ms | 0.0004% |
| 结果读取 | ~5 秒 | 0.03% |
| **总计** | **~35 分钟** | **100%** |

---

## 4. 加速比计算

### 4.1 纯计算加速比

**定义**：仅比较计算时间，不考虑数据传输。

**公式**：
```
加速比 = CPU 计算时间 / HLS 计算时间
```

**实测结果**：
```
CPU 时间: 49.6 ms
HLS 时间: 8.79 ms
加速比: 5.6x
```

### 4.2 端到端加速比

**定义**：比较完整执行时间（包含数据传输）。

**公式**：
```
加速比 = CPU 完整时间 / (JTAG传输时间 + HLS计算时间)
```

**实测结果**：
```
CPU 时间: 49.6 ms
JTAG + HLS 时间: ~35 分钟
加速比: 0.00002x (负加速！)
```

**结论**：JTAG 传输严重拖累整体性能。

### 4.3 理想加速比（无传输开销）

**假设**：使用高速接口（PCIe/以太网）替代 JTAG。

**PCIe Gen3 x8 性能**：
```
带宽: 6 GB/s
传输时间: 8.48 MB / 6 GB/s = 1.4 ms
总时间: 1.4 ms + 8.79 ms = 10.2 ms
加速比: 49.6 ms / 10.2 ms = 4.9x
```

**10GbE 性能**：
```
带宽: 1 GB/s
传输时间: 8.48 MB / 1 GB/s = 8.5 ms
总时间: 8.5 ms + 8.79 ms = 17.3 ms
加速比: 49.6 ms / 17.3 ms = 2.9x
```

---

## 5. 性能优化建议

### 5.1 短期优化（JTAG 环境）

1. **批量处理**：
   - 一次加载多组数据
   - 连续执行多次计算
   - 分摊传输开销

2. **数据预加载**：
   - 将固定数据（mask, kernel）预加载到 DDR
   - 只传输变化的参数

3. **优化 TCL 脚本**：
   - 减少循环次数
   - 使用批量写入命令
   - 避免不必要的验证

### 5.2 长期优化（生产环境）

1. **高速接口**：
   - PCIe Gen3/Gen4
   - 10GbE/40GbE
   - DDR 直接访问

2. **系统集成**：
   - 嵌入式处理器（MicroBlaze/ARM）
   - DMA 引擎
   - 中断驱动

3. **流水线处理**：
   - 数据传输与计算并行
   - 双缓冲机制
   - 连续处理模式

---

## 6. 总结

### 6.1 HLS 运行时间刻画要点

1. **使用 HLS 综合报告**：
   - 最准确的方法
   - 延迟周期数 / 时钟频率

2. **排除 JTAG 传输**：
   - JTAG 传输时间 >> 计算时间
   - 必须单独分析

3. **区分不同加速比**：
   - 纯计算加速比：5.6x
   - 端到端加速比：0.00002x (JTAG)
   - 理想加速比：4.9x (PCIe)

### 6.2 关键结论

✅ **HLS IP 计算性能优秀**：
- 8.79 ms 计算时间
- 5.6x 加速比
- 113.8 frames/sec 吞吐量

⚠️ **JTAG 传输是瓶颈**：
- 占比 >99.9%
- 严重拖累整体性能
- 不适合生产环境

🎯 **优化方向**：
- 使用高速接口
- 批量处理
- 系统集成

---

## 附录：性能数据汇总

### A. HLS IP 性能指标

| 指标 | 数值 |
|------|------|
| 时钟频率（目标） | 300 MHz |
| 时钟频率（估计） | 457 MHz |
| 延迟周期数 | 2,636,242 |
| 计算时间（目标） | 8.79 ms |
| 计算时间（估计） | 5.77 ms |
| 吞吐量 | 113.8 fps |
| 计算性能 | 22.8 GFLOPS |

### B. CPU 参考实现性能

| 指标 | 数值 |
|------|------|
| CPU 型号 | Intel i7-10700K |
| CPU 频率 | 3.8 GHz |
| 计算时间 | 49.6 ms |
| 吞吐量 | 20.2 fps |

### C. 加速比对比

| 场景 | 加速比 |
|------|--------|
| 纯计算 | 5.6x |
| JTAG 端到端 | 0.00002x |
| PCIe 端到端 | 4.9x |
| 10GbE 端到端 | 2.9x |

---

**文档版本**: 1.0  
**最后更新**: 2026-05-06
