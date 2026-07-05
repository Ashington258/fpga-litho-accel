# 板级验证目录 (Board Validation)

本目录用于 FPGA-Litho SOCS HLS IP 的硬件板级验证，包含两种数据加载方式。

> **所属模块**：`validation/board/` — 属于 [验证与测试体系](../README.md) 的板级硬件验证部分。

## 目录结构

```
validation/board/
├── README.md                   # 本说明文件
├── common/                     # 公共工具脚本
│   └── axi_memory_test.tcl     # AXI 内存读写测试脚本
├── scripts/                    # 辅助脚本
│   └── generate_jtag_tcl_from_csv.py # 从AddressSegments.csv生成JTAG TCL
├── jtag/                       # JTAG-to-AXI 验证方式
│   ├── README.md               # JTAG 验证使用指南 ⭐
│   ├── socs_hls_validation.tcl # SOCS HLS IP 验证主脚本
│   ├── bin_to_tcl_converter.py # BIN→TCL 数据转换工具
│   └── data/                   # 验证数据文件
│       ├── socs_data.tcl       # 小型数据（scales/kernels/tmpImgp）
│       ├── socs_data_batch.tcl # 大型数据（mskf_r/i，分批）
│       └── data_usage.tcl      # 数据使用说明
└── pcie/                       # PCIe XDMA 验证方式
    ├── README.md               # PCIe 验证使用指南
    ├── config/pcie_validation_config.json
    └── scripts/python/         # XDMA 访问、数据加载、输出对比脚本
```

## 验证方式对比

| 方式            | 适用场景             | 数据传输速度        | 开发状态 |
| --------------- | -------------------- | ------------------- | -------- |
| **JTAG-to-AXI** | 调试阶段、小规模验证 | 较慢（~30分钟/1MB） | ✅ 已完成 |
| **PCIe DMA**    | 生产验证、大规模数据 | 快速（~秒级）       | ✅ 脚本完成，完整上板需 AXI-Lite 可达 |

## 快速开始

### JTAG 验证（当前可用）

1. 在 Vivado Hardware Manager 中连接 FPGA
2. 参考 `jtag/README.md` 执行验证流程

### PCIe 验证

```bash
# 无硬件检查 Golden 数据和地址布局
validation/board/pcie/run.sh --config input/config/golden_1024.json --dry-run

# 板卡主机上只验证 DMA/DDR 链路
sudo validation/board/pcie/run.sh --config input/config/golden_1024.json --dma-only

# 板卡主机上完整验证：写 DDR → 配置 HLS → 启动 → 回读 → 对比 Golden
sudo validation/board/pcie/run.sh --config input/config/golden_1024.json
```

完整 PCIe 验证要求 XDMA `M_AXI` 或 user BAR 能访问 HLS `s_axi_control` (`0x00000000`) 和 `s_axi_control_r` (`0x00010000`)。

## 相关文档

- [HLS 验证流程](../source/SOCS_HLS/doc/HLS验证流程完整报告.md)
- [IP 端口连线指南](../source/SOCS_HLS/doc/IP端口连线指南.md)
- [板级验证指南](../source/SOCS_HLS/doc/板级验证指南.md)