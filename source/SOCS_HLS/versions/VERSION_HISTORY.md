# SOCS HLS 版本管理历史

本项目采用多版本并行开发策略，每个版本代表一种 FFT 实现方式。

---

## 版本列表

| 版本号 | 名称 | 状态 | FFT 实现方式 | 资源利用率 | 验证状态 |
|--------|------|------|--------------|------------|----------|
| v1.0-DFT-direct | DFT 直接计算 | ✅ 已完成 | 直接 DFT（O(N²)） | DSP 590% ❌ | C Sim ✅, Synth ✅ |
| v2.0-HLS-FFT-IP | HLS FFT IP | 🔄 开发中 | hls::fft IP（O(N log N)） | 待验证 | C Sim 待运行 |

---

## v1.0-DFT-direct（已完成，资源超限）

### 实现特点
- **算法**：直接 DFT 计算，无 FFT IP 依赖
- **代码**：`src/socs_simple.cpp`
- **复杂度**：O(N²) - 每个点需计算 sin/cos
- **实例化**：64 个 `sin_or_cos_double_s` 模块

### 验证结果
| 验证步骤 | 结果 | 详情 |
|----------|------|------|
| C Simulation | ✅ PASSED | RMSE 1.02e-07，0% error rate |
| C Synthesis | ✅ PASSED | 157,863 cycles, 789.3ms latency |
| Co-Simulation | ❌ 未执行 | 资源超限，无法继续 |
| Board Validation | ❌ 不可行 | 无法部署到目标器件 |

### 资源利用率（严重超限）
| 资源类型 | 使用量 | 百分比 | 状态 |
|----------|--------|--------|------|
| BRAM | 1,366 | 189% | ❌ 超限 |
| DSP | 8,080 | 590% | ❌ 超限 |
| FF | 556,361 | 170% | ❌ 超限 |
| LUT | 647,936 | 398% | ❌ 超限 |

### 性能指标
- **Latency**: 157,863 cycles
- **Interval**: 157,864 cycles
- **Timing**: Target 5ns, Slack 0.00ns（critical path）

### 结论
- ✅ 功能正确：算法实现与 CPU reference 一致
- ❌ 不可部署：资源需求远超 xcku3p-ffvb676-2-e 器件容量
- 🔄 需重构：切换到 HLS FFT IP 以降低资源消耗

---

## v2.0-HLS-FFT-IP（推荐方案）

### 实现特点
- **算法**：使用 Vitis HLS 内置 `hls::fft` IP
- **代码**：`src/socs_fft.cpp`
- **复杂度**：O(N log N) - Radix-2/4 Butterfly
- **资源优化**：共享 FFT IP，减少 DSP 使用

### 预期优势
| 对比项 | DFT 直接法 | HLS FFT IP |
|--------|------------|------------|
| DSP 使用 | 8,080 (590%) | ~50-100 (<10%) |
| LUT 使用 | 647K (398%) | ~50K (30%) |
| Latency | 157,863 cycles | ~4,000-8,000 cycles |
| II | 8（resource limit） | 1（可流水） |

### 开发计划
1. ✅ 复制 `socs_fft.cpp` 基础代码
2. 🔄 修复 FFT 配置问题（array-based vs stream-based API）
3. 🔄 运行 C Simulation 验证数值精度
4. 🔄 执行 C Synthesis 验证资源利用率
5. 🔄 Co-Simulation 验证 RTL 正确性
6. 🔄 Board Validation 验证硬件功能

---

## 文件路径映射

### v1.0-DFT-direct
```
source/SOCS_HLS/
├── src/
│   ├── socs_simple.cpp         # DFT 直接计算实现
│   └── socs_simple.h           # 头文件
├── script/config/
│   └── hls_config_socs_simple.cfg  # HLS 配置
├── socs_simple_synth/          # 综合输出
│   ├── hls/syn/report/         # 综合报告
│   └── vitis-comp.json         # 组件配置
└── vivado_bd/
    └── fix_hls_address_overlap.tcl  # Vivado 地址修复脚本
```

### v2.0-HLS-FFT-IP
```
source/SOCS_HLS/
├── src/
│   ├── socs_fft.cpp            # HLS FFT IP 实现
│   └── socs_fft.h              # 头文件（FFT 配置）
├── script/config/
│   └── hls_config_socs_fft.cfg # HLS 配置（待创建）
├── socs_fft_comp/              # 综合输出（待创建）
└── tb/
    └── tb_socs_fft.cpp         # 测试 bench（待创建）
```

---

## 版本切换指南

### 切换到 v1.0-DFT-direct（仅用于功能验证）
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_socs_simple.cfg --work_dir socs_simple_csim
```

### 切换到 v2.0-HLS-FFT-IP（推荐用于实际部署）
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS
# 待 hls_config_socs_fft.cfg 创建后执行
vitis-run --mode hls --csim --config script/config/hls_config_socs_fft.cfg --work_dir socs_fft_csim
```

---

## Git 提交记录

| 提交 SHA | 版本 | 描述 |
|----------|------|------|
| f298039 | v1.0-DFT | feat(SOCS_HLS): Add DFT-based SOCS implementation |
| TBD | v2.0-FFT | feat(SOCS_HLS): Add HLS FFT IP implementation |

---

## 参考文献

- **FFT IP 参考**：`reference/vitis_hls_ftt的实现/interface_stream/`
- **CPU Reference**：`verification/src/litho.cpp`
- **Golden 数据**：`output/verification/`
- **器件规格**：xcku3p-ffvb676-2-e (Kintex UltraScale+)

---

*最后更新：2026-04-15*