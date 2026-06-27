# 面向 TCC-SOCS 空中像计算的低延迟高能效 FPGA/HLS 加速架构

## 亮点

- 提出一种面向计算光刻 TCC-SOCS 在线空中像重构的 CPU-FPGA 协同加速框架。
- 将离线 TCC 构建与 SOCS 本征核提取过程同高频调用的在线重构过程解耦。
- 基于 HLS FFT IP、块浮点缩放、LUT 映射乘法和 BRAM 缓冲实现资源高效的 128 x 128 二维 IFFT 流水线。
- 采用 7 个独立 AXI-MM 存储接口，降低掩模频谱、SOCS 本征核、权重和输出缓冲之间的数据访问竞争。
- 所提出设计在 250 MHz 下实现 10.57 ms 延迟，相较 C++ SOCS 基准实现 3.37 倍加速，能效提升约 67.4 倍。

## 摘要

TCC-SOCS 空中像计算是 OPC/SMO 等计算光刻流程中的关键环节，其在线重构阶段会被反复调用，在传统 CPU/GPU 平台上仍面临延迟、功耗与能效之间的权衡。不同于已有工作多依赖通用处理器或 GPU 加速，本文提出一种面向 TCC-SOCS 在线重构的 FPGA/HLS 加速架构，用以验证可重构硬件在光刻计算中的可行性。该架构采用 CPU-FPGA 协同设计：CPU 负责离线 TCC 构建、特征分解与 SOCS 本征核提取，FPGA 负责频域嵌入、128 x 128 二维 IFFT、加权强度累加和结果写回。通过 HLS FFT IP、块浮点缩放、多端口 AXI-MM 访问和 BRAM 缓冲，所提出设计在 Xilinx Kintex UltraScale+ xcku5p 上以 250 MHz 实现 10.57 ms 的 10 核 SOCS 重构延迟，相较 MATLAB 和 C++ 基准分别实现 45.3 倍和 3.37 倍加速；C/RTL 联合仿真 RMSE 为 $8.324 \times 10^{-7}$，板级验证 RMSE 为 $2.93 \times 10^{-8}$。资源占用为 17% LUT、9% FF、2% DSP 和 42% BRAM，能效较 CPU 基准提升约 67.4 倍。实验结果表明，FPGA 可作为 CPU/GPU 之外的有效光刻计算加速平台，为低延迟、低功耗空中像仿真提供了可部署路径。

**关键词：**计算光刻；TCC-SOCS；空中像计算；FPGA；HLS；二维 IFFT；能效。

## 1. 引言

计算光刻已成为先进半导体制造中连接光学成像、工艺窗口和版图优化的关键技术。随着工艺节点持续缩小，单纯依靠投影光学系统提升分辨率愈发困难，光学邻近效应校正（OPC）、源掩模协同优化（SMO）和反向光刻技术（ILT）等方法被广泛用于补偿邻近效应、优化照明与掩模形状并扩大可制造窗口 [1]-[11]。这些方法通常以模型反馈或反问题优化的形式工作，需要在大量版图窗口、多种工艺条件和多轮迭代中反复调用空中像计算 [2], [3], [5], [8]。当工艺窗口缩小、掩模复杂度提高且优化变量从边缘移动扩展到像素化或曲线图形时，空中像计算的延迟、吞吐和能耗逐渐成为计算光刻工程部署中的核心瓶颈。

部分相干光刻成像通常以 Hopkins 理论为基础，其传输交叉系数（TCC）可将照明光源、投影光瞳、像差、薄膜效应以及掩模频谱耦合统一表示为频域算子 [12]-[18]。在光学参数固定时，TCC 可离线预计算并在不同掩模图形间复用；然而，直接 TCC 成像涉及频率对之间的密集耦合，计算和存储开销较高。Sum of coherent systems（SOCS）方法通过对 TCC 进行特征分解或低秩近似，将部分相干成像转化为多个相干本征核的加权求和，从而在保持物理模型解释性的同时降低在线计算复杂度 [12], [13], [16], [19]。因此，TCC-SOCS 已成为空中像计算中兼顾准确性和效率的重要技术路线。

尽管 SOCS 显著降低了直接 TCC 计算的复杂度，离线分解之后的在线重构阶段仍是高频计算热点。对于每个 SOCS 本征核，在线重构都需要执行掩模频谱与本征核相乘、2D IFFT、模平方、特征值加权以及多核强度累加；这些操作会随本征核数量、掩模窗口数量和 OPC/SMO/ILT 迭代次数重复放大 [19]-[23]。已有工作从数值近似、对称性利用、GPU 加速和学习型模型等方向提升光刻仿真或掩模优化效率 [20], [22]-[30]。CPU 平台具有较高灵活性，但在规则频域运算下能效有限；GPU 在大批量吞吐场景中优势明显，但在小批量、低延迟、功耗受限或需要确定性执行的场景中，数据搬移和系统功耗仍可能带来约束。

FPGA 为 TCC-SOCS 在线重构提供了 CPU/GPU 之外的补充方案。复数乘法、二维 IFFT、模平方和累加均具有规则数据通路和清晰依赖关系，适合映射为定制流水线；片上 BRAM 可缓存中间矩阵以减少 DDR 往返访问，多 AXI-MM 接口也可缓解掩模频谱、SOCS 本征核、权重和输出缓冲之间的带宽竞争。已有研究验证了 FPGA 在光刻空中像仿真和二维 FFT 加速中的潜力，并指出外部存储访问、行列转置、并行度和能效之间的权衡对 FFT 类硬件加速器至关重要 [31]-[38]。然而，现有计算光刻加速工作更多关注算法近似、CPU/GPU 实现、学习型 OPC/ILT 或非 TCC-SOCS 的 FPGA 光刻仿真；如何在有限 FPGA 资源下为 TCC-SOCS 在线空中像重构系统平衡延迟、带宽、资源和精度，仍缺少端到端架构设计和板级验证。

本文聚焦于离线 TCC 构建和 SOCS 本征核提取之后的在线重构阶段，而非加速完整 TCC 生成、特征分解、完整 OPC/SMO 系统或 Host-FPGA 全链路传输。本文提出一种 CPU-FPGA 协同架构：CPU 负责离线 TCC 构建、特征分解和 SOCS 本征核生成，FPGA 负责在线频域嵌入、128 x 128 二维 IFFT、加权累加、FFTshift 和结果写回。主要贡献包括：提出面向在线 TCC-SOCS 重构的协同计算框架；基于 HLS FFT IP、块浮点缩放和 LUT 映射乘法设计资源高效流水线；采用 7 个 AXI-MM 接口和 BRAM 缓冲优化存储访问；在 Kintex UltraScale+ xcku5p 上完成 C 仿真、C/RTL 联合仿真和板级验证。实验表明，10 核 SOCS 在线重构在 250 MHz 下延迟为 10.57 ms，相较 C++ SOCS 基准加速 3.37 倍，能效提升约 67.4 倍，C/RTL RMSE 为 $8.324 \times 10^{-7}$，板级 RMSE 为 $2.93 \times 10^{-8}$，资源占用为 17% LUT、9% FF、2% DSP 和 42% BRAM。本文其余部分组织如下：第 2 节介绍 TCC-SOCS 模型与所提出架构，第 3 节给出实验结果，第 4 节总结全文。

## 2. 原理与方法

### 2.1 Hopkins TCC 成像模型

对于部分相干光刻成像，Hopkins 公式可将空中像强度表示为掩模频谱的双线性形式。传输交叉系数用于编码照明与投影光学系统：

$$
TCC(f',g';f'',g'') =
\iint S(f_s,g_s)P(f'+f_s,g'+g_s)P^*(f''+f_s,g''+g_s)df_sdg_s,
$$

其中，$S$ 表示光源分布，$P$ 表示光瞳函数。在光学参数固定时，TCC 可被预计算。对应的图像强度可写为：

$$
I(x,y)=
\iiiint TCC(f',g';f'',g'')\hat{O}(f',g')\hat{O}^*(f'',g'')
e^{j2\pi[(f'-f'')x+(g'-g'')y]}df'dg'df''dg'',
$$

其中，$\hat{O}$ 为掩模频谱。该直接形式精度较高，但由于涉及频率对之间的密集耦合，计算代价较大。

### 2.2 SOCS 分解与问题定义

TCC 矩阵具有 Hermitian 性质，并且通常可由低秩分解近似表示：

$$
TCC \approx \sum_{k=1}^{N_k}\sigma_k\Phi_k\Phi_k^*,
$$

其中，$\sigma_k$ 为第 $k$ 个特征值权重，$\Phi_k$ 为对应的频域相干本征核，$N_k$ 为保留的 SOCS 本征核数量。将该分解代入 Hopkins 方程可得：

$$
I(x,y) \approx
\sum_{k=1}^{N_k}\sigma_k
\left|\mathcal{F}^{-1}\{M(f_x,f_y)\Phi_k(f_x,f_y)\}\right|^2,
$$

其中，$M(f_x,f_y)$ 表示掩模频谱。因此，每个本征核对应的在线计算包括频域乘法、IFFT、模平方计算和加权累加。

本文中，离线阶段由 CPU 计算 SOCS 本征核及其特征值权重。FPGA 负责如下在线映射：

$$
\hat{I}=f_{\mathrm{FPGA}}(M,\{\Phi_k,\sigma_k\}_{k=1}^{N_k}),
$$

其中，$\hat{I}$ 为重构得到的空中像。优化目标是在相对软件 SOCS 参考结果保持较小硬件实现误差的前提下，降低在线计算延迟和能耗。

### 2.3 所提出的 CPU-FPGA 框架

所提出框架将光学系统预处理与在线重构分离。CPU 负责配置解析、光源生成、掩模 FFT、TCC 构建、特征分解和数据格式化。生成的掩模频谱、SOCS 本征核和特征值权重被存储在外部 DDR 中。FPGA 通过 AXI-MM 接口读取这些数据并执行在线重构。

这种划分方式源于两个阶段不同的执行特征。TCC 构建和特征分解需要高精度矩阵运算，但仅在光学参数变化时执行；相比之下，在线 SOCS 重构会针对不同掩模窗口重复执行，并且主要由规则的 FFT 和累加操作构成。因此，将在线阶段迁移到 FPGA 可直接带来延迟和能效收益，同时保留 CPU 在离线预处理方面的灵活性。

![图 1. TCC-SOCS 计算流程。](../image/论文/ch3_fig1_hopkins_workflow.png)

**图 1.** TCC-SOCS 计算流程。离线光学系统处理生成 SOCS 本征核和特征值权重，在线重构阶段针对掩模窗口计算空中像。

### 2.4 FPGA 在线重构流水线

FPGA 流水线包含五个阶段：

1. **频域嵌入：**将 SOCS 本征核与对应的掩模频谱中心窗口相乘，并将结果嵌入固定的 128 x 128 FFT 网格。
2. **二维 IFFT：**基于 HLS FFT IP 依次执行行向 FFT、矩阵转置和列向 FFT。
3. **加权累加：**计算 $|E_k|^2$，并将 $\sigma_k|E_k|^2$ 累加至临时图像缓冲区。
4. **FFTshift：**通过象限交换将零频分量移动至图像中心。
5. **输出写回：**通过 AXI-MM burst 访问将最终 128 x 128 图像写回 DDR。

对于默认 10 核配置，上述五阶段路径针对每个本征核顺序执行。该核间时分复用策略在多个本征核之间复用同一套二维 IFFT 引擎，从而避免过高的 BRAM 消耗。

![图 2. 所提出的 FPGA 加速架构。](../image/论文/ch4_fig1_fpga_architecture.png)

**图 2.** 面向在线 TCC-SOCS 空中像重构的 FPGA 加速架构。

### 2.5 频域嵌入与复数乘法

对于默认配置，有效本征核尺寸为 17 x 17，对应 $N_x=N_y=8$。每个本征核需要完成 289 次掩模频谱与 SOCS 本征核之间的复数乘法。嵌入模块将乘法结果映射至 128 x 128 频率网格中心，其余位置补零。复数乘法定义为：

$$
(a+jb)(c+jd)=(ac-bd)+j(ad+bc).
$$

HLS 实现对该循环进行流水线化，启动间隔接近 1。运行时参数定义实际有效本征核区域，编译时常量限定循环边界，以提高 HLS 调度的确定性。

### 2.6 二维 IFFT 引擎

二维 IFFT 是主要计算阶段。本文使用 Xilinx HLS FFT IP 并基于行列分解实现。输入的 128 x 128 矩阵先逐行处理并存入中间 BRAM 缓冲区，再逐列处理。双端口 BRAM 缓冲区用于支持转置访问模式。

FFT IP 配置为 128 点变换长度、自然序输出、块浮点缩放以及 LUT 映射乘法。块浮点缩放仅在存在溢出风险时动态移位，并通过 `blk_exp` 输出累计指数。最终转换阶段对该指数进行补偿。与固定逐级缩放相比，该策略在 $\log_2 128=7$ 的奇数 FFT 深度下能够保留更多有效精度。

### 2.7 存储架构

顶层 HLS IP 使用 7 个独立 AXI-MM master 接口：

| 接口  | 缓冲区       | 数据            |      深度 | 访问方式 |
| ----- | ------------ | --------------- | --------: | -------- |
| gmem0 | `mskf_r`     | 掩模频谱实部    | 1,048,576 | 读       |
| gmem1 | `mskf_i`     | 掩模频谱虚部    | 1,048,576 | 读       |
| gmem2 | `scales`     | SOCS 特征值权重 |        32 | 读       |
| gmem3 | `krn_r`      | 本征核实部      |    76,832 | 读       |
| gmem4 | `krn_i`      | 本征核虚部      |    76,832 | 读       |
| gmem5 | `tmpImg_ddr` | 中间图像        |    16,384 | 写       |
| gmem6 | `output`     | 最终图像        |    16,384 | 写       |

四个主要片上缓冲区被绑定至 BRAM：`fft_input`、`fft_output`、`tmpImg` 和 `tmpImgp`。这减少了 FFT 与累加过程中的重复 DDR 访问。该存储设计将大规模流式输入、小规模标量参数和输出缓冲区分离，从而降低总线竞争并提升 burst 访问效率。

## 3. 仿真与实验

### 3.1 实验设置

所提出设计使用 Vitis HLS 2025.2 和 Vivado 2025.2 进行评估。目标 FPGA 为 Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e。默认光学配置为 $L_x=L_y=1024$、$NA=0.8$、$\lambda=193$ nm、环形照明、$\sigma_{in}=0.6$、$\sigma_{out}=0.9$，SOCS 本征核数量为 10。在线 FFT 网格固定为 128 x 128。

CPU 基准平台使用 Intel Xeon Platinum 8163 服务器。MATLAB 和 C++ 实现被用作软件基准。MATLAB 提供 Golden Model 以及 TCC/SOCS 直接参考结果；C++ 实现提供更优化的单精度 CPU 对比。

| 类别       | 配置                                                                                        |
| ---------- | ------------------------------------------------------------------------------------------- |
| FPGA       | Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e                                                |
| FPGA 资源  | 960 BRAM_18K，1,824 DSP，433,920 FF，216,960 LUT                                            |
| HLS/Vivado | Vitis HLS 2025.2 / Vivado 2025.2                                                            |
| 频率       | 250 MHz 保守评估                                                                            |
| CPU 基准   | Intel Xeon Platinum 8163 @ 2.50 GHz，48 核 / 96 线程，93 GB DDR4                            |
| 光学配置   | $L_x=L_y=1024$，$NA=0.8$，$\lambda=193$ nm，环形光源，$\sigma_{in}=0.6$，$\sigma_{out}=0.9$ |
| SOCS 阶数  | 默认 10 核；敏感性分析使用 50 核和 400 核                                                   |
| FFT 网格   | 128 x 128                                                                                   |

### 3.2 精度验证

本文从三个层级验证设计正确性：C 仿真、C/RTL 联合仿真和板级验证。C 仿真确认算法级正确性，C/RTL 联合仿真验证生成 RTL 的时序行为和 AXI 事务，板级验证进一步确认部署后硬件输出的正确性。

| 验证方式       |                   RMSE | 说明           |
| -------------- | ---------------------: | -------------- |
| C 仿真         |  $2.93 \times 10^{-8}$ | HLS 算法级结果 |
| C/RTL 联合仿真 | $8.324 \times 10^{-7}$ | RTL 级结果     |
| 板级验证       |  $2.93 \times 10^{-8}$ | 硬件输出       |

误差主要来自定点量化、块浮点缩放以及 FFT 蝶形运算舍入。所得 RMSE 均低于 $10^{-5}$，说明在所测试的 SOCS 重构流程中，硬件实现误差足够小。

需要区分硬件实现误差和 SOCS 截断误差。硬件实现误差比较 FPGA 输出与同一 SOCS 软件参考结果；SOCS 截断误差比较有限核 SOCS 结果与完整 TCC 直接成像结果，后者取决于保留的本征核数量。

| SOCS 核数量 |   相对完整 TCC 的 RMSE | 解释                 |
| ----------: | ---------------------: | -------------------- |
|          10 | $5.474 \times 10^{-3}$ | 低延迟，中等截断误差 |
|          50 | $0.927 \times 10^{-3}$ | 精度更高，延迟增加   |
|         400 |  $2.57 \times 10^{-6}$ | 接近满秩参考结果     |

默认选择 10 核配置，是因为其在延迟与精度之间提供了较实用的平衡。视觉对比也表明，FPGA 输出能够保持主要空中像结构，剩余误差主要分布在掩模高频边缘附近。

![图 3. 参考结果与 FPGA 输出的视觉对比。](../image/论文/ch5_fig4_visual_comparison.png)

**图 3.** 参考空中像、FPGA 输出和误差分布的视觉对比。

### 3.3 运行时间与延迟分解

在 250 MHz 下，HLS 估计延迟为 2,643,645 个周期，对应 10.57 ms。C/RTL 联合仿真报告为 2,651,856 个周期，与综合估计接近。二者之间的小幅差异主要来自协议与调度开销。

| 阶段             |        周期数 | 250 MHz 下时间 |     占比 |
| ---------------- | ------------: | -------------: | -------: |
| 频域嵌入，10 核  |       167,450 |        0.67 ms |     6.3% |
| 二维 IFFT，10 核 |     2,262,250 |        9.05 ms |    85.6% |
| 累加，10 核      |       164,250 |        0.66 ms |     6.2% |
| FFTshift         |        16,389 |       0.066 ms |     0.6% |
| DDR 输出         |        16,389 |       0.066 ms |     0.6% |
| **总计**         | **2,643,645** |   **10.57 ms** | **100%** |

二维 IFFT 占总延迟约 85.6%，说明 FFT 优化是主要性能杠杆。此处报告的延迟指 FPGA 在线重构阶段，不包含离线 TCC 构建、特征分解、主机侧预处理或全分辨率傅里叶插值。

![图 4. 所提出 FPGA 流水线的延迟分解。](../image/论文/ch5_fig2_latency_breakdown.png)

**图 4.** 所提出 FPGA 在线重构流水线的延迟分解。

### 3.4 加速比分析

FPGA 运行时间与 MATLAB 和 C++ 基准对比如下：

| 基准                     |  运行时间 | FPGA 运行时间 |  加速比 |
| ------------------------ | --------: | ------------: | ------: |
| MATLAB 完整 TCC 直接成像 |    479 ms |      10.57 ms | 45.3 倍 |
| MATLAB 10 核 SOCS        |    287 ms |      10.57 ms | 27.1 倍 |
| C++ 完整 TCC 直接成像    | 45.176 ms |      10.57 ms | 4.28 倍 |
| C++ 10 核 SOCS           |   35.6 ms |      10.57 ms | 3.37 倍 |

与 C++ SOCS 的对比最为保守，因为二者比较的是相同的在线重构范围。尽管 3.37 倍加速不如 MATLAB 对比显著，但考虑到 OPC/SMO 流程会在大量版图窗口中反复调用空中像计算，该加速仍具有工程意义。

### 3.5 资源利用率

最终设计在 xcku5p 上的资源利用率如下：

| 资源     | 使用量 |  可用量 | 利用率 |
| -------- | -----: | ------: | -----: |
| LUT      | 36,931 | 216,960 |    17% |
| FF       | 38,703 | 433,920 |     9% |
| DSP      |     34 |   1,824 |     2% |
| BRAM_18K |    399 |     960 |    42% |

两项资源优化对该架构的可部署性至关重要。首先，使用 HLS FFT IP 替代直接 DFT，使 DSP 使用量从 8,064 个降低至 34 个，降幅为 99.6%。其次，对 SOCS 本征核采用时分复用并共享二维 IFFT 引擎，使 BRAM 使用量从 1,366 个降低至 399 个，降幅为 70.8%。这些优化使设计能够适配中等规模 Kintex UltraScale+ 器件。

![图 5. 性能与资源总结。](../image/论文/ch5_fig3_performance_resource.png)

**图 5.** 所提出 FPGA/HLS 架构的性能与资源总结。

### 3.6 能效分析

FPGA 功耗估计约为 4 W，包括静态功耗和动态功耗。C++ CPU 基准平台功耗估计为 65-80 W。基于 10.57 ms 的 FPGA 延迟，FPGA 吞吐量约为 94.6 frames/s。相比 C++ SOCS 在 80 W 下约 28.09 frames/s 的吞吐量，FPGA 能效提升约 67.4 倍。

| 平台         | 运行时间 |    功耗 |         吞吐量 |           能效 |
| ------------ | -------: | ------: | -------------: | -------------: |
| C++ CPU SOCS |  35.6 ms | 约 80 W | 28.09 frames/s | 0.351 frames/J |
| FPGA SOCS    | 10.57 ms |  约 4 W |  94.6 frames/s |  23.7 frames/J |

这一结果表明，所提出 FPGA 架构尤其适合能耗受限或延迟敏感的光刻仿真负载。

### 3.7 讨论与局限性

当前设计仍存在若干局限。首先，SOCS 本征核采用时分复用方式处理，而非完全核间并行；这一选择受 FFT IP 的 BRAM 占用约束。其次，FFT 网格固定为 128 x 128，对于当前 1024 x 1024 DUV 配置较为高效，但对于更小本征核可能存在资源浪费，对于更大的 High-NA EUV 配置则可能不足。第三，本文报告的 FPGA 延迟聚焦于在线重构核，不包含完整 Host-FPGA 数据传输、主机侧傅里叶插值或完整 OPC/SMO 系统集成。第四，功耗数据为估计值，后续需要通过后实现功耗分析和板级实测进一步修正。

尽管存在上述局限，本文结果表明，只要合理管理 FFT 资源消耗与存储带宽，TCC-SOCS 在线重构能够通过 HLS 高效映射到 FPGA 架构中。

## 4. 结论

本文提出一种面向 TCC-SOCS 空中像重构的低延迟高能效 FPGA/HLS 架构。所提出 CPU-FPGA 协同设计将 TCC 构建与 SOCS 本征核提取保留在 CPU 端，并将反复调用的在线重构阶段映射到 FPGA 上加速。硬件数据通路集成了频域嵌入、基于 HLS FFT IP 的二维 IFFT、加权累加、FFTshift 和 DDR 输出。

通过块浮点 FFT 缩放、LUT 映射 FFT 乘法、多端口 AXI-MM 访问和 BRAM 缓冲，该设计在保持较高数值精度的同时显著降低资源使用量。在 xcku5p FPGA 上，10 核配置在 250 MHz 下实现 10.57 ms 延迟，C/RTL RMSE 为 $8.324 \times 10^{-7}$，板级 RMSE 为 $2.93 \times 10^{-8}$。最终架构占用 17% LUT、9% FF、2% DSP 和 42% BRAM。相较 C++ CPU SOCS 基准，其实现 3.37 倍加速和约 67.4 倍能效提升。

未来工作将集中于在更大 BRAM 容量器件上提升核间并行度、支持自适应 FFT 网格尺寸、将傅里叶插值集成至 FPGA 流水线，并在更大规模 OPC/SMO 流程、更多光源类型和工业掩模图形上评估系统表现。



## 参考文献

1. Y. Granik, "Fast pixel-based mask optimization for inverse lithography," 2006, doi: 10.1117/1.2399537.
2. N. B. Cobb, A. Zakhor, and E. A. Miloslavsky, "Mathematical and CAD framework for proximity correction," 1996, doi: 10.1117/12.240907.
3. N. Jia and E. Y. Lam, "Pixelated source mask optimization for process robustness in optical lithography," 2011, doi: 10.1364/OE.19.019384.
4. Z. Li, L. Dong, X. Ma, and Y. Wei, "Fast source mask co-optimization method for high-NA EUV lithography," 2024, doi: 10.29026/OEA.2024.230235.
5. Y. Shen, N. Wong, and E. Y. Lam, "Level-set-based inverse lithography for photomask synthesis," 2009, doi: 10.1364/OE.17.023690.
6. D. S. Abrams and L. Pang, "Fast inverse lithography technology," 2006, doi: 10.1117/12.658876.
7. L. Pang, Y. Liu, and D. Abrams, "Inverse Lithography Technology (ILT): what is the impact to the photomask industry?," 2006, doi: 10.1117/12.681857.
8. J.-R. Gao, X. Xu, B. Yu, and D. Z. Pan, "MOSAIC: Mask optimizing solution with process window aware inverse correction," 2014, doi: 10.1145/2593069.2593163.
9. B.-G. Kim et al., "Trade-off between inverse lithography mask complexity and lithographic performance," 2009, doi: 10.1117/12.824299.
10. L. Pang, Y. Liu, and D. Abrams, "Inverse lithography technology (ILT): a natural solution for model-based SRAF at 45nm and 32nm," 2007, doi: 10.1117/12.729028.
11. C.-Y. Hung et al., "Pushing the lithography limit: applying inverse lithography technology (ILT) at the 65nm generation," 2006, doi: 10.1117/12.655728.
12. X. Ma and G. R. Arce, "Binary mask optimization for inverse lithography with partially coherent illumination," 2008, doi: 10.1364/JOSAA.25.002960.
13. X. Ma and G. R. Arce, "PSM design for inverse lithography with partially coherent illumination," 2008, doi: 10.1364/OE.16.020126.
14. M. Yeung, D. Lee, R. S. Lee, and A. R. Neureuther, "Extension of the Hopkins theory of partially coherent imaging to include thin-film interference effects," 1993, doi: 10.1117/12.150443.
15. K. Adam, Y. Granik, A. Torres, and N. B. Cobb, "Improved modeling performance with an adapted vectorial formulation of the Hopkins imaging equation," 2003, doi: 10.1117/12.485357.
16. P. Gong, S. Liu, W. Lv, and X. Zhou, "Fast aerial image simulations for partially coherent systems by transmission cross coefficient decomposition with analytical kernels," 2012, doi: 10.1116/1.4767442.
17. R. Koehle, "Fast TCC algorithm for the model building of high NA lithography simulation," 2005, doi: 10.1117/12.599591.
18. X. Wu, S. Liu, W. Liu, T. Zhou, and L. Wang, "Comparison of three TCC calculation algorithms for partially coherent imaging simulation," 2010, doi: 10.1117/12.885227.
19. P. Yu and D. Z. Pan, "ELIAS: An Accurate and Extensible Lithography Aerial Image Simulator With Improved Numerical Algorithms," 2009, doi: 10.1109/TSM.2009.2017652.
20. P. Yu, W. Qiu, and D. Z. Pan, "Fast Lithography Image Simulation By Exploiting Symmetries in Lithography Systems," 2008, doi: 10.1109/TSM.2008.2005380.
21. D. A. Bernard, J. Li, J. C. Rey, K. Rouz, and V. Axelrad, "Efficient computational techniques for aerial imaging simulation," 1996, doi: 10.1117/12.240963.
22. R. Rodrigues, A. Sreedhar, and S. Kundu, "Optical lithography simulation using wavelet transform," 2009, doi: 10.1109/ICCD.2009.5413120.
23. X. Zheng, X. Ma, Q. Zhao, Y. Pan, and G. R. Arce, "Model-informed deep learning for computational lithography with partially coherent illumination," 2020, doi: 10.1364/OE.413721.
24. I. Torunoglu et al., "OPC on a single desktop: a GPU-based OPC and verification tool for fabs and designers," 2010, doi: 10.1117/12.846636.
25. Z. Yu, G. Chen, Y. Ma, and B. Yu, "A GPU-Enabled Level-Set Method for Mask Optimization," 2022, doi: 10.1109/TCAD.2022.3175939.
26. H. Yang, S. Li, Z. Deng, Y. Ma, B. Yu, and E. F. Y. Young, "GAN-OPC: Mask Optimization With Lithography-Guided Generative Adversarial Nets," 2019, doi: 10.1109/TCAD.2019.2939329.
27. W. Ye, M. B. Alawieh, Y. Lin, and D. Z. Pan, "LithoGAN," 2019, doi: 10.1145/3316781.3317852.
28. B. Jiang, L. Liu, Y. Ma, B. Yu, and E. F. Y. Young, "Neural-ILT 2.0: Migrating ILT to Domain-Specific and Multitask-Enabled Neural Network," 2021, doi: 10.1109/TCAD.2021.3109556.
29. G. Chen, W. Chen, Q. Sun, Y. Ma, H. Yang, and B. Yu, "DAMO: Deep Agile Mask Optimization for Full-Chip Scale," 2021, doi: 10.1109/TCAD.2021.3116511.
30. H. Yang et al., "Generic lithography modeling with dual-band optics-inspired neural networks," 2022, doi: 10.1145/3489517.3530580.
31. J. Cong and Y. Zou, "FPGA-Based Hardware Acceleration of Lithographic Aerial Image Simulation," 2009, doi: 10.1145/1575774.1575776.
32. J. Cong and Y. Zou, "Lithographic aerial image simulation with FPGA-based hardware acceleration," 2008, doi: 10.1145/1344671.1344683.
33. B. Akin, P. Milder, F. Franchetti, and J. C. Hoe, "Memory Bandwidth Efficient Two-Dimensional Fast Fourier Transform Algorithm and Implementation for Large Problem Sizes," 2012, doi: 10.1109/FCCM.2012.40.
34. B. Akin, F. Franchetti, and J. C. Hoe, "Understanding the design space of DRAM-optimized hardware FFT accelerators," 2014, doi: 10.1109/ASAP.2014.6868669.
35. R. Chen and V. K. Prasanna, "Energy optimizations for FPGA-based 2-D FFT architecture," 2014, doi: 10.1109/HPEC.2014.7040967.
36. R. Chen, H. Le, and V. K. Prasanna, "Energy efficient parameterized FFT architecture," 2013, doi: 10.1109/FPL.2013.6645545.
37. S. Choi, G. Govindu, J.-W. Jang, and V. K. Prasanna, "Energy-efficient and parameterized designs for fast Fourier transform on FPGAs," 2003, doi: 10.1109/ICASSP.2003.1202418.
38. S.-N. Tang, C.-H. Liao, and T.-Y. Chang, "An Area- and Energy-Efficient Multimode FFT Processor for WPAN/WLAN/WMAN Systems," 2012, doi: 10.1109/JSSC.2012.2187406.
