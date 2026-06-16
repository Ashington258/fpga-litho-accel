# FPGA Acceleration of TCC-SOCS Aerial Image Computation Using a Low-Latency and Energy-Efficient HLS Architecture

## Highlights

- A CPU-FPGA co-designed framework is proposed for online TCC-SOCS aerial image reconstruction in computational lithography.
- The offline TCC construction and SOCS kernel extraction are separated from the repeatedly invoked online reconstruction stage.
- A resource-efficient 128 x 128 two-dimensional IFFT pipeline is implemented using HLS FFT IP, block-floating-point scaling, LUT-based multiplication, and BRAM buffering.
- Seven independent AXI-MM memory interfaces are used to reduce data-access contention among mask spectra, SOCS kernels, weights, and output buffers.
- The proposed design achieves 10.57 ms latency at 250 MHz, 3.37x speedup over a C++ SOCS baseline, and approximately 67.4x higher energy efficiency.

## Abstract

Transmission cross coefficient and sum of coherent systems (TCC-SOCS) based aerial image computation is a key step in computational lithography, but its online reconstruction stage is repeatedly invoked in optical proximity correction and source-mask optimization flows and remains latency- and energy-sensitive on general-purpose processors. To address this issue, this paper presents a low-latency and energy-efficient FPGA/HLS architecture for TCC-SOCS aerial image reconstruction. The proposed CPU-FPGA co-design separates offline optical-system processing from online image reconstruction: the CPU performs TCC construction, eigen-decomposition, and SOCS kernel extraction, while the FPGA performs frequency-domain embedding, 128 x 128 two-dimensional inverse fast Fourier transform, weighted intensity accumulation, FFTshift, and output writing. To reduce resource consumption while preserving numerical accuracy, the design adopts Xilinx HLS FFT IP, block-floating-point scaling, LUT-based FFT multiplication, multi-port AXI-MM memory access, and BRAM-based intermediate buffering. Experimental results on a Xilinx Kintex UltraScale+ xcku5p device show that the proposed design computes a 10-kernel SOCS reconstruction in 10.57 ms at 250 MHz. Compared with MATLAB and C++ baselines, the architecture achieves up to 45.3x and 3.37x speedup, respectively. The C/RTL co-simulation RMSE reaches $8.324 \times 10^{-7}$, while board-level validation achieves $2.93 \times 10^{-8}$. The design uses 17% LUT, 9% FF, 2% DSP, and 42% BRAM, and improves energy efficiency by approximately 67.4x over the CPU baseline. These results demonstrate that FPGA-based SOCS reconstruction is a promising solution for low-power and low-latency lithography simulation.

**Keywords:** computational lithography; TCC-SOCS; aerial image computation; FPGA; HLS; two-dimensional IFFT; energy efficiency.

## 1. Introduction

Computational lithography has become an essential enabling technology for advanced semiconductor manufacturing. In optical proximity correction (OPC), source-mask optimization (SMO), and inverse lithography technology (ILT), aerial image simulation is repeatedly invoked to evaluate whether a mask pattern can generate the expected wafer pattern under a given illumination and projection system. As process windows shrink and layout complexity increases, the latency and energy cost of repeated aerial image evaluation become major bottlenecks in lithography optimization flows.

The Hopkins transmission cross coefficient (TCC) model provides a rigorous formulation for partially coherent imaging by incorporating the illumination source, pupil function, aberration, and mask spectrum into a frequency-domain operator [1]. Once the optical condition is fixed, the TCC can be precomputed and reused across different mask patterns. However, direct TCC-based imaging involves dense frequency-pair coupling and leads to high computational and storage costs. The sum of coherent systems (SOCS) method reduces this cost by decomposing the TCC matrix into a limited number of dominant coherent kernels, converting partially coherent imaging into the weighted summation of multiple coherent imaging systems.

Although SOCS significantly reduces algorithmic complexity, the online SOCS reconstruction stage still contains repeated frequency-domain multiplication, two-dimensional inverse FFT (2D IFFT), and pixel-wise intensity accumulation. These operations are regular and data-parallel, but they are executed for each retained SOCS kernel and for many mask windows in iterative OPC/SMO workflows. CPUs provide flexibility but limited energy efficiency, while GPUs provide high throughput but may require high power and introduce less deterministic latency. FPGA devices offer a different design point through customized pipelines, deterministic data movement, and low-power spatial computing.

This paper focuses on accelerating the online SOCS aerial image reconstruction stage after offline TCC decomposition. A CPU-FPGA co-designed architecture is proposed: the CPU performs optical-parameter parsing, TCC matrix construction, eigen-decomposition, and SOCS kernel generation; the FPGA accelerates the high-frequency online reconstruction path. The main contributions are as follows:

1. A CPU-FPGA co-designed framework for TCC-SOCS aerial image reconstruction is proposed, decoupling offline TCC decomposition from online FPGA acceleration.
2. A resource-efficient 2D IFFT data path is implemented using HLS FFT IP, block-floating-point scaling, and LUT-based FFT multiplication, reducing DSP usage from 8,064 to 34.
3. A multi-port DDR and BRAM buffering architecture is designed using seven AXI-MM interfaces to reduce memory contention and improve data-movement efficiency.
4. The architecture is validated through C simulation, C/RTL co-simulation, and board-level testing on a Kintex UltraScale+ FPGA, with quantitative analysis of accuracy, runtime, resource utilization, and energy efficiency.

The remainder of this paper is organized as follows. Section 2 introduces the TCC-SOCS imaging model and formulates the acceleration problem. Section 3 presents the proposed FPGA/HLS architecture. Section 4 reports experimental results and discusses accuracy, performance, resources, energy efficiency, and limitations. Section 5 concludes the paper.

## 2. Principle and Methods

### 2.1 Hopkins TCC Imaging Model

For partially coherent lithography imaging, the Hopkins formulation expresses aerial image intensity as a bilinear form of the mask spectrum. The transmission cross coefficient encodes the illumination and projection optics:

$$
TCC(f',g';f'',g'') =
\iint S(f_s,g_s)P(f'+f_s,g'+g_s)P^*(f''+f_s,g''+g_s)df_sdg_s,
$$

where $S$ denotes the source distribution and $P$ denotes the pupil function. Once optical parameters are fixed, the TCC can be precomputed. The image intensity can then be written as:

$$
I(x,y)=
\iiiint TCC(f',g';f'',g'')\hat{O}(f',g')\hat{O}^*(f'',g'')
e^{j2\pi[(f'-f'')x+(g'-g'')y]}df'dg'df''dg'',
$$

where $\hat{O}$ is the mask spectrum. This direct form is accurate but expensive because it involves dense coupling between frequency pairs.

### 2.2 SOCS Decomposition and Problem Formulation

The TCC matrix is Hermitian and can often be approximated by a low-rank decomposition:

$$
TCC \approx \sum_{k=1}^{N_k}\sigma_k\Phi_k\Phi_k^*,
$$

where $\sigma_k$ is the $k$-th eigenvalue weight, $\Phi_k$ is the corresponding frequency-domain coherent kernel, and $N_k$ is the number of retained SOCS kernels. Substituting this decomposition into the Hopkins equation gives:

$$
I(x,y) \approx
\sum_{k=1}^{N_k}\sigma_k
\left|\mathcal{F}^{-1}\{M(f_x,f_y)\Phi_k(f_x,f_y)\}\right|^2,
$$

where $M(f_x,f_y)$ denotes the mask spectrum. Thus, the online computation for each kernel consists of frequency-domain multiplication, IFFT, magnitude-square computation, and weighted accumulation.

In this work, the offline stage computes the SOCS kernels and eigenvalue weights on the CPU. The FPGA is responsible for the following online mapping:

$$
\hat{I}=f_{\mathrm{FPGA}}(M,\{\Phi_k,\sigma_k\}_{k=1}^{N_k}),
$$

where $\hat{I}$ is the reconstructed aerial image. The optimization objective is to reduce latency and energy consumption while keeping the hardware implementation error small relative to the software SOCS reference.

### 2.3 Proposed CPU-FPGA Framework

The proposed framework separates optical-system preprocessing from online reconstruction. The CPU performs configuration parsing, source generation, mask FFT, TCC construction, eigen-decomposition, and data formatting. The generated mask spectrum, SOCS kernels, and eigenvalue weights are stored in external DDR. The FPGA reads these data through AXI-MM interfaces and performs online reconstruction.

This partitioning follows the execution characteristics of the two stages. TCC construction and eigen-decomposition require high-precision matrix operations but are executed only when optical parameters change. In contrast, online SOCS reconstruction is repeatedly executed for different mask windows and is dominated by regular FFT and accumulation operations. Moving the online stage to FPGA provides direct latency and energy benefits while preserving CPU flexibility for offline preprocessing.

![Figure 1. TCC-SOCS computation workflow.](../image/论文/ch3_fig1_hopkins_workflow.png)

**Figure 1.** TCC-SOCS computation workflow. Offline optical-system processing produces SOCS kernels and eigenvalue weights; online reconstruction evaluates the aerial image for mask windows.

### 2.4 FPGA Online Reconstruction Pipeline

The FPGA pipeline contains five stages:

1. **Frequency embedding:** multiply the SOCS kernel with the corresponding central mask-spectrum window and embed the result into a fixed 128 x 128 FFT grid.
2. **2D IFFT:** perform row-wise FFT, matrix transpose, and column-wise FFT using HLS FFT IP.
3. **Weighted accumulation:** compute $|E_k|^2$ and accumulate $\sigma_k|E_k|^2$ into the temporary image buffer.
4. **FFTshift:** move the zero-frequency component to the image center through quadrant exchange.
5. **Output writing:** write the final 128 x 128 image to DDR through AXI-MM burst access.

For the default 10-kernel configuration, the five-stage path is executed sequentially for each kernel. This time-division multiplexing strategy reuses one 2D IFFT engine across kernels and avoids excessive BRAM usage.

![Figure 2. Proposed FPGA acceleration architecture.](../image/论文/ch4_fig1_fpga_architecture.png)

**Figure 2.** Proposed FPGA acceleration architecture for online TCC-SOCS aerial image reconstruction.

### 2.5 Frequency Embedding and Complex Multiplication

For the default configuration, the effective kernel size is 17 x 17, corresponding to $N_x=N_y=8$. Each kernel requires 289 complex multiplications between the mask spectrum and the SOCS kernel. The embedding module maps the result into the center of a 128 x 128 frequency grid and sets the remaining positions to zero. The complex multiplication is defined as:

$$
(a+jb)(c+jd)=(ac-bd)+j(ad+bc).
$$

The HLS implementation pipelines this loop with initiation interval close to one. Runtime parameters define the valid kernel region, while compile-time constants bound the loop to improve HLS scheduling determinism.

### 2.6 Two-Dimensional IFFT Engine

The 2D IFFT is the dominant computation stage. It is implemented using Xilinx HLS FFT IP with row-column decomposition. The input 128 x 128 matrix is processed row by row, stored in an intermediate BRAM buffer, and then processed column by column. A dual-port BRAM buffer implements the transpose access pattern.

The FFT IP is configured with 128-point transform length, natural output order, block-floating-point scaling, and LUT-based multiplication. Block-floating-point scaling dynamically shifts data only when overflow risk is detected and reports the accumulated exponent through `blk_exp`. The final conversion compensates this exponent. Compared with fixed per-stage scaling, this strategy preserves more effective precision for the odd FFT depth of $\log_2 128=7$.

### 2.7 Memory Architecture

The top-level HLS IP uses seven independent AXI-MM master interfaces:

| Interface | Buffer | Data | Depth | Access |
| --- | --- | --- | ---: | --- |
| gmem0 | `mskf_r` | mask spectrum real part | 1,048,576 | read |
| gmem1 | `mskf_i` | mask spectrum imaginary part | 1,048,576 | read |
| gmem2 | `scales` | SOCS eigenvalue weights | 32 | read |
| gmem3 | `krn_r` | kernel real part | 76,832 | read |
| gmem4 | `krn_i` | kernel imaginary part | 76,832 | read |
| gmem5 | `tmpImg_ddr` | intermediate image | 16,384 | write |
| gmem6 | `output` | final image | 16,384 | write |

Four major on-chip buffers are bound to BRAM: `fft_input`, `fft_output`, `tmpImg`, and `tmpImgp`. This reduces repeated DDR access during FFT and accumulation. The memory design separates large streaming inputs from small scalar parameters and output buffers, reducing bus contention and improving burst efficiency.

## 3. Simulation and Experiments

### 3.1 Experimental Setup

The proposed design is evaluated using Vitis HLS 2025.2 and Vivado 2025.2. The target FPGA is Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e. The default optical configuration is $L_x=L_y=1024$, $NA=0.8$, $\lambda=193$ nm, annular illumination with $\sigma_{in}=0.6$ and $\sigma_{out}=0.9$, and 10 SOCS kernels. The online FFT grid is fixed to 128 x 128.

The CPU baseline uses an Intel Xeon Platinum 8163 server. MATLAB and C++ implementations are used as software baselines. MATLAB provides the Golden Model and direct TCC/SOCS reference. The C++ implementation provides a more optimized single-precision CPU comparison.

| Category | Configuration |
| --- | --- |
| FPGA | Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e |
| FPGA resources | 960 BRAM_18K, 1,824 DSP, 433,920 FF, 216,960 LUT |
| HLS/Vivado | Vitis HLS 2025.2 / Vivado 2025.2 |
| Frequency | 250 MHz conservative evaluation |
| CPU baseline | Intel Xeon Platinum 8163 @ 2.50 GHz, 48 cores / 96 threads, 93 GB DDR4 |
| Optical configuration | $L_x=L_y=1024$, $NA=0.8$, $\lambda=193$ nm, annular source, $\sigma_{in}=0.6$, $\sigma_{out}=0.9$ |
| SOCS order | 10 kernels by default; 50 and 400 kernels for sensitivity analysis |
| FFT grid | 128 x 128 |

### 3.2 Accuracy Verification

The design is verified at three levels: C simulation, C/RTL co-simulation, and board-level validation. C simulation confirms algorithm-level correctness, C/RTL co-simulation validates the generated RTL timing behavior and AXI transactions, and board-level validation confirms the correctness of the deployed hardware output.

| Validation | RMSE | Comment |
| --- | ---: | --- |
| C simulation | $2.93 \times 10^{-8}$ | Algorithm-level HLS result |
| C/RTL co-simulation | $8.324 \times 10^{-7}$ | RTL-level result |
| Board validation | $2.93 \times 10^{-8}$ | Hardware output |

The error is mainly introduced by fixed-point quantization, block-floating-point scaling, and FFT butterfly rounding. The obtained RMSE remains below $10^{-5}$, indicating that the hardware implementation error is sufficiently small for the tested SOCS reconstruction workflow.

It is important to distinguish hardware implementation error from SOCS truncation error. Hardware implementation error compares FPGA output with the same SOCS software reference. SOCS truncation error compares a finite-kernel SOCS result with full TCC direct imaging. The latter depends on the number of retained kernels.

| SOCS kernels | RMSE vs full TCC | Interpretation |
| ---: | ---: | --- |
| 10 | $5.474 \times 10^{-3}$ | Low latency, moderate truncation error |
| 50 | $0.927 \times 10^{-3}$ | Better accuracy, higher latency |
| 400 | $2.57 \times 10^{-6}$ | Near full-rank reference |

The 10-kernel configuration is selected as the default because it provides a practical balance between latency and accuracy. Visual comparison also shows that the FPGA output preserves the main aerial-image structures and that the remaining error mainly appears near high-frequency mask edges.

![Figure 3. Visual comparison between reference and FPGA output.](../image/论文/ch5_fig4_visual_comparison.png)

**Figure 3.** Visual comparison of reference aerial image, FPGA output, and error distribution.

### 3.3 Runtime and Latency Breakdown

At 250 MHz, the HLS estimated latency is 2,643,645 cycles, corresponding to 10.57 ms. C/RTL co-simulation reports 2,651,856 cycles, which is close to the synthesis estimate. The small difference comes from protocol and scheduling overhead.

| Stage | Cycles | Time @250 MHz | Percentage |
| --- | ---: | ---: | ---: |
| Frequency embedding, 10 kernels | 167,450 | 0.67 ms | 6.3% |
| 2D IFFT, 10 kernels | 2,262,250 | 9.05 ms | 85.6% |
| Accumulation, 10 kernels | 164,250 | 0.66 ms | 6.2% |
| FFTshift | 16,389 | 0.066 ms | 0.6% |
| DDR output | 16,389 | 0.066 ms | 0.6% |
| **Total** | **2,643,645** | **10.57 ms** | **100%** |

The 2D IFFT accounts for approximately 85.6% of the total latency, confirming that FFT optimization is the primary performance lever. The latency reported here refers to FPGA online reconstruction and does not include offline TCC construction, eigen-decomposition, host-side preprocessing, or full-resolution Fourier interpolation.

![Figure 4. Latency breakdown of the proposed FPGA pipeline.](../image/论文/ch5_fig2_latency_breakdown.png)

**Figure 4.** Latency breakdown of the proposed FPGA online reconstruction pipeline.

### 3.4 Speedup Analysis

The FPGA runtime is compared with MATLAB and C++ baselines:

| Baseline | Runtime | FPGA runtime | Speedup |
| --- | ---: | ---: | ---: |
| MATLAB full TCC direct imaging | 479 ms | 10.57 ms | 45.3x |
| MATLAB 10-kernel SOCS | 287 ms | 10.57 ms | 27.1x |
| C++ full TCC direct imaging | 45.176 ms | 10.57 ms | 4.28x |
| C++ 10-kernel SOCS | 35.6 ms | 10.57 ms | 3.37x |

The comparison with C++ SOCS is the most conservative because it compares the same online reconstruction scope. Although 3.37x speedup is smaller than the MATLAB comparison, it is still meaningful because OPC/SMO workflows call aerial image computation repeatedly across many layout windows.

### 3.5 Resource Utilization

The final resource utilization on xcku5p is:

| Resource | Usage | Available | Utilization |
| --- | ---: | ---: | ---: |
| LUT | 36,931 | 216,960 | 17% |
| FF | 38,703 | 433,920 | 9% |
| DSP | 34 | 1,824 | 2% |
| BRAM_18K | 399 | 960 | 42% |

Two resource optimizations are central to the deployability of this architecture. First, replacing direct DFT with HLS FFT IP reduces DSP usage from 8,064 to 34, a 99.6% reduction. Second, using time-division multiplexing for SOCS kernels and a shared 2D IFFT engine reduces BRAM usage from 1,366 to 399, a 70.8% reduction. These optimizations allow the design to fit on a mid-range Kintex UltraScale+ device.

![Figure 5. Performance and resource summary.](../image/论文/ch5_fig3_performance_resource.png)

**Figure 5.** Performance and resource summary of the proposed FPGA/HLS architecture.

### 3.6 Energy Efficiency

The FPGA power is estimated at approximately 4 W, including static and dynamic components. The C++ CPU baseline platform power is estimated at 65-80 W. Based on 10.57 ms FPGA latency, the FPGA throughput is approximately 94.6 frames/s. Compared with the C++ SOCS throughput of approximately 28.09 frames/s at 80 W, the FPGA improves energy efficiency by approximately 67.4x.

| Platform | Runtime | Power | Throughput | Energy efficiency |
| --- | ---: | ---: | ---: | ---: |
| C++ CPU SOCS | 35.6 ms | about 80 W | 28.09 frames/s | 0.351 frames/J |
| FPGA SOCS | 10.57 ms | about 4 W | 94.6 frames/s | 23.7 frames/J |

This result indicates that the FPGA architecture is especially attractive for energy-constrained or latency-sensitive lithography simulation workloads.

### 3.7 Discussion and Limitations

The current design has several limitations. First, SOCS kernels are processed using time-division multiplexing rather than full kernel-level parallelism. This choice is constrained by the BRAM usage of the FFT IP. Second, the FFT grid is fixed to 128 x 128, which is efficient for the current 1024 x 1024 DUV configuration but may be suboptimal for smaller kernels or insufficient for larger High-NA EUV configurations. Third, the reported FPGA latency focuses on the online reconstruction kernel and does not include full host-FPGA data transfer, host-side Fourier interpolation, or complete OPC/SMO system integration. Fourth, the power numbers are estimates and should be refined using post-implementation power analysis and board-level measurements.

Despite these limitations, the architecture demonstrates that TCC-SOCS online reconstruction can be mapped efficiently to FPGA using HLS, provided that FFT resource consumption and memory bandwidth are carefully managed.

## 4. Conclusion

This paper presented a low-latency and energy-efficient FPGA/HLS architecture for TCC-SOCS aerial image reconstruction. The proposed CPU-FPGA co-design keeps TCC construction and SOCS kernel extraction on the CPU and accelerates the repeatedly invoked online reconstruction stage on FPGA. The hardware data path integrates frequency embedding, HLS FFT IP based 2D IFFT, weighted accumulation, FFTshift, and DDR output.

Through block-floating-point FFT scaling, LUT-based FFT multiplication, multi-port AXI-MM access, and BRAM buffering, the design significantly reduces resource usage while maintaining high numerical accuracy. On the xcku5p FPGA, the 10-kernel configuration achieves 10.57 ms latency at 250 MHz, C/RTL RMSE of $8.324 \times 10^{-7}$, and board-level RMSE of $2.93 \times 10^{-8}$. The final architecture uses 17% LUT, 9% FF, 2% DSP, and 42% BRAM. Compared with a C++ CPU SOCS baseline, it achieves 3.37x speedup and approximately 67.4x higher energy efficiency.

Future work will focus on increasing kernel-level parallelism on devices with larger BRAM capacity, supporting adaptive FFT grid sizes, integrating Fourier interpolation into the FPGA pipeline, and evaluating the system in larger OPC/SMO workflows with more diverse light sources and industrial mask patterns.

## Acknowledgements

To be completed according to the target journal requirements.

## Conflict of Interest Statement

The authors declare no conflict of interest.

## Author Contributions

To be completed according to the final author list.

## Data Availability

The data and code supporting this study are available from the corresponding author upon reasonable request. If the project repository is intended to be released, this statement should be updated with the repository link before submission.

## References

1. H. H. Hopkins, "On the diffraction theory of optical images," *Proceedings of the Royal Society of London. Series A. Mathematical and Physical Sciences*, vol. 217, no. 1130, pp. 408-432, 1953.
2. C. Mack, *Fundamental Principles of Optical Lithography: The Science of Microfabrication*. Wiley, 2008.
3. P. Yu and D. Z. Pan, "ELIAS: An accurate and extensible lithography aerial image simulator with improved numerical algorithms," *IEEE Transactions on Semiconductor Manufacturing*, vol. 22, no. 2, pp. 276-289, 2009.
4. J. Cong and Y. Zou, "FPGA-based hardware acceleration of lithographic aerial image simulation," *ACM Transactions on Reconfigurable Technology and Systems*, vol. 2, no. 3, pp. 1-29, 2009.
5. G. Chen, Z. Wang, B. Yu, et al., "Ultrafast source mask optimization via conditional discrete diffusion," *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems*, vol. 43, no. 7, pp. 2140-2150, 2024.
6. G. Chen, Z. Pei, H. Yang, et al., "Physics-informed optical kernel regression using complex-valued neural fields," in *Proceedings of the 60th ACM/IEEE Design Automation Conference*, 2023, pp. 1-6.
7. H. Tanabe, A. Jinguji, and A. Takahashi, "Accelerating EUV lithography simulation with weakly guiding approximation and STCC formula," in *International Conference on Extreme Ultraviolet Lithography 2023*, SPIE, vol. 12750, pp. 115-122, 2023.
8. Q. Jin, Q. Peng, Y. Liu, et al., "Recent advances in computational lithography technology," *Moore and More*, vol. 2, no. 1, pp. 1-18, 2025.
9. NVIDIA, "TSMC and Synopsys Bring Breakthrough NVIDIA Computational Lithography Platform to Production," 2024.
10. M. Lin, W. He, J. Liu, et al., "An Improved YOLOv5 Model for Lithographic Hotspot Detection," *Micromachines*, vol. 16, no. 5, 568, 2025.
