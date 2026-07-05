# PCIe XDMA 板级验证指南

本目录用于通过 **Xilinx XDMA PCIe** 方式运行 SOCS V18 HLS IP 板级验证。测试数据入口仍然是 `input/config/*.json`：脚本根据 JSON 使用 `validation/golden/run_verification.py` 生成或定位 golden BIN 数据，然后通过 PCIe 写入 DDR、配置 HLS 寄存器、启动 IP、回读输出并与 `tmpImgp_full_128.bin` 对比。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| Golden 数据入口 | 已接入 `input/config/*.json` |
| XDMA DDR 写入/回读 | 已实现 |
| HLS AXI-Lite 配置 | 已实现，匹配 V18 寄存器映射 |
| 输出对比 | 已实现，默认对比 128x128 float 输出 |
| 硬件实测 | 需要在带 `/dev/xdma0_*` 设备的板卡主机上执行 |

## 目录结构

```text
validation/board/pcie/
├── README.md
├── config/
│   └── pcie_validation_config.json
├── output/
├── run.sh
└── scripts/python/
    ├── pcie_config.py
    ├── run_pcie_validation.py
    └── xdma.py
```

## 地址与寄存器

默认配置来自 `config/pcie_validation_config.json`，与 `validation/board/Adress_Assigned.csv` 和 V18 HLS driver header 保持一致。

| 区域 | 地址 |
| --- | --- |
| `control` | `0x00000000` |
| `control_r` | `0x00010000` |
| `mskf_r` | `0x40000000` |
| `mskf_i` | `0x40400000` |
| `scales` | `0x40800000` |
| `krn_r` | `0x40880000` |
| `krn_i` | `0x40900000` |
| `tmpImg_ddr` | `0x40980000` |
| `output` | `0x40990000` |

`control` 标量寄存器：`nk=0x10`、`nx_actual=0x18`、`ny_actual=0x20`、`Lx=0x28`、`Ly=0x30`。

`control_r` 指针寄存器：`mskf_r=0x10`、`mskf_i=0x1c`、`krn_r=0x28`、`krn_i=0x34`、`scales=0x40`、`tmpImg_ddr=0x4c`、`output=0x58`。

## 使用方法

先在无硬件访问模式检查 JSON、golden 数据和 DDR 布局：

```bash
validation/board/pcie/run.sh \
  --config input/config/golden_1024.json \
  --dry-run
```

如果 golden 数据尚未生成，可以让脚本先调用 golden 生成流程：

```bash
validation/board/pcie/run.sh \
  --config input/config/golden_1024.json \
  --generate-golden \
  --dry-run
```

在板卡主机上运行完整 PCIe 验证：

```bash
sudo validation/board/pcie/run.sh \
  --config input/config/golden_1024.json
```

输出默认保存到：

```text
validation/board/pcie/output/aerial_image_output.bin
```

## 其他配置

脚本会自动推断常见 golden 输出目录：

| JSON | 默认 golden 输出目录 |
| --- | --- |
| `input/config/golden_1024.json` | `output/verification` |
| `input/config/config_1024x1024.json` | `output/Different_resolution_tests/1024x1024` |
| `input/config/Different_mask_tests/config_T1.json` | `output/Different_mask_tests/T1` |

也可以手动指定：

```bash
validation/board/pcie/run.sh \
  --config input/config/Different_mask_tests/config_T1.json \
  --golden-output output/Different_mask_tests/T1
```

如果 XDMA 设备名或地址不同，修改 `config/pcie_validation_config.json`，或通过 `--pcie-config` 指向新的配置文件。

## 兼容性说明

当前 V18 bitstream/HLS 头文件定义 `MAX_KERNEL_SIZE=17`，对应 `Nx=Ny<=8`。脚本会检查 `fft_meta.txt` 和 kernel 尺寸；例如 2048/4096 分辨率配置如果生成了超过 17x17 的 kernel，会在上板前直接报错，避免写入后得到不可解释的结果。

PCIe 完整验证还要求 HLS AXI-Lite 控制寄存器对主机可达。可用方式有两种：

- XDMA `M_AXI` 能访问 `0x00000000` 和 `0x00010000` 的 HLS `control/control_r` 地址段。
- 或者 bitstream 暴露 XDMA user BAR，并将 HLS AXI-Lite 映射到该 BAR。

如果 `dmesg` 显示 `identify_bars: 1 BARs: config 0, user -1, bypass -1`，且访问 `0x00000000/0x00010000` 超时，则当前 PCIe 通路只能完成 DDR 数据注入，无法通过 PCIe 启动 HLS IP。
