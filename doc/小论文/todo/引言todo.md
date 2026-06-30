# FPGA 加速 TCC-SOCS 论文引言改写 Todo 清单

## A. 明确引言总体结构

- [ ] 将引言调整为 **5 段式结构**：
  - [ ] 第 1 段：计算光刻背景与空中像计算的重要性
  - [ ] 第 2 段：Hopkins TCC 与 SOCS 模型
  - [ ] 第 3 段：SOCS 在线重构的计算瓶颈
  - [ ] 第 4 段：FPGA 加速机会与研究空白
  - [ ] 第 5 段：本文方法、贡献与论文结构

---

# B. 第 1 段：计算光刻背景与空中像计算的重要性

## 写作任务

- [ ] 说明计算光刻是先进半导体制造中的关键技术。
- [ ] 提到 OPC、SMO、ILT 等流程都依赖空中像仿真。
- [ ] 强调空中像仿真在优化流程中会被反复调用。
- [ ] 说明先进制程下：
  - [ ] 工艺窗口缩小；
  - [ ] 版图复杂度提升；
  - [ ] 优化迭代次数增加；
  - [ ] 空中像计算负担加重。
- [ ] 引出核心问题：
  - [ ] 空中像计算的延迟和能耗成为计算光刻流程瓶颈。

## 需要补充的文献

- [ ] 计算光刻综述文献。
- [ ] OPC 相关代表性文献。
- [ ] SMO 相关代表性文献。
- [ ] ILT 相关代表性文献。
- [ ] 空中像仿真在计算光刻流程中被高频调用的文献。

## 推荐检索式

```text
"computational lithography" review
"optical proximity correction" "aerial image simulation"
"source mask optimization" "aerial image"
"inverse lithography technology" computational cost
"computational lithography" OPC SMO ILT
```

---

# C. 第 2 段：Hopkins TCC 与 SOCS 模型

## 写作任务

- [ ] 介绍 Hopkins TCC 模型。
- [ ] 说明 TCC 用于描述部分相干光刻成像。
- [ ] 说明 TCC 将以下因素编码为频域算子：
  - [ ] 照明光源；
  - [ ] 投影光瞳；
  - [ ] 像差；
  - [ ] 掩模频谱耦合。
- [ ] 强调光学参数固定时 TCC 可离线预计算。
- [ ] 说明直接 TCC 计算的缺点：
  - [ ] 频率对密集耦合；
  - [ ] 计算开销高；
  - [ ] 存储开销高。
- [ ] 引入 SOCS：
  - [ ] 通过特征分解/低秩近似；
  - [ ] 将部分相干成像转化为多个相干系统加权求和；
  - [ ] 降低计算复杂度。
- [ ] 总结 TCC-SOCS 的地位：
  - [ ] 兼顾物理准确性和计算效率；
  - [ ] 是空中像计算的重要技术路线。

## 需要补充的文献

- [ ] Hopkins 部分相干成像原始或经典文献。
- [ ] TCC 在光刻成像中的经典文献。
- [ ] SOCS 分解相关文献。
- [ ] TCC-SOCS 用于空中像仿真的代表性文献。

## 推荐检索式

```text
"Hopkins" "transmission cross coefficient" lithography
"TCC" "SOCS" lithography
"sum of coherent systems" lithography
"aerial image simulation" "transmission cross coefficient"
"partially coherent imaging" "Hopkins" lithography
```

---

# D. 第 3 段：SOCS 在线重构的计算瓶颈

## 写作任务

- [ ] 明确指出：SOCS 虽然降低了复杂度，但在线重构仍然是热点。
- [ ] 描述在线重构的主要操作：
  - [ ] 掩模频谱与 SOCS 本征核逐核相乘；
  - [ ] 2D IFFT；
  - [ ] 模平方计算；
  - [ ] 特征值加权；
  - [ ] 多核强度累加。
- [ ] 强调这些操作会被重复执行：
  - [ ] 对每个 SOCS 本征核；
  - [ ] 对每个掩模窗口；
  - [ ] 对 OPC/SMO/ILT 中的大量迭代。
- [ ] 说明 CPU 平台局限：
  - [ ] 灵活性高；
  - [ ] 但规则频域运算下能效有限。
- [ ] 说明 GPU 平台局限，注意措辞稳妥：
  - [ ] 吞吐量高；
  - [ ] 但功耗较高；
  - [ ] 数据搬移开销可能较大；
  - [ ] 对小批量、低延迟或确定性执行场景可能不是最优。
- [ ] 引出需求：
  - [ ] 需要低延迟、高能效、可部署的 TCC-SOCS 在线重构架构。

## 需要补充的文献

- [ ] 空中像仿真计算复杂度相关文献。
- [ ] SOCS 在线计算复杂度相关文献。
- [ ] CPU 并行或软件优化光刻仿真文献。
- [ ] GPU 加速 OPC/SMO/空中像仿真文献。
- [ ] FFT 主导空中像计算的相关文献。

## 推荐检索式

```text
"lithography simulation" acceleration GPU
"OPC" acceleration GPU
"aerial image simulation" GPU
"SOCS" acceleration lithography
"FFT" "aerial image" lithography acceleration
"source mask optimization" GPU acceleration
```

---

# E. 第 4 段：FPGA 加速机会与研究空白

## 写作任务

- [ ] 说明 TCC-SOCS 在线重构适合 FPGA：
  - [ ] 复数乘法规则；
  - [ ] 2D IFFT 结构固定；
  - [ ] 模平方和累加数据依赖清晰；
  - [ ] 可流水线化；
  - [ ] 可用片上 BRAM 缓冲减少 DDR 访问。
- [ ] 说明 FPGA 的潜在优势：
  - [ ] 空间并行；
  - [ ] 定制流水线；
  - [ ] 确定性执行；
  - [ ] 低功耗；
  - [ ] 数据搬移可控。
- [ ] 指出现有研究空白：
  - [ ] 现有计算光刻加速多集中于 CPU/GPU；
  - [ ] FPGA/HLS 用于 TCC-SOCS 在线空中像重构的系统研究不足；
  - [ ] 缺少端到端架构设计和板级验证。
- [ ] 明确架构挑战：
  - [ ] 2D IFFT 资源消耗大；
  - [ ] BRAM 占用高；
  - [ ] SOCS 多核并行与资源之间存在权衡；
  - [ ] 多缓冲区访问会产生 DDR 带宽竞争；
  - [ ] 块浮点/定点/浮点实现会影响数值精度；
  - [ ] HLS 调度需要可综合、可部署的数据流设计。
- [ ] 形成 gap statement：
  - [ ] “如何在有限 FPGA 资源下平衡延迟、资源、带宽和精度，仍缺少系统研究。”

## 需要补充的文献

- [ ] FPGA/HLS FFT 加速文献。
- [ ] FPGA 2D FFT 架构文献。
- [ ] FPGA 图像处理或科学计算加速文献。
- [ ] FPGA/HLS 低功耗高能效计算文献。
- [ ] 若能找到，补充 FPGA/硬件加速光刻仿真文献。

## 推荐检索式

```text
"FPGA" "HLS" "FFT" acceleration
"2D FFT" "FPGA" "HLS"
"FPGA" "high-level synthesis" "image processing"
"FPGA" "scientific computing" "energy efficiency"
"FPGA" "lithography simulation"
"hardware acceleration" "computational lithography"
```

---

# F. 第 5 段：本文方法与贡献

## 写作任务

- [ ] 用一两句话概括本文方法：
  - [ ] 提出面向 TCC-SOCS 在线重构的 CPU-FPGA 协同架构；
  - [ ] CPU 负责离线 TCC 构建、特征分解、SOCS 本征核生成；
  - [ ] FPGA 负责在线频域嵌入、2D IFFT、加权累加、FFTshift 和写回。
- [ ] 强调本文不是加速完整 TCC 构建，而是加速：
  - [ ] 离线 SOCS 分解之后的在线重构阶段。
- [ ] 总结关键技术：
  - [ ] HLS FFT IP；
  - [ ] 块浮点缩放；
  - [ ] LUT 映射乘法；
  - [ ] 多 AXI-MM 接口；
  - [ ] BRAM 缓冲；
  - [ ] SOCS 核时分复用。
- [ ] 列出贡献点，建议 4 点：
  - [ ] 系统框架贡献；
  - [ ] FPGA/HLS 在线重构流水线贡献；
  - [ ] 存储与数值实现策略贡献；
  - [ ] FPGA 平台验证与性能/能效贡献。
- [ ] 给出关键实验结果：
  - [ ] 250 MHz；
  - [ ] 10.57 ms；
  - [ ] 10 核 SOCS；
  - [ ] C++ SOCS 基准 3.37 倍加速；
  - [ ] 能效提升约 67.4 倍；
  - [ ] C/RTL RMSE $8.324 \times 10^{-7}$；
  - [ ] 板级 RMSE $2.93 \times 10^{-8}$；
  - [ ] 资源占用 17% LUT、9% FF、2% DSP、42% BRAM。
- [ ] 最后给出论文组织结构。

## 推荐贡献写法

- [ ] 贡献 1：

```text
提出一种面向 TCC-SOCS 在线空中像重构的 CPU-FPGA 协同计算框架，将离线 TCC 构建和 SOCS 本征核提取与在线 FPGA 重构加速解耦。
```

- [ ] 贡献 2：

```text
设计一种资源高效的 FPGA/HLS 在线重构流水线，集成频域嵌入、二维 IFFT、模平方加权累加和结果写回，并通过 SOCS 核间时分复用降低片上资源占用。
```

- [ ] 贡献 3：

```text
提出面向该数据流的存储与数值实现策略，通过块浮点 FFT 缩放、LUT 映射乘法、7 个 AXI-MM 接口和 BRAM 缓冲缓解计算资源、存储带宽和数值精度之间的矛盾。
```

- [ ] 贡献 4：

```text
在 Xilinx Kintex UltraScale+ xcku5p 上完成 C 仿真、C/RTL 联合仿真和板级验证，并从精度、延迟、资源利用率、加速比和能效等方面证明所提出架构的有效性。
```

---

# G. 引言中需要特别澄清的边界

- [ ] 明确本文加速对象是 **online SOCS reconstruction**。
- [ ] 明确本文不包含或不重点加速：
  - [ ] TCC 构建；
  - [ ] 特征分解；
  - [ ] 完整 OPC/SMO 系统；
  - [ ] Host-FPGA 全链路数据传输；
  - [ ] 主机侧傅里叶插值。
- [ ] 在引言中避免让读者误以为 10.57 ms 包括完整 TCC-SOCS 流程。
- [ ] 对 CPU/GPU 的比较保持克制：
  - [ ] 不说 GPU 一定不适合；
  - [ ] 改为“在低延迟、小批量、能耗受限或确定性执行场景下可能受限”。
- [ ] 强调 FPGA 是 CPU/GPU 之外的补充方案，而不是完全替代。

---

# H. 需要统一的术语

- [ ] 统一使用 “空中像计算” 或 “空中像仿真”，避免混用导致歧义。
- [ ] 统一使用 “在线重构阶段” 描述 FPGA 加速范围。
- [ ] 统一使用 “离线 TCC 构建与 SOCS 本征核提取” 描述 CPU 端任务。
- [ ] 统一使用 “SOCS 本征核” 或 “相干本征核”，全文保持一致。
- [ ] 统一使用 “2D IFFT” 或 “二维 IFFT”，中英文符号保持一致。
- [ ] 统一使用 “FPGA/HLS 架构” 描述本文系统。
- [ ] 统一使用 “AXI-MM master 接口” 或 “AXI-MM 接口”，全文保持一致。
- [ ] 统一说明 “10 核 SOCS” 指保留 10 个 SOCS 本征核。

---

# I. 需要补充或核查的数据

- [ ] 核查 10.57 ms 是否只对应 FPGA kernel 时间。
- [ ] 核查 C/RTL 周期数与 HLS estimated cycles 是否一致：
  - [ ] HLS：2,643,645 cycles；
  - [ ] C/RTL：2,651,856 cycles。
- [ ] 核查 250 MHz 下时间换算是否准确。
- [ ] 核查 3.37 倍加速比：
  - [ ] C++ SOCS 35.6 ms / FPGA 10.57 ms ≈ 3.37。
- [ ] 核查 45.3 倍、27.1 倍、4.28 倍加速比。
- [ ] 核查能效提升 67.4 倍计算：
  - [ ] CPU：28.09 frames/s / 80 W = 0.351 frames/J；
  - [ ] FPGA：94.6 frames/s / 4 W = 23.65 frames/J；
  - [ ] 23.65 / 0.351 ≈ 67.4。
- [ ] 核查资源利用率：
  - [ ] LUT 36,931 / 216,960 ≈ 17%；
  - [ ] FF 38,703 / 433,920 ≈ 9%；
  - [ ] DSP 34 / 1,824 ≈ 2%；
  - [ ] BRAM 399 / 960 ≈ 42%。
- [ ] 核查 RMSE 数值是否前后一致：
  - [ ] C 仿真：$2.93 \times 10^{-8}$；
  - [ ] C/RTL：$8.324 \times 10^{-7}$；
  - [ ] 板级：$2.93 \times 10^{-8}$。
- [ ] 核查 DSP 从 8,064 到 34 的对比基准：
  - [ ] 明确 8,064 是直接 DFT 或非 FFT 方案的估计；
  - [ ] 避免审稿人质疑来源不清。
- [ ] 核查 BRAM 从 1,366 到 399 的对比基准：
  - [ ] 明确是完全核间并行还是非复用方案；
  - [ ] 说明该对比是设计空间估计还是综合结果。

---

# J. 图表与引言之间的对应

- [ ] 第 1 段提到计算光刻流程时，可在后文图 1 对应说明。
- [ ] 第 2 段提到 TCC-SOCS 时，对应第 2 节公式和图 1。
- [ ] 第 3 段提到在线重构瓶颈时，对应延迟分解表和图 4。
- [ ] 第 4 段提到 FPGA 架构挑战时，对应图 2 和资源表。
- [ ] 第 5 段贡献中提到实验结果时，对应第 3 节结果表。

---

# K. 文献检索 Todo

## 计算光刻背景

- [ ] 检索 computational lithography 综述。
- [ ] 检索 OPC 经典/综述论文。
- [ ] 检索 SMO 经典/综述论文。
- [ ] 检索 ILT 经典/综述论文。
- [ ] 检索 aerial image simulation 在 OPC/SMO 中的计算瓶颈文献。

## TCC-SOCS 模型

- [ ] 检索 Hopkins TCC 原始或经典文献。
- [ ] 检索 TCC 在部分相干光刻成像中的应用文献。
- [ ] 检索 SOCS / sum of coherent systems 文献。
- [ ] 检索 TCC eigen decomposition / low-rank decomposition 文献。

## 软件和 GPU 加速

- [ ] 检索 GPU aerial image simulation。
- [ ] 检索 GPU OPC acceleration。
- [ ] 检索 GPU source mask optimization。
- [ ] 检索 FFT-based lithography simulation acceleration。
- [ ] 检索 CPU parallel lithography simulation。

## FPGA/HLS 相关

- [ ] 检索 HLS FFT FPGA 文献。
- [ ] 检索 2D FFT FPGA 架构文献。
- [ ] 检索 FPGA image processing high-level synthesis 文献。
- [ ] 检索 FPGA scientific computing energy efficiency 文献。
- [ ] 检索 FPGA computational lithography 或 lithography simulation，如果相关文献较少，可在文中说明该方向 underexplored。

---

# L. 写作风格 Todo

- [ ] 避免堆文献，按“方法类别”组织文献。
- [ ] 每一段结尾都要自然过渡到下一段。
- [ ] 使用克制表述：
  - [ ] “may be limited”
  - [ ] “can be challenging”
  - [ ] “remains underexplored”
  - [ ] “provides an alternative”
- [ ] 避免绝对化表述：
  - [ ] “GPU 不适合”
  - [ ] “CPU 无法满足”
  - [ ] “首次”
  - [ ] “完全解决”
- [ ] 如果使用“首次”或“首个”，必须确认文献检索充分。
- [ ] 引言中不要写过多实现细节，例如代码级 HLS pragma。
- [ ] 引言中保留核心数字，但详细表格放到实验部分。
- [ ] 保持“问题—空白—方法—验证”的逻辑链。

---

# M. 最终检查清单

- [ ] 引言是否清楚说明计算光刻和空中像计算的重要性？
- [ ] 是否解释了为什么选择 TCC-SOCS？
- [ ] 是否说明 SOCS 在线阶段仍是计算瓶颈？
- [ ] 是否合理比较 CPU、GPU、FPGA？
- [ ] 是否明确指出已有研究空白？
- [ ] 是否说明本文只加速在线重构阶段？
- [ ] 是否列出清晰、具体、可验证的贡献？
- [ ] 贡献点是否与实验结果一一对应？
- [ ] 文献是否覆盖背景、模型、加速平台和 FPGA/HLS？
- [ ] 数值结果是否前后一致？
- [ ] 术语是否统一？
- [ ] 引言读完后，读者是否能自然理解“为什么需要这篇 FPGA/HLS 加速架构论文”？