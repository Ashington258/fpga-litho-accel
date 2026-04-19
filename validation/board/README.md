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
└── pcie/                       # PCIe DMA 验证方式（待开发）
    └── README.md               # PCIe 验证使用指南
```

## 验证方式对比

| 方式            | 适用场景             | 数据传输速度        | 开发状态 |
| --------------- | -------------------- | ------------------- | -------- |
| **JTAG-to-AXI** | 调试阶段、小规模验证 | 较慢（~30分钟/1MB） | ✅ 已完成 |
| **PCIe DMA**    | 生产验证、大规模数据 | 快速（~秒级）       | 🔲 待开发 |

## 快速开始

### JTAG 验证（当前可用）

1. 在 Vivado Hardware Manager 中连接 FPGA
2. 参考 `jtag/README.md` 执行验证流程

### PCIe 验证（待开发）

- 预计通过 Xilinx DMA IP 实现高速数据传输
- 需要开发 Host 驱动程序和 DMA 控制逻辑

## 相关文档

- [HLS 验证流程](../source/SOCS_HLS/doc/HLS验证流程完整报告.md)
- [IP 端口连线指南](../source/SOCS_HLS/doc/IP端口连线指南.md)
- [板级验证指南](../source/SOCS_HLS/doc/板级验证指南.md)