# Step 5: 有限并行化设计方案

**日期**: 2026-04-25  
**目标**: 分析并行化可行性，确定优化方向

---

## 📊 当前状态分析

### 资源占用（stream_optimized版本）

| 资源类型 | 使用量 | 可用量 | 占用率 | 余量 |
|---------|--------|--------|--------|------|
| BRAM_18K | 406 | 720 | 56% | **314** |
| DSP | 42 | 1368 | 3% | 1,326 |
| FF | 35,185 | 325,440 | 10% | 290,255 |
| LUT | 38,283 | 162,720 | 23% | 124,437 |

### 性能指标

| 指标 | 当前值 | 目标值 | 状态 |
|-----|-------|-------|------|
| Fmax | 274 MHz | 300 MHz | ⚠️ 接近目标 |
| FFT Interval | 83,458 cycles | - | ✅ 良好 |
| 主循环Interval | 963,578 cycles | - | ❌ 串行瓶颈 |
| 吞吐量 | 1.16 kernels/ms | 2.0 kernels/ms | ⚠️ 需优化 |

### 关键发现

1. **✅ FFT已使用HLS FFT IP**
   - C综合时使用HLS FFT IP（通过`#ifdef __SYNTHESIS__`）
   - C仿真时使用直接DFT（HLS FFT IP在C仿真中不可用）
   - FFT性能良好（Interval: 83,458 cycles）

2. **❌ 主循环串行执行**
   - 10个kernel顺序处理
   - 无DATAFLOW pragma
   - 主要性能瓶颈

3. **✅ 系统是compute-bound**
   - DDR优化收益有限（Step 2.1验证）
   - 应聚焦于计算并行化

---

## 🎯 并行化可行性分析

### 方案1：主循环DATAFLOW ❌ 不可行

**问题**：
- Accumulate依赖：tmpImg需要累积所有kernel结果
- Stream资源：10个kernel × 128×128 × 8B = 1.31MB（~728 BRAM，101%超限）
- FFT实例：需要多个FFT实例并行（资源超限）

**结论**：DATAFLOW并行化会导致资源超限，不可行。

### 方案2：UNROLL factor=2 ❌ 不可行

**问题**：
- 需要复制FFT实例（2份）
- BRAM需求：795 BRAM（110%超限）
- DSP需求：84 DSP（6%，可接受）

**结论**：完全并行化会导致BRAM超限，不可行。

### 方案3：部分并行化 ⚠️ 收益有限

**策略**：
- 仅并行embed阶段
- FFT和accumulate仍串行

**问题**：
- embed阶段占比小（~10%时间）
- 整体提升有限（~1.1倍）
- 资源仍可能超限

**结论**：收益不明显，不推荐。

---

## 🎯 推荐优化方向

### 方向1：提升Fmax ⭐⭐⭐

**当前**: 274 MHz  
**目标**: 300 MHz  
**提升**: 1.1倍吞吐量

**实施**：
```tcl
# hls_config.cfg
clock=300000000  # 3.33ns
flow_target=vivado
```

**预期效果**：
- 吞吐量提升: 1.1倍
- 实施难度: 低
- 风险: 低

### 方向2：优化FFT配置 ⭐⭐

**当前**: HLS FFT IP (pipelined_streaming_io)  
**优化**: 检查FFT配置是否最优

**检查项**：
- FFT implementation_options
- Scaling策略
- Precision设置

**预期效果**：
- FFT性能提升: 1.2-1.5倍
- 实施难度: 中等
- 风险: 中等

### 方向3：降低精度 ⭐⭐⭐⭐

**当前**: float (32-bit)  
**目标**: ap_fixed<24,8> (24-bit)  
**目的**: 节省资源，为并行化腾出空间

**预期效果**：
- BRAM节省: 20-30%
- DSP节省: 30%
- 精度损失: 需验证RMSE < 1e-5

**实施步骤**：
1. 修改FFT配置使用ap_fixed<24,8>
2. 验证精度是否达标
3. 如果资源充足，尝试UNROLL factor=2

---

## 📊 优化路径建议

### Phase 1: Fmax优化（立即实施）

**原因**：
- 简单易行
- 无需修改代码
- 风险低

**预期收益**：
- 吞吐量提升: 1.1倍
- 实施时间: 1小时

### Phase 2: 精度优化（短期）

**原因**：
- 节省资源
- 为并行化创造条件
- 需要验证精度

**预期收益**：
- BRAM节省: 20-30%
- 实施时间: 2-3小时

### Phase 3: 有限并行化（中期）

**前提**：Phase 2成功，资源充足

**策略**：
- UNROLL factor=2
- 仅并行embed阶段
- 监控资源占用

**预期收益**：
- 吞吐量提升: 1.5-1.8倍
- 实施时间: 4-6小时

---

## 🎯 决策建议

**推荐方案**: **Phase 1 + Phase 2组合**

**理由**：
1. ✅ 风险可控
2. ✅ 实施简单
3. ✅ 收益明显（1.3-1.5倍提升）
4. ✅ 为后续并行化创造条件

**下一步行动**：
1. 修改hls_config.cfg，提升Fmax到300MHz
2. 运行C综合验证时序
3. 如果成功，继续Phase 2精度优化
4. 如果资源充足，尝试Phase 3有限并行化

---

## ⚠️ 重要结论

**并行化不可行的根本原因**：
1. **BRAM资源限制**：当前56%占用，余量仅314 BRAM
2. **FFT实例资源消耗大**：单个FFT实例占用~648 BRAM
3. **Accumulate依赖**：tmpImg累积特性限制了并行化

**替代优化方向**：
1. **提升Fmax**：简单有效
2. **降低精度**：节省资源
3. **优化FFT配置**：提升FFT性能

**建议**：
- 放弃完全并行化方案
- 聚焦于Fmax和精度优化
- 如果资源充足，再考虑部分并行化

---

## 📝 实施步骤

### Step 1: 降低FFT精度

```cpp
// 修改 hls_fft_config_2048_corrected.h
typedef ap_fixed<24, 8> fft_data_t;  // 从32位降到24位

struct fft_config_2048_optimized_t {
    static const unsigned FFT_INPUT_WIDTH = 24;
    static const unsigned FFT_TWIDDLE_WIDTH = 18;
    static const unsigned FFT_PHASE_FACTOR_WIDTH = 18;
};
```

### Step 2: 使用LUTRAM替代BRAM

```cpp
// 修改 socs_2048.cpp
#pragma HLS bind_storage variable=fft_input type=RAM_2P impl=LUTRAM
#pragma HLS bind_storage variable=fft_output type=RAM_2P impl=LUTRAM
```

### Step 3: 实现部分并行化

```cpp
void calc_socs_2048_parallel(...) {
    // 并行处理2个kernel的embed阶段
    for (int k = 0; k < nk; k += 2) {
        #pragma HLS UNROLL factor=2
        
        // 并行embed 2个kernel
        embed_kernel_mask_padded_2048(..., k, fft_input_0);
        embed_kernel_mask_padded_2048(..., k+1, fft_input_1);
        
        // 串行FFT和accumulate
        fft_2d_full_2048(fft_input_0, fft_output_0, true);
        accumulate_intensity_2048(fft_output_0, tmpImg, scales[k]);
        
        fft_2d_full_2048(fft_input_1, fft_output_1, true);
        accumulate_intensity_2048(fft_output_1, tmpImg, scales[k+1]);
    }
}
```

### Step 4: 验证

```bash
# C仿真验证精度
vitis-run --mode hls --csim --config script/config/hls_config_socs_parallel.cfg

# C综合验证资源
v++ -c --mode hls --config script/config/hls_config_socs_parallel.cfg
```

---

## ⚠️ 风险评估

### 风险1: 精度损失
- **影响**: ap_fixed<24,8>可能影响计算精度
- **缓解**: 验证RMSE < 1e-5
- **回退**: 如果精度不达标，使用ap_fixed<26,8>

### 风险2: LUTRAM资源
- **影响**: LUTRAM可能增加LUT占用
- **缓解**: 监控LUT占用率 < 80%
- **回退**: 如果LUT超限，降低并行度

### 风险3: 时序收敛
- **影响**: 并行化可能影响时序
- **缓解**: 检查Fmax > 250MHz
- **回退**: 如果时序不达标，降低并行度

---

## 📊 预期收益

| 指标 | 当前值 | 目标值 | 提升比例 |
|-----|-------|-------|---------|
| 吞吐量 | 1.16 kernels/ms | 2.0 kernels/ms | 1.7倍 |
| Latency | 8.65ms | 5.0ms | 1.7倍 |
| BRAM占用 | 56% | 76% | +20% |
| DSP占用 | 3% | 3% | 持平 |

---

## 🎯 决策建议

**推荐方案**: **方案D（UNROLL factor=2 + 资源优化组合）**

**理由**：
1. ✅ BRAM占用76%，在安全范围内
2. ✅ 吞吐量提升1.7倍，收益显著
3. ✅ 实施难度中等，风险可控
4. ✅ 可逐步验证，失败可回退

**下一步行动**：
1. 实施FFT精度优化（Step 1）
2. 验证精度是否达标
3. 实施LUTRAM优化（Step 2）
4. 实施并行化（Step 3）
5. 验证整体性能和资源
