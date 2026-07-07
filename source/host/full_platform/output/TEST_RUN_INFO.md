# 全平台验证测试运行信息

## 测试环境

**测试时间**: 2026-07-07 08:18+ UTC

### PCIe 链路配置

| 项目 | 值 |
|------|-----|
| **当前链路宽度** | **x8 (ok)** ✅ |
| **当前链路速度** | 8.0 GT/s (Gen3) |
| **链路协商状态** | Speed 8GT/s (ok), Width x8 (ok) |
| **Lane 错误** | 0 (LaneErrStat: 0) |
| **链路能力** | x8 Gen3 |

### 故障诊断历史

1. **插拔前状态** (2026-07-07 08:00)
   - `LnkSta`: Speed 8GT/s, Width x2 (downgraded)
   - `LaneErrStat`: lane 1-7 错误
   - **根因**: PCIe 插槽接触不良

2. **插拔后状态** (2026-07-07 08:15)
   - `LnkSta`: Speed 8GT/s, Width x8 (ok) ✅
   - `LaneErrStat`: 0 ✅
   - **状态**: 完全恢复

### 硬件配置

- **主机 CPU**: Intel Xeon E3-1200 v3/4th Gen (Haswell)
- **主板**: ASUS Z97
- **PCIe Root Port**: 00:01.0 (Intel 8086:0c01)
- **PCIe Endpoint**: 01:00.0 (Xilinx 10ee:9038)
- **FPGA 器件**: xcku5p-ffvb676-2-e (Kintex UltraScale+)
- **XDMA 驱动**: /root/xdma_test/dma_ip_drivers/XDMA/linux-kernel

### 吞吐量测试结果

**PCIe DMA 写速度 (H2C)**:
- mskf_r (4 MB): **757.91 MiB/s**
- mskf_i (4 MB): **843.49 MiB/s**
- 平均: ~800 MiB/s (x8 Gen3 理论: 8.0 GT/s / 8 bits per symbol × 127/130 encoding = ~954 MiB/s)

### 验证精度结果

所有指标均 **PASS**（无因链路宽度变化而改变）:

| 对比目标 | RMSE | Max Abs | Status |
|---------|------|---------|--------|
| tmpImgp_vs_golden | 2.93e-08 | 3.58e-07 | ✅ PASS |
| host_FI_vs_golden_SOCS | 2.95e-08 | 4.17e-07 | ✅ PASS |
| host_FI_vs_TCC_direct | 5.47e-03 | 1.05e-02 | ✅ PASS |

### 关键发现

1. **精度独立性**: 功能精度不受 PCIe 链路宽度影响
   - x2 链路和 x8 链路测试结果相同
   - 工作负载主要受 FPGA 计算能力和内存访问模式限制

2. **物理接触重要**: PCIe 插槽接触不良可导致链路降级
   - 重新插拔是最简单的恢复方法
   - 建议定期清洁插槽和卡槽

3. **吞吐量充足**: 当前 DMA 速度充分满足需求
   - 800 MiB/s 可支持多路并行数据传输
   - 无需进一步优化 PCIe 配置

### 后续建议

- ✅ PCIe 链路已确认正常（x8 Gen3）
- ✅ FPGA 计算验证通过
- ✅ 全平台数据流验证通过
- **可继续**: 进行大规模性能测试或部署

---

**测试结果文件**:
- `full_platform_report.md` - 详细验证报告
- `timing.csv` - 逐步耗时统计
- `metrics.csv` - 精度对比指标
