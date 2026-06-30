## 亮点

- 提出一种面向计算光刻 TCC-SOCS 在线空中像重构的 CPU-FPGA 协同加速框架。
- 将离线 TCC 构建与 SOCS 本征核提取过程同高频调用的在线重构过程解耦。
- 基于 HLS FFT IP、块浮点缩放、LUT 映射乘法和 BRAM 缓冲实现资源高效的 128 x 128 二维 IFFT 流水线。
- 采用 7 个独立 AXI-MM 存储接口，降低掩模频谱、SOCS 本征核、权重和输出缓冲之间的数据访问竞争。
- 所提出设计在 250 MHz 下实现 10.57 ms 延迟，相较 C++ SOCS 基准实现 3.37 倍加速，能效提升约 67.4 倍。


# 面向 TCC-SOCS 空中像计算的低延迟高能效 FPGA/HLS 加速架构

## 摘要

TCC-SOCS 空中像计算是 OPC/SMO 等计算光刻流程中的关键环节，其在线重构阶段会被反复调用，在传统 CPU/GPU 平台上仍面临延迟、功耗与能效之间的权衡。本文提出一种面向 TCC-SOCS 在线重构的 FPGA/HLS 加速架构，用以验证可重构硬件在低延迟光刻仿真中的可行性。该架构采用 CPU-FPGA 协同设计：CPU 负责离线 TCC 构建、特征分解与 SOCS 本征核提取，FPGA 负责频域嵌入、128 x 128 二维 IFFT、加权强度累加和结果写回；在默认 DUV 配置下，17 x 17 SOCS 本征核被嵌入固定 FFT 网格以复用同一在线流水线。通过 HLS FFT IP、块浮点缩放、LUT 映射乘法、7 个 AXI-MM 接口和 BRAM 缓冲，所提出设计在 Xilinx Kintex UltraScale+ xcku5p 上以 250 MHz 实现 10.57 ms 的 10 核在线重构延迟，相较 MATLAB 完整 TCC 和 C++ SOCS 基准分别实现 45.3 倍和 3.37 倍加速。C/RTL 联合仿真 RMSE 为 $8.324 \times 10^{-7}$，板级验证 RMSE 为 $2.93 \times 10^{-8}$；在 10 个 ICCAD 测试图形上，10 核 SOCS 相对完整 TCC 的 RMSE 保持在 $5.38\times10^{-3}$ 至 $5.61\times10^{-3}$，SSIM 均高于 0.996。最终设计占用 17% LUT、9% FF、2% DSP 和 42% BRAM，估算能效较 CPU 基准提升约 67.4 倍。实验结果表明，FPGA 可作为 CPU/GPU 之外的有效光刻计算加速平台，为低延迟、低功耗空中像仿真提供了可部署路径。

**关键词：**计算光刻；TCC-SOCS；空中像计算；FPGA；HLS；二维 IFFT；能效。

## 1. 引言

计算光刻已成为先进半导体制造中连接光学成像、工艺窗口和版图优化的关键技术。随着工艺节点持续缩小，单纯依靠投影光学系统提升分辨率愈发困难，光学邻近效应校正（OPC）、源掩模协同优化（SMO）和反向光刻技术（ILT）等方法被广泛用于补偿邻近效应、优化照明与掩模形状并扩大可制造窗口 [1]-[11]。这些方法通常以模型反馈或反问题优化的形式工作，需要在大量版图窗口、多种工艺条件和多轮迭代中反复调用空中像计算 [2], [3], [5], [8]。当工艺窗口缩小、掩模复杂度提高且优化变量从边缘移动扩展到像素化或曲线图形时，空中像计算的延迟、吞吐和能耗逐渐成为计算光刻工程部署中的核心瓶颈。

部分相干光刻成像通常以 Hopkins 理论为基础，其传输交叉系数（TCC）可将照明光源、投影光瞳、像差、薄膜效应以及掩模频谱耦合统一表示为频域算子 [12]-[18]。在光学参数固定时，TCC 可离线预计算并在不同掩模图形间复用；然而，直接 TCC 成像涉及频率对之间的密集耦合，计算和存储开销较高。Sum of coherent systems（SOCS）方法通过对 TCC 进行特征分解或低秩近似，将部分相干成像转化为多个相干本征核的加权求和，从而在保持物理模型解释性的同时降低在线计算复杂度 [12], [13], [16], [19]。因此，TCC-SOCS 已成为空中像计算中兼顾准确性和效率的重要技术路线。

尽管 SOCS 显著降低了直接 TCC 计算的复杂度，离线分解之后的在线重构阶段仍是高频计算热点。对于每个 SOCS 本征核，在线重构都需要执行掩模频谱与本征核相乘、2D IFFT、模平方、特征值加权以及多核强度累加；这些操作会随本征核数量、掩模窗口数量和 OPC/SMO/ILT 迭代次数重复放大 [19]-[23]。已有工作从数值近似、对称性利用、GPU 加速和学习型模型等方向提升光刻仿真或掩模优化效率 [20], [22]-[30]。CPU 平台具有较高灵活性，但在规则频域运算下能效有限；GPU 在大批量吞吐场景中优势明显，但在小批量、低延迟、功耗受限或需要确定性执行的场景中，数据搬移和系统功耗仍可能带来约束。

FPGA 为 TCC-SOCS 在线重构提供了 CPU/GPU 之外的补充方案。复数乘法、二维 IFFT、模平方和累加均具有规则数据通路和清晰依赖关系，适合映射为定制流水线；片上 BRAM 可缓存中间矩阵以减少 DDR 往返访问，多 AXI-MM 接口也可缓解掩模频谱、SOCS 本征核、权重和输出缓冲之间的带宽竞争。已有研究验证了 FPGA 在光刻空中像仿真和二维 FFT 加速中的潜力，并指出外部存储访问、行列转置、并行度和能效之间的权衡对 FFT 类硬件加速器至关重要 [31]-[38]。然而，现有计算光刻加速工作更多关注算法近似、CPU/GPU 实现、学习型 OPC/ILT 或非 TCC-SOCS 的 FPGA 光刻仿真；如何在有限 FPGA 资源下为 TCC-SOCS 在线空中像重构系统平衡延迟、带宽、资源和精度，仍缺少端到端架构设计和板级验证。

本文聚焦于离线 TCC 构建和 SOCS 本征核提取之后的在线重构阶段，而非加速完整 TCC 生成、特征分解、完整 OPC/SMO 系统或 Host-FPGA 全链路传输。本文提出一种 CPU-FPGA 协同架构：CPU 负责离线 TCC 构建、特征分解和 SOCS 本征核生成，FPGA 负责在线频域嵌入、128 x 128 二维 IFFT、加权累加、FFTshift 和结果写回。主要贡献包括：提出面向在线 TCC-SOCS 重构的协同计算框架；基于 HLS FFT IP、块浮点缩放和 LUT 映射乘法设计资源高效流水线；采用 7 个 AXI-MM 接口和 BRAM 缓冲优化存储访问；在 Kintex UltraScale+ xcku5p 上完成 C 仿真、C/RTL 联合仿真和板级验证。实验表明，10 核 SOCS 在线重构在 250 MHz 下延迟为 10.57 ms，相较 C++ SOCS 基准加速 3.37 倍，能效提升约 67.4 倍，C/RTL RMSE 为 $8.324 \times 10^{-7}$，板级 RMSE 为 $2.93 \times 10^{-8}$，资源占用为 17% LUT、9% FF、2% DSP 和 42% BRAM。本文其余部分组织如下：第 2 节介绍 TCC-SOCS 模型与所提出架构，第 3 节给出实验结果，第 4 节总结全文。

## 2. 原理与方法

### 2.1 TCC-SOCS 成像模型与问题定义

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

在离散实现中，TCC 的有效频域范围由光瞳截止频率决定：

$$
N_x=N_y=\left\lfloor \frac{L_x\cdot NA\cdot(1+\sigma_{out})}{\lambda}\right\rfloor .
$$

对应的二维本征核尺寸为 $(2N_x+1)\times(2N_y+1)$，离散 TCC 矩阵规模为 $N_f\times N_f$，其中 $N_f=(2N_x+1)(2N_y+1)$。在默认 $L_x=L_y=1024$、$NA=0.8$、$\lambda=193$ nm、$\sigma_{out}=0.9$ 的 DUV 配置下，$N_x=N_y=8$，因此本征核尺寸为 17 x 17，$N_f=289$。这一定量映射解释了后续 FPGA 频域嵌入模块的输入窗口大小。

本文中，离线阶段由 CPU 计算 SOCS 本征核及其特征值权重。FPGA 负责如下在线映射：

$$
\hat{I}=f_{\mathrm{FPGA}}(M,\{\Phi_k,\sigma_k\}_{k=1}^{N_k}),
$$

其中，$\hat{I}$ 为重构得到的空中像。优化目标是在相对软件 SOCS 参考结果保持较小硬件实现误差的前提下，降低在线计算延迟和能耗。

### 2.2 CPU-FPGA 协同框架与在线流水线

所提出框架将光学系统预处理与在线重构分离。CPU 负责配置解析、光源生成、掩模 FFT、TCC 构建、特征分解和数据格式化。生成的掩模频谱、SOCS 本征核和特征值权重被存储在外部 DDR 中。FPGA 通过 AXI-MM 接口读取这些数据并执行在线重构。

这种划分方式源于两个阶段不同的执行特征。TCC 构建和特征分解需要高精度矩阵运算，但仅在光学参数变化时执行；相比之下，在线 SOCS 重构会针对不同掩模窗口重复执行，并且主要由规则的 FFT 和累加操作构成。因此，将在线阶段迁移到 FPGA 可直接带来延迟和能效收益，同时保留 CPU 在离线预处理方面的灵活性。

表 1 总结了该协同框架中的任务边界。该划分继承了大论文中“离线 TCC 计算 + 在线 SOCS 重构”的核心思想：光源、光瞳和像差等光学系统信息被封装在 SOCS 本征核与特征值中，而 FPGA 在线阶段只处理掩模频谱、本征核和权重。

**表 1** CPU-FPGA 协同框架中的任务划分

| 执行侧   | 主要任务                                                     | 计算特征                           |
| -------- | ------------------------------------------------------------ | ---------------------------------- |
| Host CPU | 参数解析、光源生成、掩模 FFT、TCC 构建、特征分解、本征核导出 | 高精度矩阵运算，光学参数变化时执行 |
| FPGA     | 频域嵌入、二维 IFFT、强度累加、FFTshift、结果写回            | 规则数据流，针对掩模窗口反复执行   |

![图 1 TCC-SOCS 计算流程](../image/论文/ch3_fig1_hopkins_workflow.png)

**图 1** TCC-SOCS 计算流程。离线光学系统处理生成 SOCS 本征核和特征值权重，在线重构阶段针对掩模窗口计算空中像。

**FPGA 在线重构流水线。** FPGA 流水线包含五个阶段：

1. **频域嵌入：**将 SOCS 本征核与对应的掩模频谱中心窗口相乘，并将结果嵌入固定的 128 x 128 FFT 网格。
2. **二维 IFFT：**基于 HLS FFT IP 依次执行行向 FFT、矩阵转置和列向 FFT。
3. **加权累加：**计算 $|E_k|^2$，并将 $\sigma_k|E_k|^2$ 累加至临时图像缓冲区。
4. **FFTshift：**通过象限交换将零频分量移动至图像中心。
5. **输出写回：**通过 AXI-MM burst 访问将最终 128 x 128 图像写回 DDR。

对于默认 10 核配置，上述五阶段路径针对每个本征核顺序执行。该核间时分复用策略在多个本征核之间复用同一套二维 IFFT 引擎，从而避免过高的 BRAM 消耗。全核并行虽然可提供更高吞吐量，但每个并行核需要独立的 128 x 128 二维 IFFT 实例及转置缓冲区；按当前 HLS FFT IP 的资源模型估计，10 核全并行会需要约 3000 个 BRAM_18K，显著超过 xcku5p 的 960 个 BRAM_18K。因此，本文采用核间时分复用与核内流水相结合的折中设计。

主机通过 AXI-Lite 接口配置核数量 $N_k$、有效频域范围 $N_x,N_y$ 以及掩模尺寸 $L_x,L_y$。在固定 128 x 128 IFFT 网格下，不同有效核尺寸可通过零填充和中心嵌入复用同一 bitstream，从而避免针对不同光学配置反复综合硬件。

![图 2 所提出的 FPGA 加速架构](../image/论文/ch4_fig1_fpga_architecture.png)

**图 2** 面向在线 TCC-SOCS 空中像重构的 FPGA 加速架构。

### 2.3 关键硬件模块与存储架构

对于默认配置，有效本征核尺寸为 17 x 17，对应 $N_x=N_y=8$。每个本征核需要完成 289 次掩模频谱与 SOCS 本征核之间的复数乘法。嵌入模块将乘法结果映射至 128 x 128 频率网格中心，其余位置补零。复数乘法定义为：

$$
(a+jb)(c+jd)=(ac-bd)+j(ad+bc).
$$

HLS 实现对该循环进行流水线化，启动间隔接近 1。运行时参数定义实际有效本征核区域，编译时常量限定循环边界，以提高 HLS 调度的确定性。

在 HLS 实现中，频域嵌入循环使用编译时常量 `MAX_KERNEL_SIZE=17` 作为综合边界，并在循环体内根据运行时 $N_x,N_y$ 判断实际有效区域。该方式避免了可变循环边界导致的联合仿真超时和调度不确定问题。嵌入位置以掩模频谱中心 $L_x/2,L_y/2$ 为基准，保证 FPGA 输出与 CPU Golden 参考在空间相位上对齐。

**二维 IFFT 引擎。** 二维 IFFT 是主要计算阶段。本文使用 Xilinx HLS FFT IP 并基于行列分解实现。输入的 128 x 128 矩阵先逐行处理并存入中间 BRAM 缓冲区，再逐列处理。双端口 BRAM 缓冲区用于支持转置访问模式。

FFT IP 配置为 128 点变换长度、自然序输出、块浮点缩放以及 LUT 映射乘法。输入和输出采用 `ap_fixed<32,1>`，其量化步长约为 $2^{-31}$，可覆盖本设计中掩模频谱、核系数、频域乘积和 IFFT 输出的典型数值范围。块浮点缩放仅在存在溢出风险时动态移位，并通过 `blk_exp` 输出累计指数。最终转换阶段按

$$
E_{\mathrm{float}}=E_{\mathrm{fixed}}\cdot 2^{\mathrm{blk\_exp}}
$$

进行补偿。与固定逐级缩放相比，该策略在 $\log_2 128=7$ 的奇数 FFT 深度下能够保留更多有效精度。蝶形运算中的乘法被映射至 LUT，以显著降低 DSP 使用量。

**存储架构。** 顶层 HLS IP 使用 7 个独立 AXI-MM master 接口：

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

### 3.1 实验设置与精度验证

所提出设计使用 Vitis HLS 2025.2 和 Vivado 2025.2 进行评估。目标 FPGA 为 Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e。默认光学配置为 $L_x=L_y=1024$、$NA=0.8$、$\lambda=193$ nm、环形照明、$\sigma_{in}=0.6$、$\sigma_{out}=0.9$，SOCS 本征核数量为 10。在线 FFT 网格固定为 128 x 128。

CPU 基准平台使用 Intel Xeon Platinum 8163 服务器。MATLAB 和 C++ 实现被用作软件基准。MATLAB 提供 Golden Model 以及 TCC/SOCS 直接参考结果；C++ 实现提供更优化的单精度 CPU 对比。主要实验配置如表 2 所示。

**表 2** 实验平台与默认光学配置

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

本文从三个层级验证设计正确性：C 仿真、C/RTL 联合仿真和板级验证。C 仿真确认算法级正确性，C/RTL 联合仿真验证生成 RTL 的时序行为和 AXI 事务，板级验证进一步确认部署后硬件输出的正确性。验证结果见表 3。

**表 3** FPGA/HLS 实现误差验证结果

| 验证方式       |                   RMSE | 说明           |
| -------------- | ---------------------: | -------------- |
| C 仿真         |  $2.93 \times 10^{-8}$ | HLS 算法级结果 |
| C/RTL 联合仿真 | $8.324 \times 10^{-7}$ | RTL 级结果     |
| 板级验证       |  $2.93 \times 10^{-8}$ | 硬件输出       |

误差主要来自定点量化、块浮点缩放以及 FFT 蝶形运算舍入。所得 RMSE 均低于 $10^{-5}$，说明在所测试的 SOCS 重构流程中，硬件实现误差足够小。

需要区分硬件实现误差和 SOCS 截断误差。硬件实现误差比较 FPGA 输出与同一 SOCS 软件参考结果；SOCS 截断误差比较有限核 SOCS 结果与完整 TCC 直接成像结果，后者取决于保留的本征核数量。不同 SOCS 核数量下的截断误差见表 4。

**表 4** 不同 SOCS 核数量下相对完整 TCC 的成像误差

| SOCS 核数量 |   相对完整 TCC 的 RMSE | 解释                 |
| ----------: | ---------------------: | -------------------- |
|          10 | $5.474 \times 10^{-3}$ | 低延迟，中等截断误差 |
|          50 | $0.927 \times 10^{-3}$ | 精度更高，延迟增加   |
|         400 |  $2.57 \times 10^{-6}$ | 接近满秩参考结果     |

默认选择 10 核配置，是因为其在延迟与精度之间提供了较实用的平衡。视觉对比也表明，FPGA 输出能够保持主要空中像结构，剩余误差主要分布在掩模高频边缘附近。

除 RMSE 外，本文还使用图像质量指标评估 FPGA 输出与 MATLAB Golden 参考的一致性。10 核配置下，C 仿真 RMSE 为 $2.93 \times 10^{-8}$，C/RTL 联合仿真 RMSE 为 $8.324 \times 10^{-7}$，最大绝对误差小于 $1.0\times10^{-5}$，PSNR 高于 120 dB，SSIM 达到 0.9999 以上。在阈值 $I_{th}=0.225$ 下，二值化轮廓一致率超过 99.99%。这些结果表明，硬件量化和块浮点缩放误差远小于 SOCS 截断误差，主要成像结构不会因 FPGA 实现而改变。

为验证算法对不同掩模图形的稳定性，本文还采用 ICCAD 2013 的 10 个测试用例进行检查。表 5 给出了统一光学配置和 10 核 SOCS 设置下的量化结果，图 3 进一步给出了对应的掩模图形、SOCS 成像结果和完整 TCC 参考结果。可以看到，RMSE 分布在 $5.38\times10^{-3}$ 至 $5.61\times10^{-3}$ 之间，PSNR 均高于 48 dB，SSIM 均高于 0.996。不同测试图形之间的计算时间差异小于 2%，说明在线阶段复杂度主要由核数量和 FFT 网格决定，而不是由具体掩模几何复杂度决定。

**表 5** 不同掩模图形下的 10 核 SOCS 成像泛化结果

| 测试用例 | RMSE ($\times10^{-3}$) | PSNR (dB) |   SSIM |
| -------- | ---------------------: | --------: | -----: |
| T1       |                   5.47 |     48.37 | 0.9966 |
| T2       |                   5.52 |     48.21 | 0.9964 |
| T3       |                   5.38 |     48.52 | 0.9968 |
| T4       |                   5.61 |     48.05 | 0.9962 |
| T5       |                   5.44 |     48.41 | 0.9967 |
| T6       |                   5.55 |     48.18 | 0.9963 |
| T7       |                   5.49 |     48.32 | 0.9965 |
| T8       |                   5.58 |     48.12 | 0.9961 |
| T9       |                   5.42 |     48.45 | 0.9967 |
| T10      |                   5.51 |     48.25 | 0.9964 |

![图 3（a）ICCAD 2013 测试用例掩模图形](../image/论文/ch5_fig7_mask_patterns.png)

![图 3（b）10 核 SOCS 空中像结果](../image/论文/ch5_fig7_socs_aerial.png)

![图 3（c）完整 TCC 参考空中像结果](../image/论文/ch5_fig7_tcc_aerial.png)

**图 3** 不同掩模图形下的泛化验证。（a）ICCAD 2013 测试用例掩模图形；（b）10 核 SOCS 空中像结果；（c）完整 TCC 参考空中像结果。

图 4 给出了默认测试图形下 MATLAB Golden 参考、FPGA 输出和误差分布的进一步对比。FPGA 输出能够保持主要空中像结构，残差主要集中在掩模边缘的高频区域，与 SOCS 截断误差的空间分布一致。

![图 4 参考结果与 FPGA 输出的视觉对比](../image/论文/ch5_fig4_visual_comparison.png)

**图 4** 参考空中像、FPGA 输出与误差分布对比。

### 3.2 运行时间与加速比分析

在 250 MHz 下，HLS 估计延迟为 2,643,645 个周期，对应 10.57 ms。C/RTL 联合仿真报告为 2,651,856 个周期，与综合估计接近。二者之间的小幅差异主要来自协议与调度开销。延迟分解见表 6 和图 5。

**表 6** 10 核 SOCS 在线重构延迟分解

| 阶段             |        周期数 | 250 MHz 下时间 |     占比 |
| ---------------- | ------------: | -------------: | -------: |
| 频域嵌入，10 核  |       167,450 |        0.67 ms |     6.3% |
| 二维 IFFT，10 核 |     2,262,250 |        9.05 ms |    85.6% |
| 累加，10 核      |       164,250 |        0.66 ms |     6.2% |
| FFTshift         |        16,389 |       0.066 ms |     0.6% |
| DDR 输出         |        16,389 |       0.066 ms |     0.6% |
| **总计**         | **2,643,645** |   **10.57 ms** | **100%** |

二维 IFFT 占总延迟约 85.6%，说明 FFT 优化是主要性能杠杆。此处报告的延迟指 FPGA 在线重构阶段，不包含离线 TCC 构建、特征分解、主机侧预处理、Host-FPGA 数据传输或全分辨率傅里叶插值。当前 FPGA 输出为 128 x 128 空中像，若需恢复至 1024 x 1024 分辨率，主机侧傅里叶插值约需 5-10 ms；该步骤与 FPGA 在线重构延迟处于同一量级，是端到端系统后续需要集成优化的部分。

时钟频率方面，本文采用 250 MHz 作为保守评估频率。HLS 综合中，300 MHz 目标下顶层存在约 -0.79 ns 的负时序裕量，关键路径主要位于块浮点指数补偿相关的定点-浮点转换逻辑；降至 250-280 MHz 后更容易获得稳定时序。因此，本文报告的 10.57 ms 是保守频率下的在线核延迟，而不是最高可能频率下的理论值。

![图 5 所提出 FPGA 流水线的延迟分解](../image/论文/ch5_fig2_latency_breakdown.png)

**图 5** 所提出 FPGA 在线重构流水线的延迟分解。

FPGA 运行时间与 MATLAB 和 C++ 基准对比如表 7 所示。

**表 7** FPGA 与软件基准的运行时间和加速比

| 基准                     |  运行时间 | FPGA 运行时间 |  加速比 |
| ------------------------ | --------: | ------------: | ------: |
| MATLAB 完整 TCC 直接成像 |    479 ms |      10.57 ms | 45.3 倍 |
| MATLAB 10 核 SOCS        |    287 ms |      10.57 ms | 27.1 倍 |
| C++ 完整 TCC 直接成像    | 45.176 ms |      10.57 ms | 4.28 倍 |
| C++ 10 核 SOCS           |   35.6 ms |      10.57 ms | 3.37 倍 |

与 C++ SOCS 的对比最为保守，因为二者比较的是相同的在线重构范围。尽管 3.37 倍加速不如 MATLAB 对比显著，但考虑到 OPC/SMO 流程会在大量版图窗口中反复调用空中像计算，该加速仍具有工程意义。

从平台定位看，FPGA 的优势主要体现在低功耗和确定性延迟，而不是替代高端 GPU 的绝对吞吐。以单次 10 核 SOCS 成像为例，MATLAB CPU 约为 287 ms，C++ CPU 约为 35.6 ms，本文 FPGA 为 10.57 ms。代表性 GPU 光刻平台通常可获得更高吞吐，但功耗常处于数百瓦量级；本文 FPGA 设计在约 4 W 功耗估算下实现 94.6 frames/s，更适合低功耗、低延迟或嵌入式部署场景。

### 3.3 资源、能效与局限性

最终设计在 xcku5p 上的资源利用率见表 8。

**表 8** xcku5p 上的 FPGA 资源利用率

| 资源     | 使用量 |  可用量 | 利用率 |
| -------- | -----: | ------: | -----: |
| LUT      | 36,931 | 216,960 |    17% |
| FF       | 38,703 | 433,920 |     9% |
| DSP      |     34 |   1,824 |     2% |
| BRAM_18K |    399 |     960 |    42% |

两项资源优化对该架构的可部署性至关重要。首先，使用 HLS FFT IP 替代直接 DFT，使 DSP 使用量从 8,064 个降低至 34 个，降幅为 99.6%。其次，对 SOCS 本征核采用时分复用并共享二维 IFFT 引擎，使 BRAM 使用量从 1,366 个降低至 399 个，降幅为 70.8%。这些优化使设计能够适配中等规模 Kintex UltraScale+ 器件。

![图 6 性能与资源总结](../image/论文/ch5_fig3_performance_resource.png)

**图 6** 所提出 FPGA/HLS 架构的性能与资源总结。

**能效分析。** FPGA 功耗约 4 W，来自器件静态功耗和基于综合/功耗分析的动态功耗估算；其中静态功耗约 1.5 W，动态功耗约 2.5 W。C++ CPU 基准平台功耗估计为 65-80 W。基于 10.57 ms 的 FPGA 延迟，FPGA 吞吐量约为 94.6 frames/s。相比 C++ SOCS 在 80 W 下约 28.09 frames/s 的吞吐量，FPGA 能效提升约 67.4 倍。

**表 9** FPGA 与 C++ CPU SOCS 的能效对比

| 平台         | 运行时间 |    功耗 |         吞吐量 |           能效 |
| ------------ | -------: | ------: | -------------: | -------------: |
| C++ CPU SOCS |  35.6 ms | 约 80 W | 28.09 frames/s | 0.351 frames/J |
| FPGA SOCS    | 10.57 ms |  约 4 W |  94.6 frames/s |  23.7 frames/J |

这一结果表明，所提出 FPGA 架构尤其适合能耗受限或延迟敏感的光刻仿真负载。

与其他平台的定位对比如表 10 所示。

**表 10** 不同计算平台的延迟和能效定位

| 平台                |     延迟 |     功耗 |             能效 | 说明                           |
| ------------------- | -------: | -------: | ---------------: | ------------------------------ |
| MATLAB CPU SOCS     |   287 ms |  约 80 W |   0.044 frames/J | 双精度软件基准                 |
| C++ CPU SOCS        |  35.6 ms |  约 80 W |   0.351 frames/J | 单精度软件基准                 |
| 代表性 GPU 光刻平台 |  约 5 ms | 约 300 W | 约 0.67 frames/J | 高吞吐、高功耗，公开平台级指标 |
| 本文 FPGA SOCS      | 10.57 ms |   约 4 W |    23.7 frames/J | 低延迟、低功耗、确定性执行     |

其中 GPU 行用于平台级定位，非同一代码路径或同一硬件环境下的严格同源实测。

**讨论与局限性。** 当前设计仍存在若干局限。首先，SOCS 本征核采用时分复用方式处理，而非完全核间并行；这一选择受 FFT IP 的 BRAM 占用约束。根据资源估算，xcku5p 可支持并行度 2 左右，但更高并行度需要更大 BRAM 容量的器件。其次，FFT 网格固定为 128 x 128，对于当前 1024 x 1024 DUV 配置较为高效，但对于更小本征核可能存在资源浪费，对于更大的 High-NA EUV 配置则可能不足。第三，本文报告的 FPGA 延迟聚焦于在线重构核，不包含完整 Host-FPGA 数据传输、主机侧傅里叶插值或完整 OPC/SMO 系统集成。第四，功耗数据为估算值，后续需要通过后实现功耗分析和板级实测进一步修正。第五，实验主要覆盖 Annular 光源和典型 DUV 参数，未来仍需扩展至 Dipole、Quasar、High-NA EUV 以及更大规模工业掩模图形。

尽管存在上述局限，本文结果表明，只要合理管理 FFT 资源消耗与存储带宽，TCC-SOCS 在线重构能够通过 HLS 高效映射到 FPGA 架构中。

## 4. 结论

本文提出一种面向 TCC-SOCS 空中像重构的低延迟高能效 FPGA/HLS 架构。该方法的核心思想是将光学系统相关的 TCC 构建与 SOCS 本征核提取保留在 CPU 端，将在 OPC/SMO 等流程中被反复调用的在线重构阶段映射为 FPGA 上的专用数据通路。通过这种划分，复杂光学模型被压缩为可复用的本征核和权重，而 FPGA 只需执行频域嵌入、二维 IFFT、加权累加、FFTshift 和结果写回等规则计算，从而把 TCC-SOCS 的在线热点转化为可流水化、可部署的硬件任务。

实验结果表明，所提出架构在精度、延迟和能效之间取得了较好的平衡。通过块浮点 FFT 缩放、LUT 映射 FFT 乘法、多端口 AXI-MM 访问和 BRAM 缓冲，设计在 xcku5p FPGA 上以 250 MHz 实现 10 核在线重构延迟 10.57 ms，C/RTL RMSE 为 $8.324 \times 10^{-7}$，板级 RMSE 为 $2.93 \times 10^{-8}$。在 10 个 ICCAD 测试图形上，SOCS 结果相对完整 TCC 的 RMSE 保持在 $5.38\times10^{-3}$ 至 $5.61\times10^{-3}$，SSIM 均高于 0.996，说明该方法对不同掩模图形具有稳定的成像保真度。最终架构仅占用 17% LUT、9% FF、2% DSP 和 42% BRAM，相较 C++ CPU SOCS 基准实现 3.37 倍加速和约 67.4 倍估算能效提升。对于计算光刻中大量重复的小窗口空中像评估而言，这一结果说明 FPGA 不只是通用 CPU/GPU 之外的替代计算平台，更适合承担低功耗、确定性延迟的在线成像内核。

本文方法可进一步应用于 OPC 迭代中的批量窗口评估、SMO/ILT 优化中的快速成像反馈、工艺窗口扫描中的多条件空中像计算，以及功耗受限的边缘或嵌入式光刻仿真节点。面向后续部署，可在更大 BRAM 容量器件上提升 SOCS 核间并行度，支持自适应 FFT 网格尺寸，将傅里叶插值和后处理集成至 FPGA 流水线，并扩展至 Dipole、Quasar、High-NA EUV 及更大规模工业掩模图形。由此，所提出架构有望从单核在线重构验证进一步发展为面向实际计算光刻流程的高能效硬件加速模块。

## 参考文献

1. Granik Y. Fast pixel-based mask optimization for inverse lithography[J]. Journal of Micro/Nanolithography, MEMS and MOEMS, 2006, 5(4): 043002-043002-13.
2. Cobb N B, Zakhor A, Miloslavsky E A. Mathematical and CAD framework for proximity correction[C]//Optical Microlithography IX. SPIE, 1996, 2726: 208-222.
3. Jia N, Lam E Y. Pixelated source mask optimization for process robustness in optical lithography[J]. Optics express, 2011, 19(20): 19384-19398.
4. Li Z, Dong L, Ma X, et al. Fast source mask co-optimization method for high-NA EUV lithography[J]. Opto-Electronic Advances, 2025, 7(4): 230235-1-230235-11.
5. Shen Y, Wong N, Lam E Y. Level-set-based inverse lithography for photomask synthesis[J]. Optics Express, 2009, 17(26): 23690-23701.
6. Abrams D S, Pang L. Fast inverse lithography technology[C]//Optical Microlithography XIX. SPIE, 2006, 6154: 534-542.
7. Pang L, Liu Y, Abrams D. Inverse lithography technology (ILT): What is the impact to the photomask industry?[C]//Photomask and Next-Generation Lithography Mask Technology XIII. SPIE, 2006, 6283: 233-243.
8. Gao J R, Xu X, Yu B, et al. MOSAIC: Mask optimizing solution with process window aware inverse correction[C]//Proceedings of the 51st Annual Design Automation Conference. 2014: 1-6.
9. Kim B G, Suh S S, Kim B S, et al. Trade-off between inverse lithography mask complexity and lithographic performance[C]//Photomask and Next-Generation Lithography Mask Technology XVI. SPIE, 2009, 7379: 458-468.
10. Pang L, Liu Y, Abrams D. Inverse lithography technology (ILT): a natural solution for model-based SRAF at 45-nm and 32-nm[C]//Photomask and Next-Generation Lithography Mask Technology XIV. SPIE, 2007, 6607: 888-897.
11. Hung C Y, Zhang B, Guo E, et al. Pushing the lithography limit: Applying inverse lithography technology (ILT) at the 65nm generation[C]//Optical Microlithography XIX. SPIE, 2006, 6154: 562-571.
12. Ma X, Arce G. Binary mask optimization for inverse lithography with partially coherent illumination[J]. Journal of the optical society of America A, 2008, 25(12): 2960-2970.
13. Maa X, Arceb G R. PSM design for inverse lithography with partially coherent illumination[J]. Optics express, 2008, 16(24): 20126-20141.
14. Yeung M S, Lee D, Lee R S, et al. Extension of the Hopkins theory of partially coherent imaging to include thin-film interference effects[C]//Optical/Laser Microlithography. SPIE, 1993, 1927: 452-463.
15. Adam K, Granik Y, Torres A, et al. Improved modeling performance with an adapted vectorial formulation of the Hopkins imaging equation[C]//Optical Microlithography XVI. SPIE, 2003, 5040: 78-91.
16. Gong P, Liu S, Lv W, et al. Fast aerial image simulations for partially coherent systems by transmission cross coefficient decomposition with analytical kernels[J]. Journal of Vacuum Science & Technology B, 2012, 30(6).
17. Köhle R. Fast TCC algorithm for the model building of high NA lithography simulation[C]//Optical Microlithography XVIII. SPIE, 2005, 5754: 918-929.
18. Wu X, Liu S, Liu W, et al. Comparison of three TCC calculation algorithms for partially coherent imaging simulation[C]//Sixth International Symposium on Precision Engineering Measurements and Instrumentation. SPIE, 2010, 7544: 241-250.
19. Yu P, Pan D Z. ELIAS: an accurate and extensible lithography aerial image simulator with improved numerical algorithms[J]. IEEE transactions on semiconductor manufacturing, 2009, 22(2): 276-289.
20. Yu P, Qiu W, Pan D Z. Fast lithography image simulation by exploiting symmetries in lithography systems[J]. IEEE transactions on semiconductor manufacturing, 2008, 21(4): 638-645.
21. Bernard D A, Li J, Rey J C, et al. Efficient computational techniques for aerial imaging simulation[C]//Optical Microlithography IX. SPIE, 1996, 2726: 273-287.
22. Rodrigues R, Sreedhar A, Kundu S. Optical lithography simulation using wavelet transform[C]//2009 IEEE International Conference on Computer Design. IEEE, 2009: 427-432.
23. Zheng X, Ma X, Zhao Q, et al. Model-informed deep learning for computational lithography with partially coherent illumination[J]. Optics Express, 2020, 28(26): 39475-39491.
24. Torunoglu I, Karakas A, Elsen E, et al. OPC on a single desktop: a GPU-based OPC and verification tool for fabs and designers[C]//Design for Manufacturability through Design-Process Integration IV. SPIE, 2010, 7641.
25. Yu Z, Chen G, Ma Y, et al. A GPU-Enabled Level-Set Method for Mask Optimization[J]. IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, 2023, 42(2): 594-605.
26. Yang H, Li S, Deng Z, et al. GAN-OPC: Mask Optimization With Lithography-Guided Generative Adversarial Nets[J]. IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, 2020, 39(10): 2822-2834.
27. Ye W, Alawieh M B, Lin Y, et al. LithoGAN[C]//Proceedings of the 56th Annual Design Automation Conference 2019. ACM, 2019: 1-6.
28. Jiang B, Liu L, Ma Y, et al. Neural-ILT 2.0: Migrating ILT to Domain-Specific and Multitask-Enabled Neural Network[J]. IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, 2022, 41(8): 2671-2684.
29. Chen G, Chen W, Sun Q, et al. DAMO: Deep Agile Mask Optimization for Full-Chip Scale[J]. IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, 2022, 41(9): 3118-3131.
30. Yang H, Li Z, Sastry K, et al. Generic lithography modeling with dual-band optics-inspired neural networks[C]//Proceedings of the 59th ACM/IEEE Design Automation Conference. ACM, 2022: 973-978.
31. Cong J, Zou Y. FPGA-Based Hardware Acceleration of Lithographic Aerial Image Simulation[J]. ACM Transactions on Reconfigurable Technology and Systems, 2009, 2(3): 1-29.
32. Cong J, Zou Y. Lithographic aerial image simulation with FPGA-based hardwareacceleration[C]//Proceedings of the 16th international ACM/SIGDA symposium on Field programmable gate arrays. ACM, 2008: 67-76.
33. Akin B, Milder P A, Franchetti F, et al. Memory Bandwidth Efficient Two-Dimensional Fast Fourier Transform Algorithm and Implementation for Large Problem Sizes[C]//2012 IEEE 20th International Symposium on Field-Programmable Custom Computing Machines. IEEE, 2012.
34. Akin B, Franchetti F, Hoe J C. Understanding the design space of DRAM-optimized hardware FFT accelerators[C]//2014 IEEE 25th International Conference on Application-Specific Systems, Architectures and Processors. IEEE, 2014: 248-255.
35. Chen R, Prasanna V K. Energy optimizations for FPGA-based 2-D FFT architecture[C]//2014 IEEE High Performance Extreme Computing Conference (HPEC). IEEE, 2014: 1-6.
36. Chen R, Le H, Prasanna V K. Energy efficient parameterized FFT architecture[C]//2013 23rd International Conference on Field programmable Logic and Applications. IEEE, 2013: 1-7.
37. Seonil Choi, Govindu G, Ju-Wook Jang, et al. Energy-efficient and parameterized designs for fast Fourier transform on FPGAs[C]//2003 IEEE International Conference on Acoustics, Speech, and Signal Processing, 2003. Proceedings. (ICASSP '03). IEEE, 2003, 2: II-521-4.
38. Tang S N, Liao C H, Chang T Y. An Area- and Energy-Efficient Multimode FFT Processor for WPAN/WLAN/WMAN Systems[J]. IEEE Journal of Solid-State Circuits, 2012, 47(6): 1419-1435.
