# SCI 学术严谨性整改 TODO

本文档依据对当前稿件《面向 TCC-SOCS 空中像计算的低延迟高能效 FPGA/HLS 加速架构》的学术严谨性审查整理。任务分为两大工作包：

1. **论文书写修改**：不依赖新增实验即可先行开展，重点修正定义、口径、论证边界、参考文献和可复现性描述。
2. **测试数据补全**：需要修改或新增测试代码、重新运行实验并保存原始记录，重点建立公平、可重复、可统计的性能、功耗、精度和消融证据链。

> 执行原则：任何核心数字必须能够追溯到原始日志、报告或数据文件；正文中的 `measured`、`estimated`、`simulated` 和 `on-board` 必须严格区分。完成新测试前，不得把估算结果写成实测结果。

---

## 工作要点一：论文书写的修改要点

### A. P0 必须修改：影响核心结论成立性

#### A1. 修正 CPU-FPGA 性能比较口径

- [ ] 删除或暂缓使用“相同算子的优化 C++ 基准”这一表述，直到完成工作要点二中的公平基准测试。
- [ ] 在摘要、Highlights、引言、图 2、表 7、结论中统一区分以下两种口径：
  - `kernel-only latency`：仅包含 $128\times128$ SOCS 在线重构内核。
  - `end-to-end latency`：包含输入准备、H2C、AXI-Lite 配置、FPGA 计算、C2H、Host FI 和输出形成。
- [ ] 明确当前 CPU `35.6 ms` 的实际计时范围：包含 FFTW plan 创建、$128\times128$ SOCS 重构、$128\rightarrow1024$ 傅里叶插值和中心裁剪。
- [ ] 明确当前 FPGA `10.57 ms` 的来源：由 HLS 周期数按 250 MHz 换算，不包含 PCIe 和 Host FI。
- [ ] 在公平测试完成前，将“3.37 倍同算子加速”改为“现有非对称计时口径下的初步估算”，或从摘要、Highlights 和结论中移除。
- [ ] 公平测试完成后，用新的 kernel-only 与 end-to-end 数据替换全文旧数字，并重新计算加速比、延迟降低率和吞吐量。

**验收标准：** 表 7 中 CPU 与 FPGA 每一列均明确列出计时起点、终点、是否包含 plan、FI、PCIe 和 I/O；任何加速比只在相同任务边界内计算。

#### A2. 修正 CPU 数值精度与“优化基准”描述

- [ ] 将当前 C++ 基准从“单精度浮点”改为与源码一致的“双精度 FFTW/双精度中间累加”，除非后续已实现并测试 `fftwf` 单精度版本。
- [ ] 明确掩模、本征核、FFT、中间累加和最终输出各自的数据类型，不能仅用“C++ 单精度”概括整个链路。
- [ ] 当前基准使用 `FFTW_ESTIMATE` 且在计时区间内创建 plan，不应称为“优化 C++ 基准”。在新基准完成前改称“C++ reference implementation”。
- [ ] 新基准完成后，在实验配置中写明：`fftw`/`fftwf`、plan 策略、plan 是否复用、线程数、绑核策略、编译选项和链接库版本。

**验收标准：** 论文中的 CPU 精度、FFT 库和线程描述与实际编译和运行配置逐项一致。

#### A3. 降级当前能效结论，避免把估算写成实测

- [ ] 在实测功耗完成前，从标题中删除或弱化“高能效”，可改为“面向低功耗部署”或只保留“低延迟”。
- [ ] 在实测功耗完成前，从 Highlights、摘要和结论中移除“67.4 倍能效提升”这一强结论。
- [ ] 将表 8 标题改为“基于假设功耗的能耗情景估算”，并明确 80 W 与 4 W 均非本实验同步测得。
- [ ] 不得使用当前 CPU 的 35.6 ms 与 FPGA 的 10.57 ms 直接计算“kernel-level energy efficiency”，因为两者计时范围不同。
- [ ] 分别定义并统一使用：
  - FPGA kernel dynamic power；
  - FPGA device total power；
  - board power；
  - CPU package power；
  - Host-FPGA system power。
- [ ] 实测完成后，分别报告 kernel-only 能效和 end-to-end 能效，不能用其中一个替代另一个。

**验收标准：** 所有能效数字都附有功耗来源、延迟范围、公式和是否实测的说明；摘要中的结论能够由实验表格直接复算。

#### A4. 严格区分综合估计、联合仿真和板上实测

- [ ] 将“在 xcku5p 上实现 10.57 ms”改为“面向 xcku5p 综合得到 2,643,645 周期，在 250 MHz 假设下对应 10.57 ms”，直到取得板上周期计数。
- [ ] 分别使用以下术语，不得混写：
  - `HLS synthesis-estimated latency`；
  - `C/RTL co-simulation latency`；
  - `on-board kernel latency`；
  - `PCIe end-to-end wall-clock latency`。
- [ ] 表 6 增加“数据来源”列，标明每个阶段是 HLS report、co-simulation 还是板上计数器结果。
- [ ] 板上验证结论限定为：xcku5p/XDMA 数据流、AXI-Lite 控制和输出精度已经验证；不得由此自动推导 10.57 ms 已在板上测得。
- [ ] 说明 CPU 性能基准和 xcku5p 板上实验使用了不同主机，避免把双路 Xeon 8163 与板上 Xeon E3 描述成同一测试平台。

**验收标准：** 读者可从实验表中清楚判断每个结果属于估计、仿真还是实测，以及结果在哪台主机和哪块 FPGA 上获得。

#### A5. 修正式 (5) 的量纲和符号定义

- [ ] 不再把离散像素数 $1024$ 直接作为物理长度与 $\lambda=193\,\mathrm{nm}$ 相除。
- [ ] 引入物理视场和采样间隔：

  $$
  L_x^{\mathrm{phys}}=N_x^{\mathrm{sample}}\Delta x,\qquad
  L_y^{\mathrm{phys}}=N_y^{\mathrm{sample}}\Delta y.
  $$

- [ ] 分别推导两个方向的频率截止索引，避免式 (5) 左侧写 $N_x=N_y$ 却只使用 $L_x$：

  $$
  n_{x,\max}=\left\lfloor
  \frac{L_x^{\mathrm{phys}}NA(1+\sigma_{out})}{\lambda}
  \right\rfloor,
  \quad
  n_{y,\max}=\left\lfloor
  \frac{L_y^{\mathrm{phys}}NA(1+\sigma_{out})}{\lambda}
  \right\rfloor.
  $$

- [ ] 避免同一个 $N_x$ 同时表示“图像采样数”和“频率单侧截止索引”，建议使用 $N_x^{\mathrm{sample}}$ 与 $n_{x,\max}$ 区分。
- [ ] 明确 $L_x,L_y,\Delta x,\Delta y,\lambda$ 的单位及频率坐标的归一化方式。
- [ ] 检查代码中 `Lx=1024` 的物理含义；若确实隐含 $\Delta x=1\,\mathrm{nm}$，必须在配置表和方法中明确说明并给出来源。

**验收标准：** 式 (5) 量纲闭合，代入表 4 参数可以完整复算出 $n_{x,\max}=n_{y,\max}=8$。

#### A6. 收敛论文当前能够支持的结论边界

- [ ] 将“结果验证了该架构在低功耗计算光刻部署中的有效性”收敛为“结果验证了该架构的板级功能正确性及其作为在线重构内核的可部署性”。
- [ ] 在多掩模、多光源、多核数实验完成前，不得声称已经验证广泛配置灵活性或工业场景普适性。
- [ ] 将 Dipole、Quasar、High-NA EUV 和完整 OPC/SMO 部署明确列为未来工作，不写成当前能力。
- [ ] 明确本文没有验证完整 OPC/SMO 工具链加速，也没有验证 Host-FPGA 全系统能效。
- [ ] 删除“完整证据链”等自我评价式措辞，改为客观列出已有验证和仍缺失的实验。

**验收标准：** 摘要、引言、实验讨论和结论的 claim 范围一致，且不超过现有实验覆盖范围。

### B. P1 重点修改：影响一区/二区完整性

#### B1. 重构 Related Work 与创新性论证

- [ ] 将相关工作拆分为四类：TCC-SOCS 数值方法、CPU/GPU 计算光刻加速、FPGA/专用硬件光刻加速、二维 FFT/HLS 架构。
- [ ] 对每类工作说明其任务边界、数据规模、硬件平台、精度和是否有板上验证，避免仅罗列文献。
- [ ] 增加 SOTA 对比表，至少包括：算法、FFT 尺寸、核数、精度、器件、频率、延迟、吞吐、资源、功耗、是否包含 PCIe。
- [ ] 对不可直接比较的数据标注 `not directly comparable`，不得跨算法、跨网格直接比较加速比。
- [ ] 将 novelty 写成可证伪的具体差异，不使用“首次”“业界领先”等无法由系统检索和公平实验证明的措辞。
- [ ] 补充近五年的 FPGA FFT/HLS、计算光刻硬件、GPU SOCS/OPC/ILT 和可复现系统研究。

**验收标准：** 引言中的 research gap 可以由 Related Work 表格逐项支持，每项贡献都能对应一个已有工作的明确不足。

#### B2. 重写贡献点，避免把工程组合直接等同于创新

- [ ] 将贡献压缩为 3 项左右，每项按“问题 - 方法 - 证据”组织。
- [ ] 不把“使用 HLS FFT IP”“使用 BRAM”“使用 AXI-MM”本身作为创新；需说明针对 TCC-SOCS 数据依赖做了什么专用设计。
- [ ] 把 CPU-FPGA 分工描述为系统架构贡献时，明确其区别于常见 Host-accelerator 模式的专用数据复用和在线调用特点。
- [ ] “面积优化二维 IFFT”必须由消融或基线资源对比支撑；若未完成消融，改为“area-conscious implementation”。
- [ ] “降低存储访问竞争”必须由带宽、stall、II 或端口利用数据支撑；若无数据，改为设计动机而非实验结论。

**验收标准：** 每项贡献至少对应一张实验表、一个定量结果或一段可核验实现说明。

#### B3. 补齐方法可复现性描述

- [ ] 增加算法伪代码或流程表，明确每个 SOCS 核的嵌入、IFFT、缩放、幅度平方和累加顺序。
- [ ] 解释嵌入地址公式，特别是代码中的非直观位置计算，证明其与频谱排列、FFTshift 和软件 Golden 一致。
- [ ] 给出 FFT 归一化约定：FFTW 与 HLS FFT 是否归一化、块浮点指数如何补偿、二维总缩放如何计算。
- [ ] 明确 `ap_fixed<32,1>` 的量化、溢出、舍入和饱和模式，而不只说明总位宽。
- [ ] 给出运行时参数合法范围，以及超过最大核尺寸时的行为。
- [ ] 说明 7 个 AXI-MM 接口在实际 Vivado block design 中连接到哪些 interconnect、DDR 控制器或端口，避免把逻辑 bundle 数量等同于独立物理 DDR 通道。
- [ ] 给出工具精确版本、器件、速度等级、目标时钟、实现时序裕量和生成 bitstream 的配置哈希或 commit。

**验收标准：** 独立研究者仅依据论文和补充材料即可复现输入格式、计算顺序、数值缩放和接口配置。

#### B4. 修正精度指标定义和数据来源

- [ ] 为 RMSE、MaxAE、相对误差、PSNR、SSIM 和二值一致率逐一定义公式。
- [ ] 说明 PSNR 的峰值取固定 1、参考图像最大值还是数据动态范围。
- [ ] 说明相对误差分母的零值处理和 epsilon。
- [ ] 说明二值图形一致率的阈值来源，不能只给 98.77%。
- [ ] 统一正文与数据文件中的 10 核 RMSE/MaxAE；若来自 MATLAB 与 C++ 两条不同链路，必须分行报告并解释差异。
- [ ] 不使用“高精度参考”代替可追溯定义；明确 MATLAB 完整 TCC、C++ full TCC、SOCS software Golden 各自的生成程序和精度。
- [ ] 解释为何 C/RTL RMSE 与板上 RMSE 不同，确认两者使用相同输入、版本和参考对象后再并列。

**验收标准：** 表 5 每一行都包含测试对象、参考对象、分辨率、数据版本和指标定义，正文数字与原始 CSV/JSON 一致。

#### B5. 修正图表和实验叙述

- [ ] 图 2 的 CPU/FPGA 延迟标注改为公平测试后的两个口径，或暂时删除 3.37 倍徽标。
- [ ] 图 3 明确展示的是模型截断误差还是硬件实现误差，不能在同一误差图中混用不同参考。
- [ ] 图 4 的阶段分解注明来源为 HLS estimate 或板上计数器。
- [ ] 图 5 在功耗实测前不得把 67.4 倍作为实验证据突出展示。
- [ ] 表 2 中中间缓冲区和输出缓冲区的 H2C/C2H 方向分别写清楚，清零写入和结果回读不要统一标为 H2C。
- [ ] 表 3 检查 AXI burst/outstanding 参数是否与最终综合报告一致，而不是与旧版本配置一致。
- [ ] 表 9 区分 HLS 综合资源、Vivado post-synthesis 资源和 post-implementation 资源；正文只能引用实际获得的那一级报告。

**验收标准：** 每张图表均可独立理解，caption 中写明配置、样本数、测量方式和结果口径。

### C. P2 投稿前规范化

#### C1. 系统清洗参考文献

- [ ] 修正文献 13 作者为 `Xu Ma`、`Gonzalo R. Arce`，补 DOI `10.1364/OE.16.020126`。
- [ ] 修正文献 32 的 `hardwareacceleration` 拼写并补 DOI `10.1145/1344671.1344683`。
- [ ] 为文献 31 补 DOI `10.1145/1575774.1575776`。
- [ ] 为文献 33 补 DOI `10.1109/FCCM.2012.40`，注意 `.32` 对应另一篇论文。
- [ ] 逐条核验作者、题目、期刊/会议、年份、卷期、页码或文章号和 DOI。
- [ ] 统一期刊名称大小写和缩写，不混用完整名称与非标准缩写。
- [ ] 检查每个引文是否真正支持所在句子，避免一组编号笼统支撑多个不同事实。
- [ ] 增加近五年文献并说明它们与本文的直接关系，不以文献数量替代相关性。

**验收标准：** 全部参考文献可通过 DOI/Crossref/IEEE/Optica 元数据匹配，无作者拼写错误、缺文章号或错误 DOI。

#### C2. 增加可复现性与数据可用性声明

- [ ] 增加 `Data and Code Availability` 小节。
- [ ] 为用于正文表格和图片的数据建立清单，记录文件路径、脚本、输入和输出。
- [ ] 固定投稿版本的 Git commit/tag，不引用持续变化的分支作为唯一复现入口。
- [ ] 如公开仓库，使用 Zenodo 等归档并取得 DOI；如不能公开，说明审稿阶段和发表后的可用方式。
- [ ] 补充 `Conflict of Interest`、`Funding`、`Author Contributions` 和必要的 AI 使用声明，按目标期刊要求编写。

**验收标准：** 论文中的每个核心表格或图都可由归档数据与脚本重新生成。

#### C3. 全文术语和格式统一

- [ ] 统一使用 `TCC-SOCS online reconstruction`、`SOCS eigenkernel`、`software reference`、`Fourier interpolation` 等术语，避免中英文混写。
- [ ] 统一 `kernel` 的含义，区分 SOCS eigenkernel、FPGA compute kernel 和 convolution kernel。
- [ ] 统一乘号与数字格式，例如 `$3.37\times$`、`$2.93\times10^{-8}$`、`250 MHz`。
- [ ] 统一图表单位、有效数字和小数位，避免估算数据呈现过多有效数字。
- [ ] 删除“极好”“完美匹配”“完整证据链”“工程最优”等主观评价，改为定量、可验证表述。
- [ ] 根据最终目标期刊调整摘要长度、Highlights 数量、关键词、参考文献格式和章节结构。

**验收标准：** 中英文稿数字、术语、图号、表号和 claim 完全对应。

---

## 工作要点二：测试数据需要补全的点

### D. P0 必须补测：决定性能与能效结论是否成立

#### D1. 重建公平的 CPU kernel-only 基准

- [ ] 从现有 `calcSOCS` 中拆出纯 $128\times128$ 在线重构函数，计时范围只包含：频域嵌入、10 次二维 IFFT、幅度平方、权重累加和 FFTshift。
- [ ] 将 $128\rightarrow1024$ FI、中心裁剪、文件写入、日志输出和图片生成移出 kernel-only 计时区间。
- [ ] 在计时前创建 FFTW plan，并在多次调用中复用。
- [ ] 分别实现并测试：
  - [ ] 双精度 `fftw` 单线程版本；
  - [ ] 单精度 `fftwf` 单线程版本；
  - [ ] 单精度或双精度 FFTW 多线程版本；
  - [ ] 条件允许时增加 MKL FFT 或同等级优化库版本。
- [ ] 固定线程数、CPU 亲和性和 NUMA 节点，记录 CPU governor、实际频率和编译命令。
- [ ] 先预热至少 20 次，再正式运行至少 1000 次。
- [ ] 保存每次运行的原始延迟，不只保存均值。
- [ ] 输出 mean、median、standard deviation、minimum、maximum、P5、P95 和 95% confidence interval。

**需产出数据：**

- `cpu_kernel_latency_raw.csv`
- `cpu_kernel_latency_summary.json`
- `cpu_benchmark_environment.json`
- 完整编译命令、FFTW wisdom/plan 策略和运行脚本

**验收标准：** CPU kernel-only 输出与相同配置 software Golden 满足预设误差阈值，且计时范围与 FPGA kernel-only 一致。

#### D2. 获取 FPGA 板上 kernel-only 实测延迟

- [ ] 不再仅使用 HLS 周期估计；在板上测量 `ap_start` 到 `ap_done`。
- [ ] 优先增加 FPGA AXI cycle counter 或在 IP 周围增加硬件计数器，避免 Linux 调度和寄存器轮询污染内核时间。
- [ ] 同时保留 Host wall-clock 计时，用于展示软件控制开销，但不得替代硬件周期计数。
- [ ] 预热后重复运行至少 1000 次，保存每次周期数和 wall-clock。
- [ ] 验证板上工作时钟确为 250 MHz，提供时钟配置和实现后时序报告。
- [ ] 报告 mean、median、standard deviation、P95、最坏值和是否存在抖动。
- [ ] 对照 HLS estimate、C/RTL co-simulation 与 on-board measured cycles，解释差异。

**需产出数据：**

- `fpga_kernel_cycles_raw.csv`
- `fpga_kernel_latency_summary.json`
- 时钟与 post-implementation timing report
- 计数器寄存器定义和读取脚本

**验收标准：** 论文 10.57 ms 若继续保留，必须能由板上周期数和实测时钟直接复算；否则用实际板上结果替换。

#### D3. 重测 Host-FPGA end-to-end 延迟

- [ ] 使用同一测试脚本逐阶段记录：数据准备、H2C mask、H2C kernels/weights、AXI-Lite 配置、内核执行、C2H 输出、Host FI 和最终输出形成。
- [ ] 分别测试冷启动、单窗口和批量窗口模式。
- [ ] 批量模式至少测试 batch size：1、10、100、1000。
- [ ] 分别报告本征核和权重每次传输与驻留 DDR 复用两种场景。
- [ ] 使用双缓冲或异步 DMA 时，报告各阶段独立时间及重叠后的实际吞吐。
- [ ] 保存每轮原始 wall-clock 数据并报告统计分布。
- [ ] 在同一任务边界下与 CPU end-to-end 结果比较：两侧都输出 $1024\times1024$ 空中像。

**需产出数据：**

- `fpga_e2e_latency_raw.csv`
- `fpga_e2e_stage_breakdown.csv`
- `batch_throughput.csv`
- `cpu_e2e_latency_raw.csv`
- PCIe 链路状态与驱动版本记录

**验收标准：** 可以分别回答“单窗口延迟是多少”“批处理稳态吞吐是多少”“PCIe 与 Host FI 分别占多少”。

#### D4. 实测功耗并重新计算能效

- [ ] CPU 测试期间使用 RAPL、BMC 或外部功率计采样，保存时间序列。
- [ ] 分别测量 CPU idle、kernel-only 和 end-to-end 功率；说明使用 package power 还是整机输入功率。
- [ ] FPGA 使用板载传感器、PMBus、XRT/xbutil、外部功率计或活动文件驱动的 Vivado Power Analyzer。
- [ ] 分别测量 FPGA idle、kernel running、DDR/PCIe 活动和完整 end-to-end 功率。
- [ ] 若使用 Vivado Power Analyzer，保存 SAIF/VCD、static/dynamic/total power、环境温度和电压假设。
- [ ] 每个平台至少进行 3 组独立测量，报告均值、标准差和测量设备精度。
- [ ] 分别计算：

  $$
  E_{kernel}=P_{dynamic,kernel}\times T_{kernel},
  \qquad
  E_{e2e}=\int_{t_0}^{t_1}\left(P(t)-P_{idle}\right)dt.
  $$

- [ ] 同时报告含 idle 的绝对能耗，避免只报告扣除静态功耗后的有利结果。
- [ ] 用公平延迟和实测功率重新计算 J/image、images/J 和能效提升倍数。

**需产出数据：**

- `cpu_power_trace.csv`
- `fpga_power_trace.csv`
- `power_measurement_environment.json`
- `energy_summary.json`
- 测量设备或传感器说明

**验收标准：** 任何能效倍数都可以由公开的功率时间序列和延迟数据重新计算，并给出不确定度。

### E. P1 必须补强：决定论文实验完整度

#### E1. 多掩模与多配置精度验证

- [ ] 建立独立测试集，不只使用当前单一 Golden 掩模。
- [ ] 至少覆盖以下掩模类型：line/space、contact/hole、复杂二维图形、稀疏图形、稠密图形、随机或真实裁剪窗口。
- [ ] 建议至少 30 个独立窗口；冲击一区时建议 100 个以上。
- [ ] 对每个窗口保存完整 TCC、SOCS software、FPGA $128\times128$ 和 Host FI $1024\times1024$ 输出指标。
- [ ] 报告 RMSE、NRMSE、MaxAE、PSNR、SSIM、阈值后二值一致率以及关键 CD/EPE 指标。
- [ ] 报告均值、标准差、P95 和最坏样本，并展示最坏样本误差图。
- [ ] 记录输入哈希和 Golden 生成版本，防止不同数据版本混用。

**需产出数据：**

- `multi_mask_accuracy.csv`
- `multi_mask_summary.json`
- 最坏样本和代表样本可视化
- 测试集 manifest 与文件哈希

**验收标准：** 论文结论不再依赖单一掩模，且最坏样本仍满足预先定义的硬件实现误差阈值。

#### E2. SOCS 核数可扩展性测试

- [ ] 至少测试 $N_k=1,5,10,20,50$；若硬件综合上限不足，明确最大支持值。
- [ ] 对每个核数报告模型截断误差、硬件实现误差、周期数、延迟和吞吐量。
- [ ] 验证周期数是否随核数近似线性增长，并给出线性拟合或复杂度解释。
- [ ] 比较核数增加带来的精度收益与延迟/能耗代价，形成 Pareto 曲线。
- [ ] 不再仅凭单个 10 核点宣称“工程最优平衡点”；最优点必须由明确目标函数或 Pareto 分析支持。

**需产出数据：**

- `kernel_count_scaling.csv`
- `accuracy_latency_pareto.csv`
- 核数-精度、核数-延迟和核数-能耗曲线

**验收标准：** 可以定量解释为什么选 10 核，而不是把 10 核作为未经证明的固定选择。

#### E3. 不同有效核尺寸与运行时配置测试

- [ ] 测试多个 $n_{x,\max},n_{y,\max}$，例如 4、8、12、16、24，覆盖非方形配置时再加入 $n_x\ne n_y$。
- [ ] 对每种配置验证嵌入位置、边界检查、FFTshift 和缩放正确性。
- [ ] 报告不同有效核尺寸下的周期数、RMSE、总线读取量和资源是否保持不变。
- [ ] 检查最大核尺寸是否能放入固定 $128\times128$ 网格，并明确合法范围。
- [ ] 使用真实光学参数生成测试数据，避免只用合成核证明接口可运行。

**需产出数据：**

- `kernel_size_runtime_config.csv`
- 每种配置的 Golden 对比结果
- 参数合法范围和异常输入测试报告

**验收标准：** “不同有效核尺寸复用同一流水线”由多个板上配置实测支持。

#### E4. 多光学条件鲁棒性测试

- [ ] 测试 Annular、Dipole 和 Quasar 等光源；若暂不支持，收敛论文 claim。
- [ ] 测试多个 $NA$、$\sigma_{in}$、$\sigma_{out}$ 和 defocus 值。
- [ ] 对每种光学配置重新生成 TCC 和 SOCS kernels，验证运行时参数与 FPGA 数据路径。
- [ ] 报告模型截断误差与硬件实现误差，避免把光学模型变化导致的误差归因于 FPGA。
- [ ] 对 High-NA EUV 只做经过验证的实验或明确的资源模型分析，不能仅作泛化结论。

**需产出数据：**

- `optical_configuration_accuracy.csv`
- 配置文件与 Golden 数据 manifest
- 各配置误差分布图

**验收标准：** 论文关于配置灵活性的表述与实际测试覆盖范围一致。

#### E5. 输入分辨率与系统可扩展性测试

- [ ] 至少测试 256、512、1024 输入；若当前分支目标包括 2048，再加入 2048。
- [ ] 分别报告 Host mask FFT、数据量、H2C/C2H 时间、FPGA kernel 时间和 Host FI 时间。
- [ ] 说明固定 $128\times128$ 在线网格下输入分辨率变化为何影响或不影响 FPGA kernel 时间。
- [ ] 对更大分辨率分析 DDR 容量、地址宽度、PCIe 数据量和 Host 预处理瓶颈。
- [ ] 若 4096/8192 仅有软件配置而无板上验证，必须明确标注为未验证。

**需产出数据：**

- `resolution_scaling.csv`
- `resolution_stage_breakdown.csv`
- 各分辨率精度与吞吐曲线

**验收标准：** 可扩展性结论同时包含精度、计算和数据搬运，不只报告理论支持范围。

#### E6. 架构优化消融实验

- [ ] 建立可复现 baseline，例如单 AXI bundle、DDR 中间存储、固定缩放或浮点 FFT 的基础版本。
- [ ] 分别测试以下优化的独立贡献：
  - [ ] BRAM 转置缓冲；
  - [ ] 多 AXI-MM bundle；
  - [ ] BRAM 片上累加；
  - [ ] 块浮点缩放；
  - [ ] LUT 映射 FFT 乘法；
  - [ ] 核间时分复用。
- [ ] 每个版本使用相同器件、目标时钟、输入和综合工具。
- [ ] 报告 LUT、FF、DSP、BRAM、Fmax、II、latency、RMSE 和估算/实测功耗。
- [ ] 若某个优化无法单独开关，至少提供相邻版本的受控对比并说明同时变化的因素。
- [ ] 10 核全并行约 3000 BRAM 的结论应由综合结果或透明资源模型支持，不只写经验估计。

**需产出数据：**

- `ablation_results.csv`
- 各版本 HLS 与 Vivado 报告
- 版本、commit、配置与结果映射表

**验收标准：** “面积优化”“降低访存竞争”“块浮点提升精度”等核心设计结论均有对应定量证据。

#### E7. 最终实现资源与时序数据

- [ ] 保存最终 xcku5p 配置的 HLS synthesis、Vivado synthesis 和 implementation 报告。
- [ ] 明确正文表 9 使用哪一级资源结果，三者不得混合。
- [ ] 报告 WNS、TNS、achieved Fmax、clock uncertainty 和关键路径。
- [ ] 核对 LUT、FF、DSP、BRAM 的绝对数和百分比是否都来自同一器件、同一版本。
- [ ] 检查旧 xcku3p、xcku060 和 xcku5p 报告，避免将旧器件绝对资源数套用为当前实现结果。
- [ ] 保存最终 bitstream 对应的 commit、Vivado project 配置和 IP 版本。

**需产出数据：**

- 最终 `csynth.rpt/xml`
- post-synthesis utilization report
- post-implementation utilization/timing/power reports
- bitstream manifest

**验收标准：** 论文中的器件、频率、资源、时序、bitstream 和板上结果可追溯到同一个最终设计版本。

### F. P2 可复现性与数据治理

#### F1. 建立论文数字到原始证据的映射

- [ ] 创建 `paper_evidence_manifest.csv`，每行记录：论文位置、指标名称、数值、单位、实验配置、原始文件、生成脚本、commit 和日期。
- [ ] 为摘要、Highlights、表 5 至表 9 和图 3 至图 5 的每个数字建立映射。
- [ ] 自动检查正文数字与 JSON/CSV 是否一致，避免手工复制产生漂移。
- [ ] 区分 MATLAB、C++、C simulation、C/RTL、JTAG、PCIe 和 Host FI 各数据源。

**验收标准：** 任意点击一个核心数字，都能在两步以内找到原始日志和生成脚本。

#### F2. 固化 Golden 与指标计算流程

- [ ] 为每个 Golden 文件记录 shape、dtype、endianness、归一化方式和 SHA-256。
- [ ] 统一 MATLAB 与 C++ Golden 的命名，禁止都简称为 `software Golden`。
- [ ] 使用同一个指标脚本计算 RMSE、MaxAE、PSNR、SSIM 和二值一致率。
- [ ] 给指标脚本增加单元测试，验证零值、NaN、Inf、不同 shape 和阈值边界。
- [ ] 将图表直接由最终 CSV/JSON 生成，禁止手工录入数字。

**验收标准：** 在干净环境运行一条验证命令即可重建主要精度表和图。

#### F3. 准备可归档实验包

- [ ] 提供环境说明、依赖版本、编译命令、运行顺序和预期输出。
- [ ] 整理最小可复现实验，避免依赖个人绝对路径。
- [ ] 将原始日志、报告、输入样本、Golden、分析脚本和图表脚本统一纳入版本化目录。
- [ ] 清理报告中的过期结论和互相冲突的设备/频率信息。
- [ ] 发布前使用全新环境按文档完整复跑一次，并记录复跑结果。

**验收标准：** 独立人员能够按 README 生成至少一个 CPU 结果、一个 FPGA 仿真结果和全部论文图表。

---

## 建议执行顺序

### 第一阶段：立即进行的文字止损

- [ ] 完成 A1-A6，先修正不对称比较、精度类型、能效、估计/实测和公式量纲。
- [ ] 暂时从标题、摘要、Highlights 和结论移除尚未严格成立的 3.37 倍与 67.4 倍强 claim。
- [ ] 完成 C1 的参考文献硬错误修正。

### 第二阶段：重建核心性能与能效证据

- [ ] 完成 D1 CPU kernel-only benchmark。
- [ ] 完成 D2 FPGA 板上周期实测。
- [ ] 完成 D3 end-to-end 与 batch 测试。
- [ ] 完成 D4 功耗和能效实测。
- [ ] 用新数据回填摘要、表 6-8、图 2、图 4、图 5 和结论。

### 第三阶段：提升一区/二区实验完整度

- [ ] 完成 E1 多掩模精度统计。
- [ ] 完成 E2 核数 Pareto 分析。
- [ ] 完成 E3/E4 多核尺寸与多光学配置验证。
- [ ] 完成 E5 分辨率扩展测试。
- [ ] 完成 E6 消融实验和 E7 最终实现报告。

### 第四阶段：投稿材料固化

- [ ] 完成 B1-B5 的 Related Work、贡献、方法和图表重写。
- [ ] 完成 F1-F3 的证据映射、Golden 固化和归档实验包。
- [ ] 选择目标期刊并完成 C2-C3 的声明、格式和中英文一致性检查。

---

## 最终完成定义

只有同时满足以下条件，才可恢复“低延迟高能效”和定量加速/能效倍数作为论文核心结论：

- [ ] CPU 与 FPGA kernel-only 比较具有相同输入、精度说明、输出范围和计时边界。
- [ ] FPGA 延迟来自板上硬件周期计数，而不只是 HLS 估算。
- [ ] end-to-end 结果包含 PCIe 与 Host FI，并与 CPU 相同输出任务比较。
- [ ] 功耗来自同步实测或具有活动信息的可信功耗分析，并报告测量不确定度。
- [ ] 结论经过多掩模、多核数和多配置测试，不依赖单一样本。
- [ ] 面积、带宽和数值优化具有消融或受控对比证据。
- [ ] 所有论文数字均可追溯到固定版本的原始数据、脚本和报告。
