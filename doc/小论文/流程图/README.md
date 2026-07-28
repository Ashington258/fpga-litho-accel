# CPU-SOCS 与 CPU-FPGA 流程对比图

本目录用于准备小论文中的“传统 CPU-SOCS 与本文 CPU-FPGA 协同 SOCS 空中像重构流程对比图”。

## 文件说明

- [01_流程图绘制步骤.md](01_流程图绘制步骤.md)：流程图表达目标、节点内容、布局和具体绘制步骤。
- [02_核心步骤图像清单.md](02_核心步骤图像清单.md)：主图必须使用的图像、辅助图像、现有素材来源和待生成文件名。
- [03_CPU_FPGA_SOCS流程对比.drawio](03_CPU_FPGA_SOCS流程对比.drawio)：可在 VS Code Draw.io 扩展中直接编辑的 SCI 风格双流程草图。当前使用矢量示意和文本节点，后续可将 `(a)` 至 `(e)` 的图像区域替换为真实中间结果。

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
