# SOCS Optimized HLS IP 板级验证快速参考

## 🚀 快速开始

### 1. 生成验证脚本
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized
python generate_socs_optimized_tcl.py
```

### 2. 测试TCL语法
```bash
python test_tcl_syntax.py
```

### 3. 运行验证 (Vivado Tcl Console)
```tcl
# 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
reset_hw_axi [get_hw_axis hw_axi_1]

# 运行验证
source e:/fpga-litho-accel/source/SOCS_HLS/board_validation/socs_optimized/run_socs_optimized_validation.tcl
```

### 4. 验证结果
```bash
python verify_socs_optimized_results.py
```

## 📊 关键参数

### HLS IP配置
- **器件**: xcku3p-ffvb676-2-e
- **时钟**: 5ns (200MHz)
- **FFT**: xfft_v9_1, 32-point, float, unscaled

### 资源使用
- **DSP**: 16 (99.8% reduction)
- **BRAM**: 39 (5%)
- **FF**: 25,513 (7%)
- **LUT**: 28,378 (17%)

### 性能指标
- **频率**: 273.97MHz (超过目标37%)
- **流水线**: 所有循环II=1

## 🗺️ DDR地址映射

```
0x40000000: mskf_r (512×512 float32)
0x42000000: mskf_i (512×512 float32)
0x44000000: scales (4 float32)
0x44400000: krn_r (4×9×9 float32)
0x44800000: krn_i (4×9×9 float32)
0x44840000: output (17×17 float32)
```

## 🔧 HLS IP控制

```tcl
# 启动IP
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
    -type WRITE -address 0x00000000 -data {0x00000001} -len 1
run_hw_axi [get_hw_axi_txns start_txn]

# 轮询完成
create_hw_axi_txn poll_txn [get_hw_axis hw_axi_1] \
    -type READ -address 0x00000000 -len 1
run_hw_axi [get_hw_axi_txns poll_txn]
# 检查bit 1 (0x00000002) 表示ap_done
```

## 📁 文件说明

| 文件                                | 说明         |
| ----------------------------------- | ------------ |
| `run_socs_optimized_validation.tcl` | 主验证脚本   |
| `verify_socs_optimized_results.py`  | 结果验证脚本 |
| `address_map.json`                  | DDR地址配置  |
| `generate_socs_optimized_tcl.py`    | TCL生成器    |
| `test_tcl_syntax.py`                | 语法测试     |

## 🎯 验证目标

### 主要目标
- ✅ HLS综合成功
- ✅ 资源使用达标
- ⏳ 板级验证通过
- ⏳ RMSE < 1e-3

### 预期结果
- **RMSE**: 1e-5 to 1e-7
- **相关系数**: > 0.999
- **处理时间**: 5-10μs

## ⚠️ 故障排除

### 常见问题
1. **TCL语法错误**: 运行 `python test_tcl_syntax.py`
2. **硬件连接失败**: 检查JTAG连接和服务器地址
3. **验证失败**: 检查DDR地址映射和输入数据

### 调试命令
```tcl
# 检查输入数据
create_hw_axi_txn read_debug [get_hw_axis hw_axi_1] \
    -type READ -address 0x40000000 -len 10
run_hw_axi [get_hw_axi_txns read_debug]
report_hw_axi_txn read_debug

# 检查IP状态
create_hw_axi_txn read_ctrl [get_hw_axis hw_axi_1] \
    -type READ -address 0x00000000 -len 1
run_hw_axi [get_hw_axi_txns read_ctrl]
report_hw_axi_txn read_ctrl
```

## 📞 支持

### 文档
- `BOARD_VALIDATION_GUIDE.md` - 详细指南
- `VALIDATION_STATUS.md` - 状态总结
- `EXECUTION_SUMMARY.md` - 执行总结
- `FINAL_REPORT.md` - 最终报告

### 数据
- Golden数据: `e:\fpga-litho-accel\output\verification\`
- Kernel数据: `e:\fpga-litho-accel\output\verification\kernels\`

---

**最后更新**: 2026-04-23  
**状态**: ✅ 准备完成，等待硬件执行
