# CPU-SOCS 与 CPU-FPGA 流程对比图

本目录用于准备小论文中的“传统 CPU-SOCS 与本文 CPU-FPGA 协同 SOCS 空中像重构流程对比图”。

## 文件说明

- [01_流程图绘制步骤.md](01_流程图绘制步骤.md)：流程图表达目标、节点内容、布局和具体绘制步骤。
- [02_核心步骤图像清单.md](02_核心步骤图像清单.md)：主图必须使用的图像、辅助图像、现有素材来源和待生成文件名。
- [03_CPU_FPGA_SOCS流程对比.drawio](03_CPU_FPGA_SOCS流程对比.drawio)：可在 VS Code Draw.io 扩展中直接编辑的 SCI 风格双流程草图。当前使用矢量示意和文本节点，后续可将 `(a)` 至 `(e)` 的图像区域替换为真实中间结果。
- [04_CPU_FPGA_SOCS精简版.drawio](04_CPU_FPGA_SOCS精简版.drawio)：保留的对称五节点精简草图，适合核对相同 SOCS 数学步骤及 I01 至 I10 素材编号。
- [06_Linux主机出图交接.md](06_Linux主机出图交接.md)：Ubuntu + FPGA/XDMA 主机从 Golden、板上回读到统一 SCI 素材和回传包的完整操作手册。
- [08_CPU_FPGA_SOCS核心架构对比投稿版.drawio](08_CPU_FPGA_SOCS核心架构对比投稿版.drawio)：审稿优先版。将 CPU 路径压缩为逐核软件循环瓶颈，以 C1 至 C4 展开本文的离线/在线解耦、固定网格嵌入、面积优化二维 IFFT 和片上累加贡献，并为 I01、I03、I05、I07、I09、I10 标出具体贴图位置。

08 版用于让审稿人直接看到三点：两条流程使用相同 SOCS 输入和数学模型；FPGA 将固定网格嵌入、二维 IFFT 和加权累加映射为流水线；相同 10 核在线重构从 `35.6 ms` 降低到 `10.57 ms`。

## 平台职责

- Linux 主机是最终论文图数值数据的唯一生成平台，负责 CPU Golden、FPGA/XDMA、Host FI 和 I01 至 I10 纯数据图。
- Windows 工作站只负责 10 核拼版、裁剪留白、Draw.io 组合以及 PDF/SVG 导出。
- 最终素材必须从配置 JSON 和原始 float32 BIN 生成，不得从已有 PNG、论文图或比较图中裁剪。
- Windows 侧原型或临时可视化脚本不得作为投稿图的数据来源。

## 核心表达

两条流程采用相同的 SOCS 数学模型和输入输出定义：

$$
I(x,y)=\sum_{k=1}^{N_k}\sigma_k\left|\mathcal{F}^{-1}\{\hat{M}(u,v)\Phi_k(u,v)\}\right|^2.
$$

对比重点不是两种算法，而是相同在线重构算子的不同执行方式：

- 传统流程：CPU 通过软件循环完成逐核复乘、二维 IFFT、强度计算和加权累加。
- 本文流程：CPU 完成离线光学预处理，FPGA 使用固定 FFT 网格、二维 IFFT 数据通路和片上缓冲完成在线重构，Host 完成最终傅里叶插值。

## 推荐最终图名

> 传统 CPU-SOCS 与本文 CPU-FPGA 协同 SOCS 空中像重构流程对比

英文可写为：

> Comparison of the conventional CPU-SOCS flow and the proposed CPU-FPGA collaborative SOCS aerial-image reconstruction flow

## 出图前检查

1. CPU 与 FPGA 流程必须使用同一个输入掩模、相同的 10 个 SOCS 核和相同输出定义。
2. 所有热力图使用一致的尺寸、方向、色图和归一化规则。
3. FPGA 内核时间 `10.57 ms` 不包含 PCIe 和 Host FI；CPU 对比时间 `35.6 ms` 对应 10 核在线 SOCS 重构。
4. “硬件实现误差”和“有限核 SOCS 相对完整 TCC 的截断误差”必须分开描述。
5. 频域嵌入在图中先写为“固定 128×128 FFT 网格嵌入”。只有在 CPU Golden、HLS 和论文公式坐标一致后，才写成“中心嵌入”。
