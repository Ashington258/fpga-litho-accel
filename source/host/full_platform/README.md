# Host 全平台 PCIe 验证流程

本目录提供 SOCS V18 的端到端 Host 侧全平台验证。它不再放在 `validation/board/` 下，因为该流程已经不只是“板级 I/O 验证”，还包含 Host 后处理 FI、系统级指标汇总和可视化，更适合作为 Host 运行流程放在 `source/host/full_platform/`。

1. 根据 `input/config/*.json` 选择 Golden 数据集（可切换不同 mask、source、kernel count 等参数）。
2. 通过 PCIe XDMA 将 `mskf_r/i`、`scales`、`krn_r/i` 写入 FPGA DDR。
3. 配置 HLS AXI-Lite 参数，启动 FPGA 计算并回读 `tmpImgp_full_128`。
4. 在主机侧执行 FI（Fourier Interpolation）逆向计算空中像。
5. 记录每个步骤的时间、数据路径和吞吐率，并与 Golden 模型对比。

## 快速开始

只检查 JSON、Golden 数据和地址布局：

```bash
source/host/full_platform/run.sh \
  --config input/config/golden_1024.json \
  --dry-run
```

复用已有 FPGA 输出，仅验证主机 FI 和 Golden 对比：

```bash
source/host/full_platform/run.sh \
  --config input/config/golden_1024.json \
  --reuse-fpga-output validation/board/pcie/output/aerial_image_output.bin
```

完整上板验证：

```bash
source/host/full_platform/run.sh \
  --config input/config/golden_1024.json
```

如果需要先生成或刷新 Golden 数据：

```bash
source/host/full_platform/run.sh \
  --config input/config/golden_1024.json \
  --generate-golden
```

切换不同 mask/source/config 时，只需更改 `--config`，例如：

```bash
source/host/full_platform/run.sh \
  --config input/config/Different_mask_tests/config_T1.json \
  --generate-golden
```

> 注意：当前 V18 bitstream 限制 `MAX_KERNEL_SIZE=17`，即 `Nx/Ny<=8`。脚本会在上板前检查 kernel 尺寸和 HLS depth，避免无效配置。

## 输出文件

默认输出目录：`source/host/full_platform/output/`

| 文件 | 说明 |
| --- | --- |
| `fpga_tmpimgp_full_128.bin` | FPGA 回读的 128×128 tmpImgp 输出 |
| `fpga_aerial_fi.bin` | 主机 FI 后的最终空中像 |
| `timing.csv` | 每个步骤耗时、地址、字节数、吞吐率 |
| `metrics.csv` | tmpImgp 与空中像对比指标 |
| `summary.json` | 机器可读完整摘要 |
| `full_platform_report.md` | 人类可读验证报告 |
| `tmpimgp_comparison.png` | matplotlib 可视化：FPGA tmpImgp、Golden tmpImgp、差异图 |
| `aerial_comparison.png` | matplotlib 可视化：Host FI 空中像、Golden、差异图 |

可视化 PNG 不计入 `timing.csv`。如需跳过可视化：

```bash
source/host/full_platform/run.sh --config input/config/golden_1024.json --no-visualize
```

## 关键对比项

- `tmpImgp_vs_golden`：FPGA 128×128 输出 vs `tmpImgp_full_128.bin`
- `host_FI_vs_golden_SOCS`：FPGA 输出经主机 FI 后 vs `aerial_image_socs_kernel.bin`
- `host_FI_vs_TCC_direct`：FPGA+FI 最终空中像 vs `aerial_image_tcc_direct.bin`（含 SOCS 截断误差，仅作参考）

## 实测示例（golden_1024）

完整验证通过，关键指标：

| 对比项 | RMSE | Max abs | Max rel |
| --- | ---: | ---: | ---: |
| `tmpImgp_vs_golden` | `2.9303560514e-08` | `3.5762786865e-07` | `2.6540381633e-06` |
| `host_FI_vs_golden_SOCS` | `2.9528027459e-08` | `4.1723251343e-07` | `8.6624720582e-06` |
| `host_FI_vs_TCC_direct` | `5.4737755323e-03` | `1.0465636849e-02` | `9.4251801039e-01` |

关键耗时示例：

- `mskf_r` H2C：4 MiB，约 701 MiB/s
- `mskf_i` H2C：4 MiB，约 505 MiB/s
- FPGA compute：约 20.47 ms
- C2H 回读 tmpImgp：64 KiB，约 0.17 ms
- Host FI：128×128 → 1024×1024，约 33.50 ms

## XDMA 环境要求

完整流程依赖 Xilinx XDMA Reference Driver 字符设备：

- `/dev/xdma0_h2c_0`
- `/dev/xdma0_c2h_0`
- `/dev/xdma0_user` 或 XDMA `M_AXI` 可访问 HLS AXI-Lite 控制段

若系统自带 `drivers/dma/xilinx/xdma.ko` 不生成 `/dev/xdma*`，需使用 Xilinx XDMA Reference Driver，并为当前运行内核重新编译。
