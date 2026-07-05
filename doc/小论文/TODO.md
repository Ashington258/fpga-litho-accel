# SCI 投稿前修改核对表

本文已具备较好的投稿基础：贡献点清晰，包含 CPU-FPGA 协同、离线/在线解耦、HLS FFT、块浮点缩放、多 AXI-MM、BRAM 缓冲、板级验证和能效对比。后续修改重点是强化创新定位、实验严谨性、图表质量和投稿规范。

## P0 必做：投稿接受率关键项

### 1. 创新性与贡献定位

- [x] 在 Introduction 中明确本文聚焦的是 **TCC-SOCS 在线重构阶段**，不是完整 TCC 构建、完整 OPC/SMO 系统或 Host-FPGA 全链路加速。
- [x] 增加或强化 research gap：现有 FPGA 光刻加速工作多为早期 aerial image 仿真或非 TCC-SOCS 流程，缺少面向在线 TCC-SOCS 重构的端到端 FPGA/HLS 架构与板级验证。
- [ ] 在 Related Work 中单独对比三类工作：
  - [ ] CPU/GPU TCC-SOCS 或空中像计算加速。
  - [ ] FPGA 二维 FFT / 图像计算加速。
  - [ ] AI surrogate / learning-based OPC、ILT 或 lithography simulation。
- [x] 在贡献列表中突出 4 个核心贡献：
  - [x] CPU-FPGA 离线/在线解耦框架。
  - [x] 面向 17 x 17 SOCS kernel 到 128 x 128 FFT 网格的频域嵌入与 HLS 流水线。
  - [x] HLS FFT IP、块浮点缩放、LUT 映射乘法、BRAM 累加缓冲和 7 个 AXI-MM 接口的资源/带宽优化。
  - [x] C 仿真、C/RTL 联合仿真、板级验证、ICCAD 多图形泛化和能效评估。
- [x] 避免把 MATLAB 完整 TCC 的 45.3x 加速作为唯一主卖点；正文中重点强调相对 C++ SOCS 基准的 3.37x 加速和约 67.4x 能效提升。
- [x] 增加一段 CPU/GPU/FPGA trade-off 讨论：GPU 适合高吞吐批处理，FPGA 适合低功耗、低延迟、确定性在线内核。

### 2. 功耗与能效验证

- [x] 明确当前功耗数据来源：Vivado 估算、后实现功耗分析、板级测量或其他来源。
- [ ] 若使用 Vivado Power Analyzer：
  - [ ] 写明工具版本、目标器件、时钟频率、切换率或 SAIF/VCD 来源。
  - [ ] 区分 static power、dynamic power 和 total power。
  - [ ] 说明是否包含 DDR、PCIe/JTAG、Host CPU 或仅 FPGA kernel。
- [ ] 若条件允许，补充板级实测功耗：
  - [ ] 记录测量设备或板卡监控接口。
  - [ ] 给出 idle power、kernel running power 和 dynamic delta。
  - [ ] 明确能效计算公式和单位，例如 images/s/W 或 J/image。
- [x] 统一 CPU 与 FPGA 能效对比口径：
  - [x] CPU 功耗来源。
  - [x] CPU 线程数与编译优化参数。
  - [x] 是否包含数据传输和 I/O。
- [x] 在实验节中补充一句限制说明：当前能效提升为估算值或板级实测值，适用范围是什么。

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
- [ ] 如条件允许，增加 GPU baseline 或说明无法公平复现 GPU 对比的原因。
- [x] 在 Discussion 中解释 3.37x 加速的工程意义：OPC/SMO 中大量窗口重复调用时，总延迟和能耗收益会累积。

## P1 强烈建议：提升论文完整度

### 4. 架构图美化

- [ ] 将当前图 2 改为白底 SCI 风格，不再使用黑底。
- [ ] 将图 2 拆成多子图：
  - [ ] (a) CPU-FPGA co-design overview。
  - [ ] (b) SOCS HLS IP online pipeline。
  - [ ] (c) AXI-MM / DDR memory organization。
- [ ] 在图中明确标注 Offline / Online 边界。
- [ ] 用实线箭头表示 AXI-MM burst 数据流，用虚线箭头表示 AXI-Lite control。
- [ ] 在 FPGA pipeline 中画出：
  - [ ] Kernel embedding, 17 x 17 -> 128 x 128。
  - [ ] Complex multiplication。
  - [ ] 128 x 128 2D IFFT。
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

### 5. 图表与表格统一

- [ ] 检查所有图号、表号和正文引用是否一致。
- [ ] 检查图 3、图 4 的 mask / SOCS / TCC / error heatmap 标注是否清晰。
- [ ] 图 3 或图 4 中若有色条，统一 colorbar 范围、字体和单位。
- [ ] 检查图 5 延迟分解是否与表 6 数值完全一致。
- [ ] 检查图 6 性能与资源总结是否与表格数据一致。
- [ ] 合并或精简重复的资源/性能对比表，避免表 7、表 9、表 10 信息重复。
- [ ] 所有表格统一有效数字格式：
  - [ ] latency 使用 ms。
  - [ ] cycles 使用整数。
  - [ ] RMSE 使用科学计数法。
  - [ ] resource utilization 使用百分比。

### 6. 方法节细化

- [ ] 规范 Hopkins / TCC / SOCS 公式的符号定义。
- [ ] 确保 $TCC$、$K_k$、$\sigma_k$、$M(f_x,f_y)$、$I(x,y)$ 等符号全文一致。
- [x] 增加在线重构伪代码或流程框，展示 k-loop、embedding、IFFT、weighted accumulation。
- [x] 补充 HLS 关键实现细节：
  - [x] 关键 loop pipeline。
  - [x] array partition / BRAM buffer。
  - [x] AXI-MM bundle 划分。
  - [x] FFT IP 配置。
  - [x] block floating scaling 设置。
- [x] 明确 17 x 17 kernel 尺寸由默认 DUV 参数推导得到。
- [x] 说明固定 128 x 128 FFT 网格的选择理由和限制。

### 7. 误差分析

- [ ] 区分三类误差来源：
  - [ ] SOCS 低秩截断误差。
  - [ ] HLS/FFT 块浮点缩放误差。
  - [ ] 板级数据搬运或格式转换误差。
- [x] 在实验节中明确说明硬件误差远小于 SOCS 截断误差。
- [x] 对 C sim、C/RTL cosim、board validation 的 RMSE、PSNR、SSIM 口径做统一说明。
- [ ] 讨论误差对下游 OPC/SMO 迭代的潜在影响。

## P2 可选增强：时间允许时补充

### 8. 敏感性分析

- [ ] 补充不同 SOCS kernel 数量下的延迟、精度和资源趋势。
- [ ] 若已有 50/400 kernel 数据，整理为 trade-off 曲线或表格。
- [ ] 补充不同 FFT 网格大小的讨论或实验：
  - [ ] 64 x 64。
  - [ ] 128 x 128。
  - [ ] 256 x 256。
- [ ] 若条件允许，补充不同光源配置：
  - [ ] Annular。
  - [ ] Dipole。
  - [ ] Quasar。
- [ ] 若条件允许，补充 High-NA EUV 参数下的可扩展性分析。

### 9. 可重复性材料

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

### 10. 投稿材料

- [ ] 确定目标期刊：
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

## 逐章修改核对

### Abstract

- [x] 按“问题-方法-结果-意义”压缩摘要。
- [x] 将关键量化结果前移：10.57 ms、3.37x、67.4x、RMSE、资源占用。
- [x] 弱化主观表述，尽量用数据支撑结论。
- [x] 关键词增加或检查：
  - [x] computational lithography。
  - [x] TCC-SOCS。
  - [x] aerial image simulation。
  - [x] FPGA。
  - [x] HLS。
  - [x] 2D IFFT。
  - [x] hardware acceleration。
  - [x] energy efficiency。

### Introduction / Related Work

- [x] 第一段明确计算光刻中空中像计算的瓶颈。
- [x] 第二段说明 TCC-SOCS 的准确性和在线计算负担。
- [x] 第三段引出 FPGA 的低延迟、低功耗和确定性优势。
- [x] 第四段明确现有工作的不足和本文 gap。
- [x] 贡献列表保持 3-4 条，不要过长。
- [ ] Related Work 减少与 Introduction 重复的内容。
- [ ] 更新 2025-2026 年 GPU/AI lithography acceleration 文献。

### Method

- [ ] 公式排版规范。
- [ ] 架构图更新为 SCI 风格。
- [x] 增加伪代码或时序图。
- [x] 补充 HLS pragma、FFT IP 和 AXI bundle 细节。
- [x] 说明资源优化设计选择的原因。

### Experiments

- [x] 实验环境描述完整。
- [x] baseline 描述完整。
- [x] 功耗估计或实测方法描述完整。
- [x] 延迟、资源、精度、能效指标口径统一。
- [ ] 图表数据互相一致。
- [x] 增加实验结果的解释，而不是只罗列数值。

### Discussion / Limitations

- [x] 单独讨论当前设计的限制：
  - [x] kernel 间采用时分复用。
  - [x] FFT 网格固定为 128 x 128。
  - [x] 未包含完整 Host-FPGA 数据传输。
  - [x] 功耗可能是估算值。
  - [x] 光源和工艺条件覆盖仍有限。
- [x] 说明这些限制为什么不影响本文核心结论。
- [x] 给出后续扩展方向：
  - [x] 更大 BRAM 器件上的 kernel 并行。
  - [x] 自适应 FFT 网格。
  - [x] 集成插值和后处理。
  - [x] 扩展至 Dipole、Quasar、High-NA EUV。

### Conclusion

- [x] 用 1 段总结方法。
- [x] 用 1 段总结关键结果。
- [x] 用 1 段说明应用价值和未来方向。
- [ ] 避免重复摘要中的全部数字。

## 最终提交前检查

- [ ] 全文术语统一：TCC-SOCS、SOCS eigen-kernel、aerial image reconstruction、online reconstruction。
- [ ] 全文单位统一：MHz、ms、W、J/image、RMSE、SSIM、PSNR。
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
