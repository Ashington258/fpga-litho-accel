# FPGA-Litho 项目进度与 SCI 论文升级计划

**最后更新**: 2026-07-07
**当前分支**: feature/2048-optimization
**项目状态**: HLS 内核与 PCIe/XDMA 板级验证已完成，当前重点转向 SCI 论文严谨化、可重复性补强和投稿材料整理

---

## 📊 项目整体进度

### ✅ 已完成阶段

#### Phase 1: HLS IP基础开发 (2026-04-19)
- ✅ C Simulation验证通过
- ✅ C Synthesis完成 (Fmax=274MHz, BRAM=56%)
- ✅ Co-Simulation验证通过
- ✅ 功能正确性验证 (RMSE=8.32e-07)

#### Phase 2: 性能优化 (2026-04-22)
- ✅ BRAM优化: 56% → 47% (-13.1%)
- ✅ Fmax提升: 274MHz → 280MHz (+2.2%)
- ✅ 精度验证: RMSE=1.25e-06 ✅

#### Phase 3: BRAM深度优化 (2026-04-25)
- ✅ BRAM优化: 47% → 44% (-6.4%)
- ✅ HLS FFT IP集成 (替代Direct DFT)
- ✅ DSP优化: 8,064 → 53 (-99.3%)
- ✅ 精度验证: RMSE=8.32e-07 ✅

#### 批量测试验证 (2026-05-07)
- ✅ 不同mask测试 (10种mask pattern)
- ✅ 不同分辨率测试 (256×256 ~ 8192×8192)
- ✅ 结果已保存至 `output/Different_mask_tests/`, `output/Different_resolution_tests/`

#### Phase 4: Host-FPGA PCIe/XDMA 板级验证 (2026-07-05)
- ✅ 目标板卡 `01:00.0 [10ee:9038]` 识别成功
- ✅ Xilinx XDMA Reference Driver v2020.2.2 加载成功
- ✅ XDMA H2C/C2H 数据搬运、DDR 写入和代表性回读验证通过
- ✅ HLS AXI-Lite `control/control_r` 寄存器配置与回读通过
- ✅ FPGA 输出 `128×128 tmpImgp` 与 golden 对比通过
- ✅ 板级输出 RMSE = `2.9303560514e-08`
- ✅ PCIe 全平台执行路径实测为 `67.95 ms`
- ⚠️ XDMA 接口按 PCIe Gen3 x8 描述；本次主机实测协商链路为 Gen3 x2，论文中需持续保持该口径

### 🔄 当前工作

#### SCI 小论文升级
- 📝 当前主文档: `doc/小论文/小论文.md`
- 📝 详细核对表: `doc/小论文/TODO.md`
- 📝 目标: 将现有工程验证稿升级为面向 TCAD / TODAES / Optics Express / JM3 等期刊的 SCI 论文
- 📝 核心主线: TCC-SOCS 在线重构的 CPU-FPGA 协同架构、固定 FFT 网格复用、多 AXI-MM/HLS 优化和 PCIe/XDMA 端到端板级验证
- 📝 当前需优先区分的实验口径:
  - FPGA kernel-only latency: `10.57 ms`
  - PCIe full-platform execution path: `67.95 ms`
  - FPGA kernel-level estimated energy efficiency: `67.4×`
  - PCIe full-platform power: 待板级实测
  - PCIe interface/target: Gen3 x8
  - Current host-negotiated link: Gen3 x2

### ⏳ 下一步计划

#### Phase 5: SCI 论文严谨化与投稿准备
- ⏳ 重写 Abstract / Introduction / Contribution list，强化 novelty 与 research gap
- ⏳ 规范 Method 公式、符号、数据布局和 HLS/XDMA 可复现参数
- ⏳ 强化 Experiments：误差分类、统计分析、资源 breakdown、功耗口径和 PCIe 瓶颈分析
- ⏳ 更新图表为 SCI 风格，统一 caption、单位、有效数字和数据来源
- ⏳ 补充 2024-2026 相关文献，明确与 GPU/FPGA/AI lithography acceleration 工作的差异
- ⏳ 转换为目标期刊 LaTeX 模板并准备 supplementary material

---

## 🎯 关键技术指标

### HLS IP核性能 (V18架构)

**目标器件**: xcku5p-ffvb676-2-e

**资源占用**:
| 资源 | 使用量 | 可用量 | 占用率 |
|------|--------|--------|--------|
| BRAM | 399 | 960 | 42% ✅ |
| DSP | 53 | 1,824 | 3% ✅ |
| FF | 31,942 | 433,920 | 7% ✅ |
| LUT | 37,098 | 216,960 | 17% ✅ |

**性能指标**:
- **Fmax**: 280 MHz (超过目标40%)
- **精度**: RMSE = 8.32e-07 ✅
- **吞吐量**: ~0.36 kernels/ms (顺序处理10个kernel)
- **Kernel-only latency**: 10.57 ms @ 250 MHz
- **PCIe full-platform path**: 67.95 ms
- **PCIe board output RMSE**: 2.93e-08
- **Host FI output RMSE**: 2.95e-08
- **Kernel-level estimated energy efficiency**: 67.4× vs. C++ SOCS baseline

### 支持的配置

**FFT架构**: 固定128×128 (支持Nx=2~24)

**测试配置**:
- golden_original: Lx=512, Nx=4
- golden_1024: Lx=1024, Nx=8 (推荐)
- config_Nx16: Lx=1536, Nx=12

---

## 📁 项目结构

```
fpga-litho-accel/
├── source/
│   ├── SOCS_HLS/          # HLS IP核开发
│   ├── TCC_HLS/           # TCC IP核开发
│   └── host/              # Host端预处理
├── validation/            # 验证脚本
│   ├── golden/            # Golden数据生成
│   ├── board/             # 板级验证脚本
│   └── batch_test_runner.py  # 批量测试
├── input/                 # 输入数据
│   ├── config/            # 配置文件
│   └── mask/              # Mask数据
├── output/                # 输出结果
│   ├── verification/      # 验证结果
│   ├── Different_mask_tests/      # 不同mask测试结果
│   └── Different_resolution_tests/ # 不同分辨率测试结果
├── doc/                   # 文档
│   └── 论文/              # 论文相关
└── reference/             # 参考实现
```

---

## 🧭 SCI 论文升级路线图

目标是把当前稿件从“扎实工程验证报告”提升为“问题清晰、创新边界明确、实验可复现、claim 有证据支撑的 SCI 期刊论文”。

### Phase A: 论文定位与主线重构

- [ ] 明确目标期刊优先级:
  - [ ] IEEE TCAD / ACM TODAES: 强调 EDA、HLS、架构、资源/性能 trade-off
  - [ ] Optics Express / JM3: 强调计算光刻、TCC-SOCS、成像精度和光学适配性
- [ ] 重写 Introduction 的逻辑链:
  - [ ] 计算光刻空中像计算是 OPC/SMO/ILT 中的高频瓶颈
  - [ ] TCC-SOCS 降低复杂度，但在线重构仍需反复执行
  - [ ] CPU/GPU 在低延迟、低功耗或确定性执行场景中存在限制
  - [ ] FPGA 适合规则频域流水线、BRAM 缓冲和低功耗部署
  - [ ] 现有工作缺少面向 TCC-SOCS 在线重构的 FPGA/HLS 架构与 PCIe/XDMA 板级验证
- [ ] 统一贡献列表:
  - [ ] CPU-FPGA 离线/在线协同划分
  - [ ] 17×17 SOCS eigenkernel 到 128×128 FFT 网格的固定嵌入架构
  - [ ] HLS FFT、BRAM 缓冲、块浮点缩放、多 AXI-MM 接口优化
  - [ ] XDMA PCIe 全平台验证
  - [ ] 精度、延迟、资源、能效和局限性系统评估

### Phase B: 方法节严谨化与可重复性

- [ ] 为核心公式编号并统一符号:
  - [ ] Hopkins / TCC 公式
  - [ ] SOCS 分解公式
  - [ ] 在线重构公式
  - [ ] RMSE / PSNR / SSIM 指标公式
- [ ] 明确三类误差:
  - [ ] FPGA/HLS 数值误差 vs. float SOCS
  - [ ] SOCS 截断误差 vs. full TCC
  - [ ] PCIe + Host FI 端到端误差
- [ ] 补充关键实现参数:
  - [ ] FFT 网格: 128×128
  - [ ] 默认 SOCS 核数: 10
  - [ ] SOCS eigenkernel: 17×17
  - [ ] 数据类型: float32
  - [ ] HLS FFT IP 配置
  - [ ] AXI-MM bundle 数量与用途
  - [ ] XDMA DDR 数据布局与寄存器映射
- [ ] 保持 PCIe 表述一致:
  - [ ] XDMA 接口/目标链路: PCIe Gen3 x8
  - [ ] 本次主机实际协商链路: Gen3 x2
  - [ ] 表 8 的传输延迟和吞吐按实测 x2 链路报告

### Phase C: 实验与证据增强

- [ ] 补全 benchmark 设置:
  - [ ] CPU 型号、核心数、线程数
  - [ ] GCC 版本与优化参数
  - [ ] FFTW / LAPACK / BLAS / OpenMP 配置
  - [ ] MATLAB 版本
  - [ ] HLS / Vivado 版本
- [ ] 延迟拆分必须分开报告:
  - [ ] FPGA kernel-only
  - [ ] PCIe H2C
  - [ ] AXI-Lite 配置
  - [ ] FPGA 计算与主机轮询
  - [ ] PCIe C2H
  - [ ] Host FI
- [ ] 精度验证必须分层报告:
  - [ ] C simulation
  - [ ] C/RTL co-simulation
  - [ ] PCIe board 128×128 output
  - [ ] PCIe + Host FI 1024×1024 output
  - [ ] SOCS vs. full TCC across ICCAD cases
- [ ] 增加统计结果:
  - [ ] 10 个 ICCAD case 的平均值
  - [ ] 标准差
  - [ ] 最大值和最小值
  - [ ] 趋势解释，而不是只罗列表格
- [ ] 敏感性分析:
  - [ ] SOCS 核数: 10 / 50 / 400
  - [ ] FFT 网格: 64 / 128 / 256
  - [ ] 光源类型: Annular / Dipole / Quasar
  - [ ] High-NA EUV 可扩展性讨论
- [ ] 资源 breakdown:
  - [ ] FFT IP
  - [ ] 频域嵌入
  - [ ] 强度累加
  - [ ] BRAM buffer
  - [ ] AXI / control logic
- [ ] 功耗与能效:
  - [ ] 当前 4 W 仅作为 FPGA kernel-level estimate
  - [ ] 不将 4 W 外推为 PCIe full-platform power
  - [ ] 若可行，补 Vivado Power Analyzer post-implementation 报告
  - [ ] 若可行，补 idle / running / dynamic delta 板级功耗实测

### Phase D: 图表与 SCI 风格升级

- [ ] 图表建议结构:
  - [ ] Fig. 1: TCC-SOCS workflow
  - [ ] Fig. 2: CPU-FPGA co-design and XDMA dataflow
  - [ ] Fig. 3: FPGA online reconstruction pipeline
  - [ ] Fig. 4: full TCC / SOCS / FPGA output / error map
  - [ ] Fig. 5: latency breakdown
  - [ ] Fig. 6: resource utilization and performance summary
- [ ] 表格建议结构:
  - [ ] Table 1: CPU / PCIe / FPGA task partition
  - [ ] Table 2: PCIe/XDMA data layout
  - [ ] Table 3: experimental platform
  - [ ] Table 4: error taxonomy
  - [ ] Table 5: ICCAD case statistics
  - [ ] Table 6: latency breakdown
  - [ ] Table 7: software vs. FPGA comparison
  - [ ] Table 8: resource breakdown
  - [ ] Table 9: energy efficiency
- [ ] 所有图表 caption 补充实验条件、单位和数据来源
- [ ] 图中文字统一英文，图注中解释缩写
- [ ] 导出 600 dpi PNG 或矢量 PDF/SVG

### Phase E: 英文 SCI 写作与投稿材料

- [ ] 术语统一:
  - [ ] aerial image reconstruction
  - [ ] TCC-SOCS online reconstruction
  - [ ] SOCS eigenkernel
  - [ ] kernel-only latency
  - [ ] PCIe full-platform execution path
- [ ] 所有数字格式统一:
  - [ ] `10.57 ms`
  - [ ] `3.37×`
  - [ ] `67.4×`
  - [ ] `$2.93 \times 10^{-8}$`
- [ ] 强化 claim 边界:
  - [ ] 能效为 kernel-level estimate
  - [ ] PCIe full-platform power 尚未实测
  - [ ] 当前主要覆盖 Annular DUV 配置
  - [ ] PCIe x8 为接口/目标配置，x2 为本次主机实测协商链路
- [ ] 投稿前材料:
  - [ ] 转 LaTeX
  - [ ] 套用目标期刊模板
  - [ ] 补 2024-2026 最新文献
  - [ ] 准备 supplementary material
  - [ ] 准备 cover letter
  - [ ] 补 Funding / Conflict of Interest / Data Availability / Author Contributions

### 优先执行顺序

1. [ ] Abstract / Introduction / Contribution list
2. [ ] Method 公式、符号和架构描述
3. [ ] Experiments 误差分类、统计、资源 breakdown 和功耗口径
4. [ ] 图表重绘与 caption 统一
5. [ ] 英文 SCI 润色和 LaTeX 转换

### 最关键的三项原则

- [ ] 把 novelty 讲尖: 不是普通 FPGA 加速，而是 TCC-SOCS 在线重构的 CPU-FPGA/XDMA 端到端架构
- [ ] 把实验口径讲清: kernel-only、PCIe full-platform、SOCS truncation、hardware numerical error 必须分开
- [ ] 把局限讲稳: 功耗估算、PCIe x8/x2、Host FI、Annular-only 都要主动交代

---

## 📝 详细任务追踪

详细的HLS开发任务请参考: `source/SOCS_HLS/SOCS_TODO.md`

论文更新任务:
1. 优先更新 `doc/小论文/小论文.md`
2. 同步维护 `doc/小论文/TODO.md` 中的细粒度投稿核对表
3. 添加不同 mask 测试结果分析 (数据来源: ICCAD 2013 contest)
4. 添加不同分辨率测试结果分析
5. 从 `output/` 提取图像到 `doc/image/` 或论文图目录
6. 确保学术化描述、实验口径和图表风格统一

**标注**: S. Banerjee, Z. Li, and S. R. Nassif, 'ICCAD-2013 CAD contest in mask optimization and benchmark suite,' in Proc. IEEE/ACM Int. Conf. Computer-Aided Design (ICCAD), Nov. 2013.
pp. 271–274.
