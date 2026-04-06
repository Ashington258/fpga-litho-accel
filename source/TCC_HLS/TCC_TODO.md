# TCC HLS 重构任务清单

**模块**: TCC (Transmission Cross Coefficient) 计算
**创建时间**: 2026-04-06
**状态**: Phase 1 存档（等待DDR计算卡）

---

## 全局约束（TCC专用）

> **所有通用约束见 `.github/copilot-instructions.md`**

### 器件配置
| 参数 | 值 |
|------|-----|
| 目标器件 | xcku3p-ffvb676-2-e (Kintex UltraScale+) |
| 时钟目标 | 5ns (200 MHz) |
| 实际时钟 | 25.733ns (~38.86 MHz) **需优化** |

### 数据类型
```cpp
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;
```

### 关键尺寸约束
| 配置 | srcSize | Nx/Ny | tccSize | BRAM需求 |
|------|---------|-------|---------|----------|
| **最小(CoSim)** | 33 | 4/4 | 81×81=6561 | 6.6MB (超限) |
| **当前测试** | 101 | 4/4 | 81×81=6561 | 6.6MB (超限) |

**⚠️ BRAM资源超限**: KU3P仅720 BRAM_18K (~960KB)，当前需求2932 (407%)

---

## 已完成工作

### Phase 0: 项目初始化 ✅
| # | 任务 | 状态 |
|---|------|------|
| 0.1 | 目录结构 `source/TCC_HLS/` | ✅ |
| 0.2 | 数据类型 `data_types.h` | ✅ |
| 0.3 | HLS配置 `hls_config.cfg` | ✅ |
| 0.4 | TCL脚本 `run_csynth.tcl` | ✅ |
| 0.5 | Golden数据（outerRadius=0.9） | ✅ |
| 0.6 | 算法文档存档 | ✅ |
| 0.7 | 数组约束存档 | ✅ |

### Phase 1: calcTCC核心模块 ✅ (存档)
| # | 任务 | 状态 | 说明 |
|---|------|------|------|
| 1.1 | `calc_tcc.cpp` 实现 | ✅ | C仿真通过，误差<1e-5 |
| 1.2 | HLS PIPELINE优化 | ✅ | 添加II=1 pragma |
| 1.3 | hls::sin/cos/sqrt | ✅ | 替换标准库函数 |
| 1.4 | 固定数组替代动态分配 | ✅ | pupil_storage[MAX_*] |
| 1.5 | C仿真验证 | ✅ | 相对误差 < 1e-5 |
| 1.6 | C综合验证 | ✅ | RTL生成，发现BRAM超限 |
| 1.7 | 进度存档 | ✅ | `PROGRESS_PHASE1_ARCHIVE.md` |

---

## 待完成工作（等待DDR计算卡）

### Phase 2: AXI-Master重构
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 2.1 | pupil矩阵改用DDR存储 | P0 | 2d | BRAM利用率降至合理范围 | ⬜ |
| 2.2 | AXI-Master接口实现 | P0 | 1d | 接口报告显示M_AXI端口 | ⬜ |
| 2.3 | On-the-fly pupil计算（可选） | P1 | 2d | 减少存储需求 | ⬜ |
| 2.4 | 时钟频率优化 | P0 | 1d | 达到目标200MHz或降低目标 | ⬜ |
| 2.5 | C综合重新验证 | P0 | 1d | 资源利用率<100% | ⬜ |

### Phase 3: FFT模块
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 3.1 | 集成hls::fft | P0 | 2d | 编译通过 | ⬜ |
| 3.2 | 2D FFT实现（行+转置+列） | P0 | 2d | 功能正确 | ⬜ |
| 3.3 | fftshift移植 | P0 | 1d | 输出对齐CPU参考 | ⬜ |
| 3.4 | r2c/c2r独立测试 | P0 | 1d | CSIM PASS | ⬜ |

### Phase 4: calcImage模块
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 4.1 | `calc_image.cpp` 实现 | P1 | 2d | 功能正确 | ⬜ |
| 4.2 | TCC矩阵分块加载 | P1 | 1.5d | BRAM使用合理 | ⬜ |
| 4.3 | 循环流水线化 | P1 | 1.5d | Latency优化 | ⬜ |

### Phase 5: 系统集成
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 5.1 | `tcc_top.cpp` DATAFLOW顶层 | P0 | 2d | 子模块串联 | ⬜ |
| 5.2 | AXI-Lite寄存器映射 | P0 | 1d | 参数可配置 | ⬜ |
| 5.3 | AXI4-Stream输入接口 | P0 | 1d | S_AXIS端口 | ⬜ |
| 5.4 | 端到端CoSim | P0 | 3d | PASS | ⬜ |
| 5.5 | IP导出 | P0 | 1d | .zip生成 | ⬜ |

### Phase 6: Vivado验证
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 6.1 | Vivado Block Design | P0 | 2d | BD连线正确 | ⬜ |
| 6.2 | TCL验证脚本 | P0 | 1d | JTAG可启动 | ⬜ |
| 6.3 | 板级测试 | P0 | 2d | 输出正确 | ⬜ |

---

## 运行命令

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

### 验证报告
```bash
cat hls_litho_proj/solution1/syn/report/calc_tcc_csynth.rpt
```

---

## 关键问题记录

### BRAM超限问题
- **当前状态**: 2932 BRAM_18K / 720可用 = 407%
- **根本原因**: pupil_storage数组过大 (81×101×101)
- **解决方案**: 改用DDR存储或On-the-fly计算

### 时钟频率问题
- **目标**: 200 MHz (5ns)
- **实际**: 38.86 MHz (25.733ns)
- **原因**: 浮点加法链路延迟 (12.653ns)
- **解决方案**: 降低目标频率或优化数据通路

---

## 参考文档

| 文档 | 路径 |
|------|------|
| 算法公式 | `/memories/repo/ALGORITHM_MATH.md` |
| 数组约束 | `/memories/repo/ARRAY_SIZE_CONSTRAINTS.md` |
| CPU参考 | `/memories/repo/litho_tcc_reference.md` |
| 进度存档 | `hls/PROGRESS_PHASE1_ARCHIVE.md` |