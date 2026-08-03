# 论文实验数据索引

本目录按 `SCI学术严谨性整改TODO.md` 的工作要点二组织数据。正文取数应优先读取 `data/` 下的汇总 CSV/JSON；`runs/` 保留单次板测的时间线、指标、日志和二进制输出；`config/` 保留生成每组结果的固定配置。

## 快速入口

| 任务 | 状态 | 论文用汇总 | 原始数据或说明 |
| --- | --- | --- | --- |
| D1 CPU kernel-only | 完成 | `data/D1_cpu_kernel/cpu_kernel_latency_summary.json` | `data/D1_cpu_kernel/cpu_kernel_latency_raw.csv`, `raw/` |
| D2 FPGA kernel-only | 部分完成 | `data/D2_fpga_kernel/fpga_kernel_latency_summary.json` | 已有 1000 次 host busy-poll；硬件周期计数器未映射 |
| D3 end-to-end | 完成 | `data/D3_end_to_end/end_to_end_latency_summary.json` | CPU/FPGA 原始 CSV、阶段分解和 batch 结果 |
| D4 功耗能效 | 部分完成 | `data/D4_power_energy/energy_summary.json` | CPU package 已实测；FPGA 仅有低置信度 routed estimate |
| E1 多掩模 | 10/30 | `data/E1_multi_mask/multi_mask_accuracy.csv` | T1-T10 独立 Golden、板测、Host FI 位于 `runs/E1_multi_mask/` |
| E2 核数扩展 | 1/5/10 | `data/E2_kernel_count/kernel_count_scaling.csv` | V18 `MAX_NK=10`，20/50 核需新 bitstream |
| E3 有效核尺寸 | 2/4/8 | `data/E3_kernel_size/kernel_size_runtime_config.csv` | V18 `MAX_KERNEL_SIZE=17`，即 `Nx<=8` |
| E4 光学配置 | 4 组通过 | `data/E4_optical_config/optical_configuration_accuracy.csv` | 含 Annular、Dipole、CrossQuadrupole、NA 变化；defocus 负结果单列 |
| E5 分辨率 | 完成 | `data/E5_resolution/resolution_stage_breakdown.csv` | 256/512/1024 均已上板；对应 `Nx=2/4/8` |
| E6 消融 | 阻塞 | `data/E6_ablation/ABLATION_BASELINE_BLOCKER.json` | 当前仓库无同版本可开关 baseline 与报告集 |
| E7 最终实现 | 部分完成 | `data/E7_implementation/implementation_manifest.json` | 缺最终 xcku5p utilization/timing/bitstream manifest |

完整机器可读状态见 `data/experiment_index.json`，所有归档文件的大小和 SHA-256 见 `data/experiment_files.csv`。论文数字到证据的映射见 `data/F1_paper_evidence/paper_evidence_manifest.csv`。

## Git 归档范围

仓库现有 `.gitignore` 全局排除 `*.bin`、`*.png` 和 `*.log` 生成物，且当前仓库未配置 Git LFS。因此本次 Git 提交包含：

- 全部原始延迟 CSV、CPU 功耗 trace、阶段时间、精度指标和统计 JSON；
- 所有实验配置、采集脚本、论文映射、状态 blocker 和 SHA-256 manifest；
- 每次板测的 `timing.csv`、`metrics.csv`、`summary.json` 和 Markdown 报告；
- Golden metadata 与 kernel metadata。

约 370 MB 的可重建中间数组和可视化文件继续保留在实验物理机，不直接写入普通 Git 历史。`experiment_files.csv` 的 `git_archived` 列区分远端可获取文件与本地生成文件；被排除文件仍保留大小和 SHA-256，必要时可通过对应配置和采集器重建。

## 口径约束

- `measured`：本机 CPU 或物理 FPGA 板卡实际采集。
- `on-board host wall-clock`：Host 发起 `ap_start` 并轮询 `ap_done` 的时间，不等于硬件周期数。
- `estimated`：综合或 Vivado power report 推导，不得写成板上实测。
- CPU Xeon 8163 历史结果与本机 Core i3-4170 新基准属于不同主机，不能混成同一平台。
- `fpga_compute` 若来自 full-platform 单次 run，受 10 ms 轮询间隔量化；D2 的 1000 次 busy-poll 数据更适合报告 host-observed kernel latency。
- 多掩模实验使用统一的 1024x1024 V18 物理窗口，与旧 `Lx=2048` 软件批测分开报告。

## 重建索引

```bash
python experiments/organize_paper_evidence.py
python experiments/build_evidence_index.py
```

板测采集入口：

```bash
python experiments/collect_multi_mask.py --reuse-existing
python experiments/collect_optical_config.py
```

运行采集器会访问 `/dev/xdma0_*` 并启动当前 FPGA bitstream。执行前应确认板卡已编程且地址映射与 `validation/board/pcie/config/pcie_validation_config.json` 一致。