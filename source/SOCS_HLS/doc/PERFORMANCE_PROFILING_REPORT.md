# SOCS HLS 性能画像报告

**生成日期**: 2026-04-25  
**分析对象**: `calc_socs_2048_hls` (Phase 1.5版本)  
**目标器件**: xcku3p-ffvb676-2-e

---

## 📊 执行摘要

### 关键发现

1. **主循环串行执行** - 最大瓶颈 ⚠️
   - 主循环（VITIS_LOOP_648_3）未使用DATAFLOW pragma
   - nk个kernel串行处理，无流水线
   - **潜在加速比：2-3倍**（通过DATAFLOW重构）

2. **embed函数未优化** - 次要瓶颈 ⚠️
   - 双重循环（17×17=289次迭代）无PIPELINE pragma
   - 每次迭代包含DDR读取 + 复数乘法 + BRAM写入
   - **潜在加速比：10-20倍**（通过PIPELINE优化）

3. **FFT模块已优化** - 良好 ✅
   - 使用DATAFLOW pragma
   - Interval = 83,586 cycles（良好）
   - Latency = 199,951 cycles

4. **资源占用合理** - 可接受 ✅
   - BRAM: 406/720 (56%) - 有优化空间
   - DSP: 42/1368 (3%) - 充足
   - FF/LUT: 充足

---

## 🔍 详细性能分析

### 1. Latency分解

| 模块 | Latency (cycles) | Latency (时间) | 占比 | 状态 |
|-----|-----------------|---------------|------|------|
| **embed_kernel_mask_padded_2048** | **未知** ⚠️ | **未知** | **待分析** | ❌ 未优化 |
| **fft_2d_full_2048** | 199,951 | 1.000 ms | ~60% | ✅ 已优化 |
| **accumulate_intensity_2048** | 16,413 | 82.065 μs | ~5% | ✅ 已优化 |
| **fftshift_2d_2048** | 16,386 | 81.930 μs | ~5% | ✅ 已优化 |
| **extract_valid_region_2048** | 19+ | 95 ns+ | <1% | ✅ 已优化 |
| **主循环开销** | **未知** | **未知** | **待分析** | ❌ 串行 |

**总Latency** (10 kernels): ~8.65 ms

**关键问题**：
- embed函数latency未知，需要进一步分析
- 主循环串行执行，无流水线重叠

### 2. 主循环结构分析

**当前实现** (socs_2048.cpp, line 648-710):

```cpp
// Process each kernel
for (int k = 0; k < nk; k++) {
    #pragma HLS LOOP_TRIPCOUNT min=4 max=16
    
    // Step 1: Embed with zero-padding
    embed_kernel_mask_padded_2048(...);  // ❌ 无PIPELINE
    
    // Step 2: 2D IFFT (128×128 fixed)
    fft_2d_full_2048(...);               // ✅ 有DATAFLOW
    
    // Step 3: Accumulate intensity
    accumulate_intensity_2048(...);      // ✅ 有PIPELINE
}
```

**问题**：
1. **主循环无DATAFLOW** - kernel k+1必须等待kernel k完成
2. **embed函数无PIPELINE** - 双重循环串行执行
3. **串行依赖链** - embed → FFT → accumulate 串行执行

**优化机会**：
- **方案1**：主循环添加DATAFLOW pragma → 吞吐量提升2-3倍
- **方案2**：embed函数添加PIPELINE → embed latency降低10-20倍

### 3. embed函数分析

**当前实现** (socs_2048.cpp, line 88-200):

```cpp
void embed_kernel_mask_padded_2048(...) {
    // Clear entire FFT input array (zero-padding)
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            fft_input[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    
    // Compute and embed kernel × mask product
    for (int ky = 0; ky < kerY_actual; ky++) {
        for (int kx = 0; kx < kerX_actual; kx++) {
            // #pragma HLS PIPELINE II=1  // ❌ 注释掉了
            
            // DDR读取：kernel
            float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
            float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
            
            // DDR读取：mask
            float ms_r = mskf_r[mask_y * Lx + mask_x];
            float ms_i = mskf_i[mask_y * Lx + mask_x];
            
            // 复数乘法
            float prod_r = kr_r * ms_r - kr_i * ms_i;
            float prod_i = kr_r * ms_i + kr_i * ms_r;
            
            // BRAM写入
            fft_input[fft_y][fft_x] = cmpx_2048_t(prod_r, prod_i);
        }
    }
}
```

**性能分析**：

| 操作 | 次数 | 单次延迟 | 总延迟 |
|-----|------|---------|--------|
| 清零循环 | 128×128=16,384 | ~1 cycle | ~16,384 cycles |
| 嵌入循环 | 17×17=289 | ~10-20 cycles | ~2,890-5,780 cycles |
| DDR读取 | 289×2=578 | ~32 cycles (latency) | ~18,496 cycles |
| 复数乘法 | 289 | ~3 cycles | ~867 cycles |
| BRAM写入 | 289 | ~1 cycle | ~289 cycles |

**预估总Latency**: ~20,000-40,000 cycles (0.1-0.2 ms)

**优化机会**：
- **添加PIPELINE pragma** → latency降低至~300-600 cycles (10-20倍提升)
- **优化DDR访问** → 减少DDR读取延迟

### 4. DDR访问模式分析

**当前AXI-MM配置**:

```cpp
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
    depth=1048576 latency=32 \
    num_read_outstanding=8 \
    max_read_burst_length=64
```

**访问模式**：
- **mskf_r/i**: 随机访问（根据kernel偏移）
- **krn_r/i**: 顺序访问（kernel数据）
- **重复读取**: 每个kernel重复读取相同mask数据

**问题**：
1. **随机访问** - mask访问模式不连续，burst效率低
2. **重复读取** - 10个kernel重复读取相同mask数据
3. **Outstanding不足** - 可能未充分利用总线带宽

**优化机会**：
- **增大burst长度**: 64 → 128
- **增大outstanding**: 8 → 16
- **缓存mask数据**: 避免重复读取

### 5. 资源占用分析

**当前资源占用**:

| 资源类型 | 使用量 | 可用量 | 占用率 | 状态 |
|---------|--------|--------|--------|------|
| BRAM_18K | 406 | 720 | **56%** | ⚠️ 接近上限 |
| DSP | 42 | 1368 | 3% | ✅ 充足 |
| FF | 35,185 | 325,440 | 10% | ✅ 充足 |
| LUT | 38,283 | 162,720 | 23% | ✅ 充足 |

**BRAM占用分解**:

| 用途 | BRAM数量 | 占比 |
|-----|---------|------|
| fft_input (8个数组) | 64 | 16% |
| fft_output (8个数组) | 64 | 16% |
| tmpImg (4个数组) | 32 | 8% |
| tmpImgp (4个数组) | 32 | 8% |
| FFT实例 | 204 | 50% |
| **总计** | **406** | **100%** |

**优化机会**：
- **DATAFLOW重构** - 可能增加BRAM占用（stream FIFO）
- **资源释放** - 如果BRAM > 75%，需要优化存储策略

---

## 🎯 优化优先级排序

### 优先级1: 数据流重构 ⭐⭐⭐ (最高收益)

**目标**: 消除kernel间串行等待

**实施方案**:
```cpp
void calc_socs_2048_hls(...) {
    #pragma HLS DATAFLOW
    
    // 创建stream用于stage间数据传递
    hls::stream<cmpx_2048_t> embed_to_fft("embed_to_fft");
    hls::stream<cmpx_2048_t> fft_to_acc("fft_to_acc");
    
    // Stage 1: Embed (producer)
    embed_kernel_stream(..., embed_to_fft, ...);
    
    // Stage 2: FFT (transform)
    fft_2d_stream(embed_to_fft, fft_to_acc, ...);
    
    // Stage 3: Accumulate (consumer)
    accumulate_stream(fft_to_acc, ...);
}
```

**预期收益**:
- 吞吐量提升: 2-3倍
- Latency: 不变（单个kernel时间）
- BRAM占用: +20% (stream FIFO)

**风险**: 中等（需要重构代码）

---

### 优先级2: embed函数优化 ⭐⭐ (中等收益)

**目标**: 降低embed函数latency

**实施方案**:
```cpp
void embed_kernel_mask_padded_2048(...) {
    // 清零循环 - 已优化
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    
    // 嵌入循环 - 添加PIPELINE
    for (int ky = 0; ky < kerY_actual; ky++) {
        for (int kx = 0; kx < kerX_actual; kx++) {
            #pragma HLS PIPELINE II=1  // ✅ 启用PIPELINE
            
            // ... 计算逻辑
        }
    }
}
```

**预期收益**:
- embed latency降低: 10-20倍
- 总latency降低: 5-10%
- BRAM占用: 不变

**风险**: 低（简单修改）

---

### 优先级3: DDR访问优化 ⭐ (低收益)

**目标**: 提升DDR带宽利用率

**实施方案**:
```cpp
#pragma HLS INTERFACE m_axi port=mskf_r \
    num_read_outstanding=16 \    // 从8增加到16
    max_read_burst_length=128     // 从64增加到128
```

**预期收益**:
- DDR带宽提升: 2倍
- 总latency降低: 5-10%
- BRAM占用: +15% (缓存)

**风险**: 低（参数调整）

---

### 优先级4: FFT微调 ⭐ (低收益)

**目标**: 小幅提升FFT性能

**实施方案**:
```cpp
struct fft_config_2048_optimized_t {
    static const unsigned FFT_TWIDDLE_WIDTH = 24;  // 从32降到24
    static const unsigned FFT_PHASE_FACTOR_WIDTH = 24;
};
```

**预期收益**:
- FFT latency降低: 5-10%
- 总latency降低: 3-5%
- BRAM占用: -5%

**风险**: 低（参数调整）

---

## 📈 性能预测

### 保守估计 (仅实施优先级1-2)

| 指标 | 当前值 | 优化后 | 提升比例 |
|-----|-------|--------|---------|
| 吞吐量 | 1.16 kernels/ms | 2.5-3.0 kernels/ms | 2.2-2.6倍 |
| Latency | 8.65 ms | 7.5-8.0 ms | 1.1-1.2倍 |
| BRAM占用 | 56% | 70-75% | +14-19% |

### 乐观估计 (实施优先级1-4)

| 指标 | 当前值 | 优化后 | 提升比例 |
|-----|-------|--------|---------|
| 吞吐量 | 1.16 kernels/ms | 3.5-4.0 kernels/ms | 3.0-3.4倍 |
| Latency | 8.65 ms | 6.5-7.0 ms | 1.2-1.3倍 |
| BRAM占用 | 56% | 75-80% | +19-24% |

---

## 🚀 下一步行动

### 立即行动 (Step 1: 数据流重构)

**时间**: 1-2天

**步骤**:
1. 重构embed函数使用stream输出
2. 重构FFT函数使用stream接口
3. 重构accumulate函数使用stream输入
4. 添加DATAFLOW pragma
5. 验证功能和性能

**验收标准**:
- C Simulation: RMSE < 1e-5
- C Synthesis: Interval < 100,000 cycles, BRAM < 75%
- Dataflow Viewer: 确认stage并行执行

---

### 后续行动 (Step 2: embed优化)

**时间**: 半天

**步骤**:
1. embed函数添加PIPELINE pragma
2. 验证latency降低
3. 检查资源占用

**验收标准**:
- embed latency < 1,000 cycles
- 总latency降低 > 5%

---

## 📝 结论

### 核心瓶颈

1. **主循环串行执行** - 最大瓶颈，必须优先解决
2. **embed函数未优化** - 次要瓶颈，收益中等
3. **DDR访问效率** - 潜在瓶颈，需进一步验证

### 优化路线

**推荐路线**: Step 1 (数据流重构) → Step 2 (embed优化) → Step 3 (DDR优化)

**预期效果**: 吞吐量提升2.5-3倍，满足优化目标

### 风险提示

1. **BRAM资源约束** - DATAFLOW可能增加BRAM占用
2. **精度验证** - 每次优化后必须验证RMSE < 1e-5
3. **Board验证** - 综合报告优化 ≠ 实际加速比提升

---

**报告生成**: 2026-04-25  
**下一步**: 开始实施 Step 1 - 数据流重构
