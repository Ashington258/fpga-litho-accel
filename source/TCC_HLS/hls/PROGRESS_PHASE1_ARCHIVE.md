# Phase 1 TCC HLS 重构 - 工作存档

**存档时间**: 2026-04-06
**状态**: 暂停（等待DDR大容量计算卡）

---

## 已完成工作

### 1. C仿真验证 ✅
- **测试配置**: srcSize=101, Nx=Ny=4, tccSize=81×81=6561
- **光源分布**: 环形光源 outerRadius=0.9
- **验证结果**: 所有相对误差 < 1e-5
  ```
  Real Mean Error:    3.85e-07
  Real StdDev Error:  1.90e-07
  Imag StdDev Error:  7.53e-08
  ```

### 2. HLS Pragma优化 ✅
- **添加PIPELINE II=1**: 内层循环流水线化
- **添加TRIPCOUNT**: 循环次数估算指导
- **添加BIND_STORAGE**: BRAM存储映射
- **固定数组**: 替代动态内存分配
- **C仿真通过**: 优化后功能正确

### 3. C综合验证 ✅
- **命令**: `make csynth`
- **生成RTL**: Verilog/VHDL
- **器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)

---

## C综合发现的问题

### 问题1: BRAM资源严重超限
```
需要:  2932 BRAM_18K
可用:  720 BRAM_18K (KU3P)
超限:  407%
```
**根本原因**: `pupil_storage` 数组尺寸过大
- 当前配置: `MAX_TCC_SIZE * MAX_SRC_SIZE * MAX_SRC_SIZE = 81 × 101 × 101 = 828,183`
- 每元素: 8 bytes (complex<float>)
- 总需求: ~6.6 MB → 需要2932个BRAM_18K

**解决方案** (Phase 2):
- 改用AXI-Master接口访问DDR存储
- pupil矩阵不再存储在BRAM，而是On-the-fly计算或DDR缓存
- 需要带DDR的大容量计算卡

### 问题2: 时钟频率无法满足
```
目标时钟:  5ns (200 MHz)
实际时钟:  25.733 ns (~38.86 MHz)
```
**根本原因**: 浮点加法链路延迟过大
- 关键路径: `fadd` 操作在 `tcc_accum_mx` 循环
- 组合延迟: 12.653 ns (超出时钟周期)

**解决方案** (可选):
1. 降低时钟频率要求 → 接受38MHz
2. 添加 `BIND_OP` 绑定DSP资源加速浮点运算
3. 使用 `hls::stream` 流式处理减少组合延迟

### 问题3: 循环延迟无法估算
- 多层嵌套循环使用变量边界
- HLS无法静态估算Latency

---

## 关键文件状态

### 源文件
| 文件 | 状态 | 说明 |
|------|------|------|
| `hls/src/calc_tcc.cpp` | 优化版 | HLS pragma已添加 |
| `hls/src/calc_tcc.h` | 完成 | 函数声明 |
| `hls/src/data_types.h` | 完成 | 数据类型定义 |

### 测试文件
| 文件 | 状态 | 说明 |
|------|------|------|
| `hls/tb/tb_calc_tcc.cpp` | 完成 | outerSigma=0.9 |
| `hls/golden/source_annular.bin` | 完成 | 101×101环状光源 |
| `hls/golden/tcc_expected_r.bin` | 完成 | TCC实部期望值 |
| `hls/golden/tcc_expected_i.bin` | 完成 | TCC虚部期望值 |

### 脚本文件
| 文件 | 状态 | 说明 |
|------|------|------|
| `hls/script/run_csim.tcl` | 完成 | C仿真脚本 |
| `hls/script/run_csynth.tcl` | 完成 | C综合脚本 |
| `hls/script/config/hls_config.cfg` | 完成 | HLS配置 |

### 输出文件
| 文件 | 状态 | 说明 |
|------|------|------|
| `hls_litho_proj/solution1/syn/report/calc_tcc_csynth.rpt` | 已生成 | 综合报告 |
| `hls_litho_proj/solution1/syn/verilog/` | 已生成 | RTL代码 |

---

## Phase 2 重构计划（待DDR计算卡就绪）

### 目标
1. **解决BRAM超限**: AXI-Master接口重构
2. **优化时钟频率**: 可能降低到50MHz目标
3. **CoSim验证**: RTL级仿真验证

### 技术方案

#### 方案A: On-the-fly计算（推荐）
- 不存储完整pupil矩阵
- 每次TCC累加时实时计算需要的pupil值
- 优点: 减少存储需求
- 缺点: 增加计算量（需权衡）

#### 方案B: DDR缓存
- 通过AXI-Master将pupil矩阵存入DDR
- 使用stream接口读取
- 需要DDR控制器（MIG IP核）

#### 方案C: 分块处理
- 将大矩阵分成小块，逐块处理
- 使用DATAFLOW并行化

### 接口设计建议
```cpp
void calc_tcc_top(
    // AXI-Lite控制接口
    volatile ap_uint<32>* ctrl_reg,
    // AXI4-Stream输入
    hls::stream<cmpxData_t>& s_axis_src,
    // AXI-Master存储
    ap_uint<32>* m_axi_mem,  // DDR访问
    // AXI4-Stream输出
    hls::stream<cmpxData_t>& m_axis_tcc
);
```

---

## 运行命令记录

### C仿真
```bash
cd /root/project/FPGA-Litho/source/TCC_HLS
make csim
```

### C综合
```bash
cd /root/project/FPGA-Litho/source/TCC_HLS
make csynth
```

### 验证输出
```bash
cat hls_litho_proj/solution1/syn/report/calc_tcc_csynth.rpt
```

---

## 下次继续时的建议

1. **首先检查DDR计算卡**: 确认DDR可用
2. **评估方案A/B/C**: 根据实际DDR带宽选择最优方案
3. **修改接口**: 添加AXI-Master接口
4. **重新C综合**: 验证资源利用率下降到合理范围
5. **CoSim验证**: RTL级仿真

---

## 统计摘要

| 项目 | Phase 1结果 |
|------|-------------|
| C仿真 | ✅ PASS (误差<1e-5) |
| C综合 | ✅ RTL生成 |
| BRAM利用率 | ❌ 407%超限 |
| 时钟频率 | ❌ 38MHz (目标200MHz) |
| DSP利用率 | 68/1368 (4.97%) |
| FF利用率 | 10394/325440 (3.2%) |
| LUT利用率 | 15637/162720 (9.6%) |

**结论**: Phase 1算法正确，但资源使用需DDR支持才能进入Phase 2。