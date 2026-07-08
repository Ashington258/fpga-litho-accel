# SCI 投稿前修改核对表

本文已具备较好的投稿基础：贡献点清晰，包含 CPU-FPGA 协同、离线/在线解耦、HLS FFT、块浮点缩放、多 AXI-MM、BRAM 缓冲、板上验证和能效对比。后续修改重点是强化创新定位、实验严谨性、图表质量和投稿规范。

## SCI 升级总目标与执行路线

目标是将当前工程验证型小论文升级为面向 IEEE TCAD、ACM TODAES、Optics Express、JM3 等期刊的完整 SCI 稿件。修改重点不是简单语言润色，而是让论文具备清晰的 novelty、严谨的实验边界、可复现的实现细节和规范的英文 SCI 表达。

### 总目标

- [x] 明确论文主线：本文不是通用 FPGA 加速报告，而是面向 **TCC-SOCS online reconstruction** 的 CPU-FPGA/XDMA 端到端架构。
- [x] 将所有 claim 绑定到证据：10.57 ms、3.37x、67.4x、RMSE、资源占用、PCIe 单窗口板上验证路径 66.89 ms 均需在正文、表格和图中口径一致。
- [x] 明确实验边界：kernel-only latency、PCIe single-window on-board validation/profile path、SOCS truncation error、hardware numerical error、Host FI error 分开报告。
- [x] 强化可重复性：HLS/Vivado 版本、FFT IP 配置、关键 pragma、AXI bundle、XDMA 地址布局、baseline 编译参数和线程数都能被复现。
- [ ] 完成 SCI 英文表达统一：术语、数字格式、时态、图表 caption、参考文献和投稿材料符合目标期刊规范。

### 推荐执行顺序

1. [x] 第一轮：重写 Abstract、Introduction 和 contribution list，先把 novelty 与论文主线讲尖。
2. [ ] 第二轮：规范 Method 公式、符号、架构图、伪代码、HLS/XDMA 配置和误差定义。
3. [ ] 第三轮：补强 Experiments，完成 baseline、精度分类、统计结果、资源 breakdown、功耗口径和 PCIe Gen3 x8 实测说明。
4. [ ] 第四轮：升级图表与表格，统一 caption、单位、有效数字、图号表号和正文引用。
5. [ ] 第五轮：英文 SCI 化润色，控制摘要长度，统一术语，消除口语化和未量化表述。
6. [ ] 第六轮：转 LaTeX 模板，准备 cover letter、highlights、supplementary material 和投稿系统文件。

### 投稿定位

- [ ] 若目标为 IEEE TCAD / ACM TODAES：突出 HLS 架构、资源/延迟/带宽 trade-off、可复现实现和硬件验证。
- [ ] 若目标为 Optics Express / JM3：突出 TCC-SOCS 成像模型、SOCS 截断精度、光学配置适配性和计算光刻应用意义。
- [ ] 在定稿前选择一个主目标期刊，并按该期刊重排标题、摘要、图表密度、参考文献风格和篇幅。

## P0 必做：投稿接受率关键项

### 1. 创新性与贡献定位

- [x] 在 Introduction 中明确本文聚焦的是 **TCC-SOCS 在线重构阶段**，不是完整 TCC 构建或完整 OPC/SMO 系统；同时说明已完成 Host-FPGA PCIe 单窗口板上集成与验证。
- [x] 增加或强化 research gap：现有 FPGA 光刻加速工作多为早期 aerial image 仿真或非 TCC-SOCS 流程，缺少面向在线 TCC-SOCS 重构的端到端 FPGA/HLS 架构与板上验证。
- [x] 在 Introduction 中补充工业意义背景，例如全芯片 OPC/SMO 中大量窗口和多轮迭代会放大单次 online reconstruction 的延迟与能耗收益；该描述需配参考文献或谨慎表述为应用动机。
- [ ] 在 Related Work 中单独对比三类工作：
  - [ ] CPU/GPU TCC-SOCS 或空中像计算加速。
  - [ ] FPGA 二维 FFT / 图像计算加速。
  - [ ] AI surrogate / learning-based OPC、ILT 或 lithography simulation。
- [ ] 将 Related Work 按“算法近似 -> CPU/GPU 加速 -> FPGA/专用硬件 -> AI surrogate”组织，并在段尾明确本文 gap。
- [x] 在贡献列表中突出 4 个核心贡献：
  - [x] CPU-FPGA 离线/在线解耦框架。
  - [x] 面向 $17 \times 17$ SOCS kernel 到 $128 \times 128$ FFT 网格的频域嵌入与 HLS 流水线。
  - [x] HLS FFT IP、块浮点缩放、LUT 映射乘法、BRAM 累加缓冲和 7 个 AXI-MM 接口的资源/带宽优化。
  - [x] C 仿真、C/RTL 联合仿真、板上验证、ICCAD 多图形泛化和能效评估。
- [x] 避免把 MATLAB 完整 TCC 的 45.3x 加速作为唯一主卖点；正文中重点强调相对 C++ SOCS 基准的 3.37x 加速和约 67.4x 能效提升。
- [x] 增加一段 CPU/GPU/FPGA trade-off 讨论：GPU 适合高吞吐批处理，FPGA 适合低功耗、低延迟、确定性在线内核。
- [x] 在贡献和结论中避免把实验结果本身写成独立创新点；实验应服务于证明架构创新。

### 2. 功耗与能效验证

- [x] 明确当前功耗数据来源：Vivado 估算、后实现功耗分析、板上测量或其他来源。
- [ ] 若使用 Vivado Power Analyzer：
  - [ ] 写明工具版本、目标器件、时钟频率、切换率或 SAIF/VCD 来源。
  - [ ] 区分 static power、dynamic power 和 total power。
  - [x] 区分 FPGA kernel 估算功耗与 PCIe 单窗口板上验证路径功耗口径，说明后者仍需板上实测。
- [ ] 若条件允许，补充板上实测功耗：
  - [ ] 记录测量设备或板卡监控接口。
  - [ ] 给出 idle power、kernel running power 和 dynamic delta。
  - [ ] 明确能效计算公式和单位，例如 images/s/W 或 J/image。
- [x] 若暂时没有板上功耗，明确写成 limitation：67.4x 为 kernel-level estimated energy efficiency，不代表 Host+PCIe+DDR 全平台能效。
- [x] 统一 CPU 与 FPGA 能效对比口径：
  - [x] CPU 功耗来源。
  - [x] CPU 线程数与编译优化参数。
  - [x] 是否包含数据传输和 I/O。
- [x] 在实验节中补充一句限制说明：当前能效提升为估算值或板上实测值，适用范围是什么。

### 3. 对比实验

- [x] 保留 MATLAB 完整 TCC 基准，但标注其作用是验证算法链路和物理参考，不作为最公平性能基准。
- [x] 强化 C++ SOCS baseline 描述：
  - [x] 编译器与优化选项。
  - [x] CPU 型号、频率、线程数。
  - [x] 是否使用 FFTW、MKL、OpenMP 或手写 FFT。
  - [x] 输入尺寸、kernel 数量和 FFT 网格是否与 FPGA 完全一致。
- [ ] 如条件允许，增加至少一个更强 CPU baseline：
  - [ ] 单线程 C++ SOCS。
  - [ ] 多线程 C++ SOCS。
  - [ ] FFTW/MKL 版本。
- [ ] 明确 MATLAB baseline 版本、运行模式和是否使用多线程/并行工具箱。
- [ ] 明确 FFTW/LAPACK/BLAS/OpenMP 的版本、线程数和链接方式。
- [ ] 如条件允许，增加 GPU baseline 或说明无法公平复现 GPU 对比的原因。
- [x] 在 Discussion 中解释 3.37x 加速的工程意义：OPC/SMO 中大量窗口高频调用时，总延迟和能耗收益会累积。

### 4. PCIe 与系统级口径

- [x] 区分 XDMA/PCIe 接口配置和主机实测协商链路：接口按 PCIe Gen3 x8 描述，最新 lspci/板上报告已恢复为 Gen3 x8，LaneErrStat 为 0。
- [x] 在实验平台表、PCIe 延迟表和正文中保持 Gen3 x8 说法完全一致。
- [x] 解释表中 PCIe H2C/C2H 吞吐是基于本次 Gen3 x8 协商链路的全平台实测结果，不等同于理论上限。
- [x] 明确 66.89 ms 是单窗口 PCIe on-board validation/profile path，不作为与 C++ SOCS 纯计算阶段计算加速比的口径。
- [x] 将“单窗口路径慢”的叙事改为 decoupled profiling / end-to-end transparency：66.89 ms 用于剖析 DMA、AXI-Lite、硬件计算、Host FI 和轮询开销，而不是作为失败或遮掩系统性能的表述。
- [x] 增加批量窗口摊薄延迟模型，说明配置、本征核和权重可复用，系统吞吐由 kernel、stream 或 Host FI 中最慢阶段决定。
- [x] 讨论 batch processing / spatial pipelining 场景下通过 DMA 双缓冲、FI 下沉和流水线重叠隐藏 PCIe 与主机开销。
- [x] 已重新协商到 Gen3 x8，并补充最新 H2C/C2H 和单窗口路径数据。
- [x] 已记录 x2 降级原因和恢复方式：PCIe 插槽接触不良导致 lane training 失败，重新插拔后恢复 x8。

## P1 强烈建议：提升论文完整度

### 5. 架构图美化

- [ ] 将当前图 2 改为白底 SCI 风格，不再使用黑底。
- [ ] 将图 2 拆成多子图：
  - [ ] (a) CPU-FPGA co-design overview。
  - [ ] (b) SOCS HLS IP online pipeline。
  - [ ] (c) AXI-MM / DDR memory organization。
- [ ] 在图中明确标注 Offline / Online 边界。
- [ ] 用实线箭头表示 AXI-MM burst 数据流，用虚线箭头表示 AXI-Lite control。
- [ ] 在 FPGA pipeline 中画出：
  - [ ] Kernel embedding, $17 \times 17$ -> $128 \times 128$。
  - [ ] Complex multiplication。
  - [ ] $128 \times 128$ 2D IFFT。
  - [ ] Block floating scaling。
  - [ ] Magnitude square。
  - [ ] sigma_k weighting。
  - [ ] BRAM accumulation。
  - [ ] FFTshift and output writeback。
- [ ] 将图中文字统一为英文，中文解释放入图注。
- [ ] 统一字体、线宽和配色：
  - [ ] CPU 浅蓝。
  - [ ] DDR 浅灰。
  - [ ] FPGA 浅绿或浅青。
  - [ ] Pipeline 浅黄或浅橙。
  - [ ] Control path 灰色虚线。
- [ ] 导出 600 dpi PNG 或矢量 PDF/SVG，确保缩放后文字仍清晰。

### 6. 图表与表格统一

- [x] 检查所有图号、表号和正文引用是否一致。
- [ ] 检查图 3、图 4 的 mask / SOCS / TCC / error heatmap 标注是否清晰。
- [ ] 图 3 或图 4 中若有色条，统一 colorbar 范围、字体和单位。
- [ ] 检查图 5 延迟分解是否与表 8 数值完全一致。
- [ ] 检查图 6 性能与资源总结是否与表格数据一致。
- [ ] 合并或精简重复的资源/性能对比表，避免表 10、表 12、表 13 信息重复。
- [ ] 所有表格统一有效数字格式：
  - [ ] latency 使用 ms。
  - [ ] cycles 使用整数。
  - [ ] RMSE 使用科学计数法。
  - [ ] resource utilization 使用百分比。
- [ ] 每个 caption 写清实验条件和口径，例如 “kernel-only latency at 250 MHz” 或 “PCIe path measured under Gen3 x8 negotiated link”。
- [ ] 推荐最终图表组织：
  - [ ] Fig. 1: TCC-SOCS workflow。
  - [ ] Fig. 2: CPU-FPGA/XDMA co-design overview。
  - [ ] Fig. 3: FPGA online reconstruction pipeline。
  - [ ] Fig. 4: full TCC / SOCS / FPGA output / error map。
  - [ ] Fig. 5: latency breakdown。
  - [ ] Fig. 6: resource and performance summary。
  - [ ] Table 1: task partition。
  - [ ] Table 2: XDMA data layout。
  - [ ] Table 3: HLS/IP implementation configuration。
  - [ ] Table 4: experimental platform。
  - [ ] Table 5: error taxonomy。
  - [ ] Table 6: SOCS truncation trade-off。
  - [ ] Table 7: ICCAD case statistics。
  - [ ] Table 8: kernel-only latency breakdown。
  - [ ] Table 9: PCIe single-window board validation path。
  - [ ] Table 10: software vs. FPGA comparison。
  - [ ] Table 11: resource utilization。
  - [ ] Table 12: energy efficiency。
  - [ ] Table 13: platform positioning。

### 7. 方法节细化

- [ ] 规范 Hopkins / TCC / SOCS 公式的符号定义。
- [x] 为核心公式添加编号和交叉引用：TCC、SOCS 分解、online reconstruction、RMSE/PSNR/SSIM。
- [ ] 确保 $TCC$、$K_k$、$\sigma_k$、$M(f_x,f_y)$、$I(x,y)$ 等符号全文一致。
- [x] 增加在线重构伪代码或流程框，展示 k-loop、embedding、IFFT、weighted accumulation。
- [x] 补充 HLS 关键实现细节：
  - [x] 关键 loop pipeline。
  - [x] array partition / BRAM buffer。
  - [x] AXI-MM bundle 划分。
  - [x] FFT IP 配置。
  - [x] block floating scaling 设置。
- [x] 明确 $17 \times 17$ kernel 尺寸由默认 DUV 参数推导得到。
- [x] 说明固定 $128 \times 128$ FFT 网格的选择理由和限制。
- [x] 在表 2 或方法节中显式写明所有输入/输出的数据类型为 IEEE-754 单精度浮点，并区分 real/imag 数据布局。
- [x] 将关键 HLS pragma 和 FFT IP 配置整理为表格或附录，避免只在文字中描述。
- [x] 将“基于 HLS FFT IP”的工程表述升级为“面向 DUV 截止频率的面积优化二维复数 IFFT 处理器”。
- [x] 强化块浮点动态范围补偿和冲突规避片上矩阵转置存储的架构描述。
- [x] 增加 `ap_fixed<32,1>` 的量化语义：32 位二进制补码定点、1 位整数宽度、31 位小数精度、量化步长约 $2^{-31}$。
- [x] 将外部 DDR 数据格式统一写为 IEEE-754 单精度浮点，避免使用 `float32` 代码式表述。
- [x] 将网格和尺寸统一为 LaTeX `\times` 写法，例如 $17 \times 17$、$128 \times 128$、$1024 \times 1024$。
- [x] 将关键术语更新为更正式的领域表述：mask spectrum / diffracted-order map、magnitude squared operation、area-optimized 2D IFFT processor、conflict-free transposition memory、memory access contention / bus congestion、on-board validation。
- [ ] 若目标期刊偏体系结构，进一步给出转置缓冲 bank/port 访问图或冲突规避时序表。

### 8. 误差分析

- [x] 区分三类误差来源：
  - [x] SOCS 低秩截断误差。
  - [x] HLS/FFT 块浮点缩放误差。
  - [x] 板上数据搬运或格式转换误差。
- [x] 将误差表重构为四类验证口径：
  - [x] C simulation vs. floating-point SOCS。
  - [x] C/RTL co-simulation vs. double-precision CPU SOCS reference。
  - [x] PCIe board $128 \times 128$ output vs. software reference image。
  - [x] PCIe+Host FI $1024 \times 1024$ output vs. SOCS reference。
- [x] 在实验节中明确说明硬件误差远小于 SOCS 截断误差。
- [x] 对 C sim、C/RTL cosim、board validation 的 RMSE、PSNR、SSIM 口径做统一说明。
- [ ] 讨论误差对下游 OPC/SMO 迭代的潜在影响。

### 9. 英文 SCI 风格与术语统一

- [ ] 全文统一使用 “aerial image reconstruction” 或 “online reconstruction”，避免 reconstruction / simulation / imaging 混用。
- [ ] 统一 “SOCS eigenkernel” 或 “coherent kernel”，避免 kernel、eigen-kernel、eigenkernel 在同一语境混用。
- [ ] 统一 “kernel-only latency” 与 “PCIe single-window board validation path” 两种性能口径。
- [ ] 所有倍数统一为 `3.37x` 或 `3.37\\times`，整篇保持一种格式。
- [x] 将 “显著”“反复调用”“仍受效率约束”“直接带来”“保守频率”等主观或口语化表达替换为可量化或工程化表述。
- [x] 将 250 MHz 叙事从“保守降频/负时序裕量”改为 PVT 鲁棒约束和可部署硬件时序收敛口径。
- [ ] 方法描述可使用现在时，实验实现和验证结果使用过去时或被动语态，整篇时态保持一致。
- [ ] 长句拆分，尤其是摘要、引言最后一段、实验结果段和局限性段。
- [x] 将正文和表格中的代码变量名替换为学术符号或描述性名称，例如 $I_{128\times128}$、$M_{\mathrm{Re}}$、$\Phi_{\mathrm{Re}}$。
- [x] 将实验脚本式参考对象名称替换为双精度 CPU 参考实现或软件参考图像。
- [ ] 最终英文稿中统一使用 “hardware-software co-design” 和 “energy-efficient acceleration” 等检索友好的关键词。

## P2 可选增强：时间允许时补充

### 10. 敏感性分析

- [ ] 补充不同 SOCS kernel 数量下的延迟、精度和资源趋势。
- [x] 若已有 50/400 kernel 数据，整理为 trade-off 曲线或表格。
- [ ] 补充不同 FFT 网格大小的讨论或实验：
  - [ ] $64 \times 64$。
  - [ ] $128 \times 128$。
  - [ ] $256 \times 256$。
- [ ] 若条件允许，补充不同光源配置：
  - [ ] Annular。
  - [ ] Dipole。
  - [ ] Quasar。
- [ ] 若条件允许，补充 High-NA EUV 参数下的可扩展性分析。
- [x] 对 10 个 ICCAD 测试用例补充平均值、标准差、最小值和最大值，避免只展示逐项表格。
- [ ] 若不能补充完整敏感性实验，在 Discussion 中明确哪些结果是当前配置下的结论，哪些属于未来扩展。

### 11. 可重复性材料

- [x] 增加实验环境表：
  - [x] FPGA 型号。
  - [x] HLS/Vivado 版本。
  - [x] 时钟频率。
  - [x] CPU 型号。
  - [x] 编译器版本。
  - [x] 操作系统。
- [x] 增加数据格式说明：
  - [x] input mask spectrum。
  - [x] kernel real/imag。
  - [x] scale weights。
  - [x] tmp image。
  - [x] output image。
- [ ] 如果允许公开代码，补充 repository 链接。
- [ ] 如果不能公开代码，补充伪代码和关键参数表。
- [ ] 准备 supplementary material：
  - [ ] HLS pragma 摘要。
  - [ ] XDMA 地址映射和运行命令。
  - [ ] 测试数据生成流程。
  - [ ] 更多 mask case 的精度结果。
  - [ ] FFT IP 和 Vivado 工程关键配置。

### 12. 投稿材料

- [ ] 确定目标期刊：
  - [ ] IEEE TCAD。
  - [ ] ACM TODAES。
  - [ ] JM3 / J. Micro/Nanopatterning, Materials, and Metrology。
  - [ ] IEEE Transactions on Semiconductor Manufacturing。
  - [ ] Optics Express。
  - [ ] ACM TRETS。
  - [ ] Microelectronics Journal。
- [ ] 按目标期刊模板调整格式。
- [ ] 检查摘要字数、关键词数量、图表数量和参考文献格式。
- [ ] 补充 Funding。
- [ ] 补充 Conflict of Interest。
- [ ] 补充 Data Availability。
- [ ] 补充 Author Contributions。
- [ ] 使用 iThenticate 或其他工具检查重复率。
- [ ] 全文进行英文润色或母语级校对。
- [ ] 准备 cover letter，重点写清 novelty、区别于已有 GPU/FPGA lithography acceleration 的地方、板上验证和可复现性亮点。

## 逐章修改核对

### Abstract

- [x] 按“问题-方法-结果-意义”压缩摘要。
- [ ] 英文版摘要控制在 250-300 words 以内。
- [x] 将关键量化结果前移：10.57 ms、3.37x、67.4x、RMSE、资源占用。
- [x] 弱化主观表述，尽量用数据支撑结论。
- [x] 摘要中删去 PCIe 协商细节，仅保留 Host-FPGA 单窗口板上验证路径；链路协商口径放在正文实验平台和 PCIe 分析中说明。
- [x] 摘要最后一句强调 “CPU-FPGA co-design with end-to-end on-board verification”，强化系统级贡献。
- [x] 关键词增加或检查：
  - [x] computational lithography。
  - [x] TCC-SOCS。
  - [x] aerial image simulation。
  - [x] FPGA。
  - [x] HLS。
  - [x] hardware-software co-design。
  - [x] energy-efficient acceleration。
- [x] 删除摘要/关键词中过细的 “2D IFFT” 关键词，将其放在方法节作为实现细节。

### Introduction / Related Work

- [x] 第一段明确计算光刻中空中像计算的瓶颈。
- [x] 第二段说明 TCC-SOCS 的准确性和在线计算负担。
- [x] 第三段引出 FPGA 的低延迟、低功耗和确定性优势。
- [x] 第四段明确现有工作的不足和本文 gap。
- [x] 将应用场景定位为批量掩模窗口流水线处理、边缘式线宽验证、低功耗确定性在线内核，避免与 full-chip GPU 集群拼绝对单帧吞吐。
- [x] 强化 GPU 局限性表述：数百瓦级功耗、主机调度和非确定性中断延迟限制紧凑集成。
- [x] 贡献列表保持 3-4 条，不要过长。
- [ ] 贡献列表与后续章节一一对应，避免 contribution 和 experimental validation 重复。
- [ ] Related Work 减少与 Introduction 重复的内容。
- [ ] 更新 2025-2026 年 GPU/AI lithography acceleration 文献。

### Method

- [ ] 公式排版规范。
- [ ] 所有公式、变量和表格在英文版中可直接迁移到 LaTeX。
- [ ] 架构图更新为 SCI 风格。
- [x] 增加伪代码或时序图。
- [x] 补充 HLS pragma、FFT IP 和 AXI bundle 细节。
- [x] 说明资源优化设计选择的原因。
- [ ] 补充可扩展性推导：更大 kernel 数、FFT 网格或并行度主要受 BRAM、DSP、DDR 带宽还是 PCIe 传输限制。

### Experiments

- [x] 实验环境描述完整。
- [x] baseline 描述完整。
- [x] 功耗估计或实测方法描述完整。
- [x] 延迟、资源、精度、能效指标口径统一。
- [ ] 增加 resource breakdown：FFT IP、embedding、accumulation、AXI/control、BRAM buffer 分别占用资源。
- [ ] 增加 bandwidth utilization 分析：H2C 写入、C2H 回读、AXI-Lite 配置、Host FI 哪些是真正瓶颈。
- [x] 增加 10 个 ICCAD case 的 mean/std/min/max 汇总。
- [x] 增加 amortized latency / streaming throughput 分析，避免 66.89 ms 被误读为 FPGA 内核加速比。
- [x] 明确功耗为 kernel-level estimate，非 Host+PCIe+DDR 全平台实测功耗。
- [ ] 补充 Vivado Power Analyzer 细节：工具版本、时钟、结温/电压、toggle rate 或 SAIF/VCD 活动文件来源。
- [ ] 若能补跑，给出板上 idle/running/dynamic delta 功耗，替换或校准 4 W 估算值。
- [ ] 图表数据互相一致。
- [x] 增加实验结果的解释，而不是只罗列数值。

### Discussion / Limitations

- [x] 单独讨论当前设计的限制：
  - [x] kernel 间采用时分复用。
  - [x] FFT 网格固定为 $128 \times 128$。
  - [x] 已纳入 PCIe 单窗口板上验证路径延迟，并区分 kernel-only 与 PCIe+Host FI 执行路径。
  - [x] 功耗可能是估算值。
  - [x] 光源和工艺条件覆盖仍有限。
- [x] 说明当前 PCIe on-board profile result 是单窗口验证路径；工业批量场景需依靠配置复用、双缓冲 DMA、多板并行或 FI 下沉提升系统吞吐。
- [x] 说明这些限制为什么不影响本文核心结论。
- [x] 给出后续扩展方向：
  - [x] 更大 BRAM 器件上的 kernel 并行。
  - [x] 自适应 FFT 网格。
  - [x] 集成插值和后处理。
  - [x] 扩展至 Dipole、Quasar、High-NA EUV。
- [ ] 补充未来工作更具体方向：集成到 OPC/SMO 迭代闭环、动态 kernel 并行度、FPGA 端 FI 后处理、中断替代轮询、批量窗口传输优化。

### Conclusion

- [x] 用 1 段总结方法。
- [x] 用 1 段总结关键结果。
- [x] 用 1 段说明应用价值和未来方向。
- [ ] 避免重复摘要中的全部数字。
- [x] 结论强调边界：当前是 online reconstruction kernel + PCIe on-board profile path，不声称完整 OPC/SMO 工具链加速。

## 最终提交前检查

- [ ] 全文术语统一：TCC-SOCS、SOCS eigen-kernel、aerial image reconstruction、online reconstruction。
- [ ] 全文单位统一：MHz、ms、W、J/image、RMSE、SSIM、PSNR。
- [ ] 全文数字格式统一：10.57 ms、3.37x、67.4x、$2.93 \times 10^{-8}$。
- [ ] 全文性能口径统一：kernel-only、PCIe single-window board validation path、Host FI、estimated power 不混用。
- [x] 全文 PCIe 口径统一：Gen3 x8 为接口配置和最新主机实测协商结果。
- [ ] 全文图表标题和正文引用一致。
- [ ] 所有缩写首次出现时给出全称。
- [ ] 所有参考文献在正文中被引用。
- [ ] 所有正文引用在参考文献列表中存在。
- [ ] 数学符号没有前后冲突。
- [ ] 图像分辨率满足期刊要求。
- [ ] 表格不超页宽。
- [ ] PDF 编译无 warning 或只保留无害 warning。
- [ ] 投稿系统所需文件准备完毕：
  - [ ] manuscript。
  - [ ] figures。
  - [ ] cover letter。
  - [ ] highlights。
  - [ ] graphical abstract，如期刊要求。
  - [ ] supplementary material，如有。
