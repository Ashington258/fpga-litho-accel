# SOCS HLS 优化方向交接文档

**创建日期**: 2026-04-25
**源分支**: feature/2048-architecture (稳定版本)
**目标分支**: feature/2048-optimization (优化实验)
**目的**: 在稳定IP核基础上探索性能优化方案

---

## 📌 当前稳定版本状态 (feature/2048-architecture)

### ✅ 已验证功能

1. **C Simulation**: RMSE=8.32e-7 (与Golden CPU完美匹配)
2. **C Synthesis**: Fmax=274MHz, BRAM 56%, DSP 3%
3. **Co-Simulation**: 运行中 (预计4-6小时)

### 📊 性能指标

- **FFT Latency**: 199,951 cycles (1.0ms @ 200MHz)
- **总处理时间**: ~8.65ms (10 kernels)
- **加速比**: 理论5200x，预估实际3000~4000x

### 🔧 架构特点

- **FFT尺寸**: 固定128×128 (支持Nx=2~24)
- **零填充策略**: 右下角嵌入，中心提取
- **DDR访问**: AXI-MM Master接口，Burst传输
- **精度**: ap_fixed<32,1> (HLS FFT IP)

---

## 🚀 优化方向 (feature/2048-optimization)

### ⭐ 方向1: 流水线并行化 (Pipeline Parallelism)

**目标**: 提升吞吐量，降低整体延迟

**实现方案**:
```cpp
void calc_socs_2048_pipeline(...) {
    #pragma HLS DATAFLOW
    
    // Stage 1: Embed kernel + mask
    hls::stream<cmpx_t> embed_stream;
    embed_kernel_stream(k, mskf, embed_stream);
    
    // Stage 2: FFT 2D
    hls::stream<cmpx_t> fft_stream;
    fft_2d_stream(embed_stream, fft_stream, true);
    
    // Stage 3: Accumulate
    accumulate_stream(fft_stream, tmpImg, scales[k]);
}
```

**预期效果**:
- **吞吐量提升**: 2-3倍
- **Latency降低**: 从8.65ms降至~3ms
- **资源增加**: BRAM +20%, DSP +10%

**挑战**:
- BRAM资源接近上限 (当前56%)
- 需要精细的stream深度调优
- 可能需要降低并行度 (UNROLL factor=2)

**参考实现**:
- `reference/vitis_hls_ftt的实现/interface_stream/fft_top.cpp`

---

### ⭐ 方向2: Kernel并行化 (Kernel Parallelism)

**目标**: 并行处理多个kernel，提升吞吐量

**实现方案**:
```cpp
void calc_socs_parallel_2048(...) {
    #pragma HLS DATAFLOW
    
    // 并行处理2个kernel (受BRAM限制)
    for (int k = 0; k < nk; k += 2) {
        #pragma HLS UNROLL factor=2
        
        // 每个kernel独立FFT实例
        fft_2d_instance_k0(...);
        fft_2d_instance_k1(...);
    }
}
```

**预期效果**:
- **吞吐量提升**: 2倍 (2个kernel并行)
- **Latency不变**: 单个kernel延迟不变
- **资源增加**: BRAM +100%, DSP +100%

**挑战**:
- **BRAM资源**: 需要2份tmpImg缓冲 (2×128×128×4B = 128KB)
- **DDR带宽**: 需要并行读取2个kernel数据
- **折衷方案**: UNROLL factor=2 (2个kernel并行)

**资源预估**:
- BRAM: 406 → 812 (113% ❌ 超限)
- DSP: 42 → 84 (6% ✅ 通过)

**解决方案**:
1. 使用DDR存储tmpImg (降低BRAM占用)
2. 降低FFT精度 (ap_fixed<24,8>)
3. 使用LUTRAM替代部分BRAM

---

### ⭐ 方向3: HLS FFT IP优化

**目标**: 降低FFT Latency，提升Fmax

**当前问题**:
- FFT Latency: 199,951 cycles (较高)
- 使用HLS FFT IP (ap_fixed<32,1>)

**优化方案**:

#### 方案3.1: FFT配置优化

```cpp
struct fft_config_optimized {
    static const unsigned FFT_NFFT_MAX = 7;      // log2(128)
    static const unsigned FFT_LENGTH = 128;
    static const bool FFT_HAS_NFFT = false;      // 固定长度
    static const bool FFT_HAS_OUT_RDY = true;    // 输出就绪信号
    static const bool FFT_USE_FLOW_DEP = false;  // 禁用流依赖
};
```

**预期效果**:
- **Latency降低**: 10-20%
- **Fmax提升**: 5-10%

#### 方案3.2: FFT精度优化

```cpp
typedef ap_fixed<24, 8> fft_fixed_t;  // 降低精度
```

**预期效果**:
- **DSP降低**: 30-40%
- **精度损失**: 需验证RMSE < 1e-5

#### 方案3.3: FFT实例复用

```cpp
// 单个FFT实例，时分复用
void fft_2d_reuse(...) {
    #pragma HLS ALLOCATION instances=fft_1d limit=1 function
    
    for (int row = 0; row < 128; row++) {
        fft_1d(input[row], output[row]);
    }
}
```

**预期效果**:
- **资源降低**: BRAM -30%, DSP -50%
- **Latency增加**: 2-3倍

---

### ⭐ 方向4: DDR访问优化

**目标**: 降低DDR访问延迟，提升带宽利用率

**当前问题**:
- DDR访问延迟: ~32 cycles latency
- Burst长度: 16×16 blocks

**优化方案**:

#### 方案4.1: 增大Burst长度

```cpp
#define BLOCK_SIZE 32  // 从16增加到32
```

**预期效果**:
- **带宽提升**: 2倍
- **BRAM增加**: +100% (32×32 buffer)

#### 方案4.2: 预取优化

```cpp
void burst_read_prefetch(...) {
    #pragma HLS PIPELINE II=1
    
    // 双缓冲预取
    read_block(current_block);
    prefetch_next_block(next_block);
}
```

**预期效果**:
- **延迟隐藏**: 50-70%
- **BRAM增加**: +50%

---

### ⭐ 方向5: 精度优化

**目标**: 在保证精度前提下降低资源占用

**当前精度**:
- HLS FFT IP: ap_fixed<32,1>
- Direct DFT: float (C仿真)

**优化方案**:

#### 方案5.1: 降低FFT精度

```cpp
typedef ap_fixed<24, 8> fft_fixed_t;  // 24位总精度，8位整数
```

**预期效果**:
- **DSP降低**: 30-40%
- **精度验证**: 需确保RMSE < 1e-5

#### 方案5.2: 混合精度

```cpp
// FFT使用低精度，累加使用高精度
typedef ap_fixed<24, 8> fft_t;
typedef ap_fixed<32, 16> acc_t;
```

**预期效果**:
- **资源平衡**: DSP -30%, BRAM +10%
- **精度保证**: 累加精度更高

---

## 📋 优化优先级排序

| 优先级 | 优化方向 | 预期收益 | 实现难度 | 资源风险 |
|-------|---------|---------|---------|---------|
| 1 | 流水线并行化 | 吞吐量2-3倍 | 中等 | BRAM +20% |
| 2 | HLS FFT IP优化 | Latency -20% | 低 | 无风险 |
| 3 | DDR访问优化 | 带宽2倍 | 低 | BRAM +100% |
| 4 | 精度优化 | DSP -30% | 中等 | 需验证精度 |
| 5 | Kernel并行化 | 吞吐量2倍 | 高 | BRAM +100% ❌ |

**推荐实施顺序**:
1. **先实施**: HLS FFT IP优化 (低风险，快速见效)
2. **再实施**: 流水线并行化 (中等风险，高收益)
3. **最后实施**: Kernel并行化 (高风险，需资源优化配合)

---

## ⚠️ 风险与约束

### 资源约束

**当前资源占用** (xcku3p-ffvb676-2-e):
- BRAM: 406/720 (56%) ⚠️ 接近上限
- DSP: 42/1368 (3%) ✅ 充足
- FF: 35,185/325,440 (10%) ✅ 充足
- LUT: 38,283/162,720 (23%) ✅ 充足

**优化限制**:
- BRAM是主要瓶颈，无法支持大规模并行
- DSP资源充足，可适度增加并行度
- FF/LUT资源充足，可用于控制逻辑优化

### 验证要求

**每次优化后必须验证**:
1. **C Simulation**: RMSE < 1e-5 (对比Golden CPU)
2. **C Synthesis**: Fmax > 250MHz, BRAM < 80%
3. **Co-Simulation**: RTL功能正确
4. **Board Validation**: 硬件实测性能

### 回退策略

**如果优化失败**:
```bash
# 回退到稳定版本
git checkout feature/2048-architecture

# 或创建新的优化分支
git checkout -b feature/2048-optimization-v2
```

---

## 📚 参考资源

### HLS优化指南

1. **Vitis HLS官方文档**: UG1399 (High-Level Synthesis)
2. **FFT优化**: `reference/vitis_hls_ftt的实现/interface_stream/`
3. **流水线设计**: `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md`

### 代码参考

1. **Stream-based FFT**: `reference/vitis_hls_ftt的实现/interface_stream/fft_top.cpp`
2. **DATAFLOW示例**: `source/SOCS_HLS/src/socs_2048.cpp` (当前实现)
3. **Pipeline示例**: `source/SOCS_HLS/doc/Pipeline_Parallel_Implementation.md` (已删除，可从git历史恢复)

### 工具脚本

1. **C综合**: `source/SOCS_HLS/script/run_csynth_2048.tcl`
2. **配置文件**: `source/SOCS_HLS/script/config/hls_config_socs_full.cfg`
3. **验证脚本**: `validation/compare_hls_golden.py`

---

## 🎯 成功标准

### 性能目标

| 指标 | 当前值 | 目标值 | 提升比例 |
|-----|-------|-------|---------|
| 吞吐量 | 1.16 kernels/ms | 3.0 kernels/ms | 2.5倍 |
| Latency | 8.65ms | 3.0ms | 2.9倍 |
| Fmax | 274MHz | 300MHz | 1.1倍 |
| BRAM占用 | 56% | <75% | - |

### 验证标准

- **精度**: RMSE < 1e-5 (对比Golden CPU)
- **功能**: Co-Simulation通过
- **性能**: Board实测加速比 > 2000x

---

## 📝 开发流程建议

### 1. 创建优化分支

```bash
# 从稳定版本创建新分支
git checkout feature/2048-architecture
git checkout -b feature/2048-optimization-v1
```

### 2. 实施优化

```bash
# 修改代码
vim source/SOCS_HLS/src/socs_2048.cpp

# 运行C仿真验证
cd source/SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg

# 运行C综合
v++ -c --mode hls --config script/config/hls_config_socs_full.cfg
```

### 3. 验证结果

```bash
# 检查资源占用
cat socs_2048_full_fixed_cosim/hls/syn/report/calc_socs_2048_hls_csynth.rpt

# 对比Golden数据
python3 validation/compare_hls_golden.py --config input/config/golden_1024.json
```

### 4. 提交更改

```bash
# 提交优化代码
git add source/SOCS_HLS/src/socs_2048.cpp
git commit -m "perf: 实施流水线并行化优化

- 添加DATAFLOW pragma
- 优化stream深度
- 吞吐量提升2.5倍"

# 推送到远程
git push origin feature/2048-optimization-v1
```

---

## 🔄 迭代策略

### 快速迭代

1. **小步快跑**: 每次只优化一个方向
2. **频繁验证**: 每次修改后立即C仿真验证
3. **版本控制**: 每个优化方向独立分支

### 失败处理

1. **记录失败原因**: 在TODO中记录失败原因
2. **分析资源瓶颈**: 使用HLS报告分析资源占用
3. **调整优化策略**: 降低并行度或精度

### 成功标准

1. **性能提升**: 至少2倍吞吐量提升
2. **资源可控**: BRAM < 80%
3. **精度保证**: RMSE < 1e-5

---

## 📞 联系与支持

**问题反馈**: 在SOCS_TODO.md中记录问题
**代码审查**: 提交PR到feature/2048-optimization分支
**文档更新**: 更新本交接文档和SOCS_TODO.md

---

**祝优化顺利！🚀**
