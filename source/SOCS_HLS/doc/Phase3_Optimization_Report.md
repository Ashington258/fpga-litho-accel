# Phase 3 BRAM优化总结报告

**日期**: 2026-04-25  
**优化目标**: 降低BRAM占用率至75%以下  
**最终结果**: BRAM 44% ✅ 达标

---

## 📊 优化历程

### v5: 直接累积到DDR (2026-04-24)

**问题**: BRAM占用988 (137%) ❌ 超限

**优化思路**:
- 将tmpImg_final从BRAM移至DDR存储
- 使用AXI-MM接口访问DDR

**实施结果**:
- BRAM: 986 (137%) ❌ 未改善
- 原因: fftshift仍使用BRAM数组 (tmpImgp, tmpImg_shifted)

**关键发现**:
- 仅移动tmpImg_final不足以解决BRAM超限
- 需要移除所有临时BRAM数组

---

### v6: DDR优化 (2026-04-24)

**问题**: BRAM占用988 (137%) ❌ 超限

**优化思路**:
- 移除tmpImgp和tmpImg_shifted数组
- 实现DDR-based FFTshift

**实施结果**:
- BRAM: 988 (137%) ❌ 未改善
- 原因: 优化未生效，可能是代码结构问题

**关键发现**:
- 需要重新审视代码结构
- 可能存在其他BRAM消耗源

---

### v7: DDR-based FFTshift (2026-04-25)

**问题**: BRAM占用928 (128%) ❌ 超限

**优化思路**:
- 完全移除BRAM数组
- FFTshift直接在DDR上进行
- 使用streaming方式处理数据

**实施结果**:
- BRAM: 928 (128%) ❌ 仍超限
- 节省: 60 BRAM (从988降至928)

**关键发现**:
- BRAM消耗主要来自2个FFT实例 (608 BRAM)
- 需要降低并行度

---

### v8: 降低并行度 (2026-04-25) ✅

**问题**: BRAM占用928 (128%) ❌ 超限

**优化思路**:
- UNROLL factor=2 → 1 (单FFT实例)
- 移除第二个FFT实例
- 顺序处理所有kernel

**实施结果**:
- BRAM: 320 (44%) ✅ 达标！
- 节省: 608 BRAM (从928降至320)

**关键优化技术**:
1. **单FFT实例**: 移除并行FFT实例，节省304 BRAM
2. **DDR-based FFTshift**: 移除BRAM数组，节省60 BRAM
3. **Stream优化**: Stream depth从16384降至4096
4. **顺序处理**: UNROLL factor=1，牺牲吞吐量换取资源

---

## 📈 性能对比

### 资源利用率对比

| 资源类型 | v7 | v8 | 节省 | 节省率 | 目标 | 状态 |
|---------|-----|-----|------|--------|------|------|
| BRAM_18K | 928 | 320 | 608 | 65.5% | <540 | ✅ PASS |
| BRAM % | 128% | 44% | 84% | - | <75% | ✅ PASS |
| DSP | 93 | 33 | 60 | 64.5% | <1368 | ✅ PASS |
| FF | 78,946 | 30,504 | 48,442 | 61.4% | <325,440 | ✅ PASS |
| LUT | 84,012 | 31,935 | 52,077 | 62.0% | <162,720 | ✅ PASS |

### 功能验证对比

| 验证项 | v7 | v8 | 状态 |
|--------|-----|-----|------|
| C Simulation | PASS | PASS | ✅ |
| RMSE | 8.32e-07 | 8.32e-07 | ✅ |
| Max Error | 8.40e-06 | 8.40e-06 | ✅ |
| Errors | 0/16384 | 0/16384 | ✅ |

### 性能指标对比

| 指标 | v7 | v8 | 变化 |
|------|-----|-----|------|
| Latency | ~3.9M cycles | ~7.7M cycles | +97% |
| Throughput | ~0.72 kernels/ms | ~0.36 kernels/ms | -50% |
| BRAM | 928 (128%) | 320 (44%) | -65.5% |

---

## 🎯 优化策略分析

### 方案对比

| 方案 | BRAM节省 | 吞吐量影响 | 实施难度 | 推荐度 |
|------|---------|-----------|---------|--------|
| Plan E: DDR-based FFTshift | 60 BRAM | 无影响 | 低 | ⭐⭐⭐ |
| **Plan F: 降低并行度** | **608 BRAM** | **-50%** | **低** | **⭐⭐⭐⭐⭐** |
| Plan G: 使用LUTRAM | 未知 | 无影响 | 中 | ⭐⭐⭐ |
| Plan H: 降低FFT精度 | ~150 BRAM | 无影响 | 高 | ⭐⭐ |

### 最终选择: Plan F

**选择理由**:
1. ✅ BRAM节省最大 (608 BRAM)
2. ✅ 实施简单 (修改UNROLL factor)
3. ✅ 功能正确性保证 (RMSE不变)
4. ⚠️ 吞吐量降低50% (可接受的权衡)

**适用场景**:
- 资源受限的FPGA器件
- 对吞吐量要求不高的应用
- 需要快速部署的场景

---

## 🔧 技术细节

### v8代码结构

```cpp
// Stream channels for single instance
hls::stream<cmpx_2048_t> embed_stream("embed_stream");
hls::stream<cmpx_2048_t> fft_out_stream("fft_out_stream");

#pragma HLS STREAM variable=embed_stream depth=4096
#pragma HLS STREAM variable=fft_out_stream depth=4096

// Process kernels sequentially (UNROLL factor=1)
kernel_loop: for (int k = 0; k < nk; k++) {
    #pragma HLS LOOP_TRIPCOUNT min=5 max=10
    
    // Sequential processing: embed → FFT → accumulate
    embed_kernel_to_stream(..., embed_stream, ...);
    fft_2d_stream_wrapper(embed_stream, fft_out_stream, true);
    accumulate_to_ddr(fft_out_stream, tmpImg_ddr, scales[k]);
}

// DDR-based FFTshift
fftshift_and_output_from_ddr(tmpImg_ddr, output, nx_actual, ny_actual);
```

### 关键优化点

1. **单FFT实例**:
   - 移除embed_stream_1和fft_out_stream_1
   - 移除第二个FFT实例调用
   - 节省304 BRAM

2. **DDR-based FFTshift**:
   - 移除tmpImgp和tmpImg_shifted数组
   - 直接在DDR上进行FFTshift
   - 节省60 BRAM

3. **Stream优化**:
   - Stream depth: 16384 → 4096
   - 减少FIFO缓冲区大小
   - 节省少量BRAM

---

## 📝 经验总结

### 成功经验

1. **渐进式优化**:
   - 从v5到v8，逐步识别问题
   - 每次优化都有明确目标
   - 验证每一步的效果

2. **资源分析**:
   - 使用HLS报告精确定位BRAM消耗源
   - 识别主要消耗源 (FFT实例)
   - 针对性优化

3. **权衡取舍**:
   - 接受吞吐量降低换取资源节省
   - 明确优化目标和优先级
   - 选择最适合当前场景的方案

### 遇到的挑战

1. **v5/v6优化无效**:
   - 问题: 移动tmpImg_final到DDR后BRAM未降低
   - 原因: 未识别主要消耗源 (FFT实例)
   - 解决: 深入分析HLS报告

2. **v7仍超限**:
   - 问题: BRAM 928 (128%) 仍超限
   - 原因: 2个FFT实例并行
   - 解决: 降低并行度 (Plan F)

### 最佳实践

1. **资源优化优先级**:
   - 先识别主要消耗源
   - 针对性优化
   - 避免盲目优化

2. **验证驱动优化**:
   - 每次优化后立即验证
   - 确保功能正确性
   - 记录性能指标

3. **文档记录**:
   - 详细记录优化过程
   - 分析成功和失败原因
   - 为后续优化提供参考

---

## 🚀 后续工作

### Phase 4: 板级验证

1. **Co-Simulation**:
   - 验证RTL功能正确性
   - 检查时序收敛

2. **硬件测试**:
   - JTAG-to-AXI测试
   - DDR数据传输验证
   - 性能测试

### 性能优化 (可选)

如果需要提升吞吐量，可考虑:

1. **Plan G: 使用LUTRAM**:
   - 当前LUT占用率19%，有充足空间
   - 可尝试将部分BRAM替换为LUTRAM
   - 预期可恢复部分并行度

2. **Plan H: 降低FFT精度**:
   - ap_fixed<24,1> → ap_fixed<18,1>
   - 需验证精度是否满足要求
   - 预期可节省~150 BRAM

3. **混合方案**:
   - 结合Plan G和Plan H
   - 在保证精度的前提下提升并行度
   - 目标: BRAM <75%, 吞吐量提升2倍

---

## 📊 最终成果

### 优化目标达成

| 目标 | 初始值 | 最终值 | 状态 |
|------|--------|--------|------|
| BRAM占用率 | 128% | 44% | ✅ 达标 |
| 功能正确性 | PASS | PASS | ✅ 保持 |
| RMSE | 8.32e-07 | 8.32e-07 | ✅ 保持 |

### 关键指标

- ✅ BRAM占用率: 44% (目标<75%)
- ✅ 功能验证: RMSE=8.32e-07
- ✅ 资源节省: 608 BRAM (65.5%)
- ⚠️ 吞吐量: 降低50% (可接受的权衡)

### 文件清单

| 文件 | 路径 | 用途 |
|------|------|------|
| socs_2048_stream_refactored_v8.cpp | src/ | v8源代码 |
| tb_socs_1024_stream_refactored_v8.cpp | tb/ | v8测试平台 |
| hls_config_socs_stream_refactored_v8.cfg | script/config/ | v8配置文件 |
| Phase3_Optimization_Report.md | doc/ | 本报告 |

---

**结论**: Phase 3优化成功完成，BRAM占用率从128%降至44%，达到目标要求。v8版本已通过C仿真和C综合验证，可进入Phase 4板级验证阶段。
