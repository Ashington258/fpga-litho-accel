# 工作要点二数据补测执行报告

**执行日期**：2026-08-03

**物理机**：Intel Core i3-4170，Linux 6.8，Xilinx PCIe 设备 `10ee:9038`
**FPGA 通路**：XDMA H2C/C2H + AXI-Lite + xcku5p 目标设计

## 已完成的实测

### D1 CPU kernel-only

- 已归档 double/float、单线程/双线程四种 FFTW 基准，每种 20 次预热、1000 次正式运行。
- 单精度单线程均值为 `0.627230542 ms`，双精度单线程均值为 `1.103415676 ms`。
- 计时范围仅含频域嵌入、10 次 128x128 IFFT、幅度平方、权重累加和 FFTshift。
- 2026-08-03 额外完成一次双精度单线程 1000 次复测，原始数据单独保存在 `D1_cpu_kernel/raw/`。

### D2/D3 FPGA 与 end-to-end

- 当前板卡 PCIe、DDR、AXI-Lite、`ap_start/ap_done`、C2H 和 Golden 对比均通过。
- D2 已有 20 次预热、1000 次无 sleep busy-poll，host-observed kernel 中位数 `13.600688 ms`。
- D3 CPU end-to-end 本次复测 1000 次，均值 `15.7830791 ms`；范围包含 plan 创建、10 核 SOCS、128 到 1024 FI 和输出形成。
- D3 FPGA resident-input 与 retransmit-input 原始数据、stage breakdown 和 batch size 1/10/100/1000 已归档。
- 当前 bitstream 未映射硬件周期计数器，因此 D2 不能声称 `ap_start` 到 `ap_done` 的硬件周期实测。

### D4 功耗

- CPU package 已使用 turbostat 完成 3 组独立采样：idle 均值 `4.3836 W`，kernel load 均值 `22.9005 W`，动态差值 `18.5169 W`。
- 单精度 kernel-only 动态能耗为 `0.0116143 J/call`，绝对 package 能耗为 `0.0143639 J/call`。
- FPGA routed power report 为 `7.523 W` total、`6.890 W` dynamic，但无 SAIF/VCD 且 confidence 为 Low，只能写成估算。
- 本机未暴露 FPGA rail/board sensor，未生成伪造的 `fpga_power_trace.csv`，也未计算跨平台能效倍数。

### E1 多掩模

- T1-T10 共 10 个 ICCAD 2013 独立掩模已完成软件 Golden、FPGA 板测、Host FI 和 TCC/SOCS 对比。
- 统一配置为 1024x1024 V18 window、Annular 0.6/0.9、NA=0.8、193 nm、10 核。
- 实现 RMSE 均值 `2.6475e-08`，最坏值 `3.5810e-08`（T6）；最坏 SSIM `0.9999999797`。
- 阈值 0.225 下二值一致率为 100%。模型截断误差与硬件实现误差已分列。
- 当前只有 10 个独立样本，未达到 TODO 建议的最低 30 个；CD/EPE 尚无固定测量定义。

### E2/E3 核数与有效核尺寸

- 核数 1/5/10 已各完成 1000 次板上 host-observed 测试及硬件误差统计。
- 当前 V18 编译上限为 `MAX_NK=10`，20/50 核需要新 bitstream。
- `Nx=2/4/8`（5x5、9x9、17x17 核）已上板，三组均通过；当前 `MAX_KERNEL_SIZE=17`，不支持 `Nx>8`。

### E4 多光学条件

- Annular baseline、Dipole、CrossQuadrupole 和 NA=0.6 四组已完成软件与板测，全部通过。
- 实现 RMSE 范围为 `1.9541e-08` 到 `4.0446e-08`。
- 10 nm 和 100 nm defocus 的 10 核 SOCS 对 full TCC 最大相对差异分别约 29.5% 和 43.5%，作为模型截断负结果保留，未归因于 FPGA。
- Quasar 尚未由 CPU source generator 实现，不能声称已支持。

### E5 分辨率

- 256、512、1024 三种分辨率已生成新 Golden 并上板，对应 `Nx=2/4/8`。
- 三组实现 RMSE 分别为 `9.9870e-09`、`1.3602e-08`、`2.9304e-08`，均通过。
- H2C、AXI-Lite、host-observed FPGA、C2H 和精度已写入 `resolution_stage_breakdown.csv`。
- 该表中的 FPGA 时间受 10 ms host polling 量化，不是硬件周期时间。

## 尚未完成且不能替代的数据

- D2：硬件 cycle counter、板上实测时钟与 1000 次周期原始数据。
- D4：FPGA board/rail 功耗时间序列和跨平台同边界能效。
- E1：至少再增加 20 个独立掩模；定义并计算 CD/EPE。
- E2/E3：新 bitstream 支持 20/50 核和 `Nx=12/16/24`。
- E4：实现并验证 Quasar；defocus 场景需要更多 SOCS 核或新的截断策略。
- E6：构建同版本、可独立开关的受控消融 baseline。
- E7：归档最终 xcku5p post-synthesis/post-implementation utilization、timing、power、bitstream 和 commit manifest。

## 论文取数入口

- 全局状态：`experiments/data/experiment_index.json`
- 文件哈希：`experiments/data/experiment_files.csv`
- 论文数字映射：`experiments/data/F1_paper_evidence/paper_evidence_manifest.csv`
- 目录说明：`experiments/README.md`

Git 提交包含全部 CSV/JSON 指标、原始计时与功耗 trace、配置、报告、哈希清单，以及完整 Golden/FPGA `.bin`、可视化 `.png`、运行 `.log` 和对应测试输入 BIN。文件大小、SHA-256 和 Git 归档状态记录在 `experiment_files.csv`。

任何正文数字应继续标记为 `measured`、`on-board host wall-clock` 或 `estimated`，不得把本报告列出的 blocker 用估算值补齐。