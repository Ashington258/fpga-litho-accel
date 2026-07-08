## Highlights

- A CPU-FPGA co-acceleration framework is presented for TCC-SOCS online aerial image reconstruction in computational lithography, decoupling low-frequency offline optical preprocessing from high-frequency online reconstruction while preserving model fidelity and configuration flexibility.
- A $17 \times 17$ SOCS coherent eigen-kernel is embedded into a fixed $128 \times 128$ FFT grid, and an area-optimized 2-D complex IFFT datapath is designed to reuse the same online reconstruction pipeline across different effective kernel sizes.
- Block floating-point dynamic range compensation, multiple independent AXI-MM interfaces, and BRAM buffering are combined to reduce resource usage and memory access contention in 2-D IFFT and multi-kernel accumulation.
- The architecture is validated on a Xilinx XDMA-based Host-FPGA PCIe platform, and the xcku5p implementation achieves a 10.57 ms 10-kernel online reconstruction latency, corresponding to a 3.37x speedup over the C++ SOCS baseline.


# A Low-Latency and Energy-Efficient FPGA/HLS Acceleration Architecture for TCC-SOCS Aerial Image Reconstruction

## Abstract

TCC-SOCS online aerial image reconstruction is a high-frequency computational kernel in OPC/SMO/ILT flows for computational lithography. Conventional CPU/GPU platforms are often constrained by throughput and power budgets in low-power and deterministic-latency deployment scenarios. This paper presents a CPU-FPGA collaborative acceleration architecture implemented with FPGA/HLS for TCC-SOCS online reconstruction. Offline TCC construction, eigendecomposition, and SOCS eigen-kernel extraction are retained on the CPU, while frequency-domain embedding, 2-D IFFT, weighted intensity accumulation, FFTshift, and result write-back are mapped to an FPGA online pipeline. Under the default DUV configuration, the architecture embeds $17 \times 17$ SOCS eigen-kernels into a fixed $128 \times 128$ FFT grid and employs block floating-point dynamic range compensation, multiple independent AXI-MM interfaces, and BRAM buffering to construct an area-optimized datapath. On a Xilinx Kintex UltraScale+ xcku5p device, the 10-kernel online reconstruction kernel achieves a latency of 10.57 ms at 250 MHz, yielding a 3.37x kernel-level speedup over the C++ SOCS baseline. A single-window on-board validation path, including PCIe data transfer, control configuration, FPGA execution, result readback, and host-side Fourier interpolation, takes 66.89 ms. C/RTL co-simulation yields an RMSE of $8.324 \times 10^{-7}$, the PCIe on-board $128 \times 128$ output yields an RMSE of $2.93 \times 10^{-8}$, and the final $1024 \times 1024$ aerial image after host-side Fourier interpolation yields an RMSE of $2.95 \times 10^{-8}$. Across ten ICCAD layout benchmarks, 10-kernel SOCS achieves an average RMSE of $5.50 \times 10^{-3}$ and an average SSIM of 0.9965 relative to full TCC imaging. The final design uses 17% LUTs, 9% FFs, 2% DSPs, and 42% BRAMs. Using an estimated 4 W FPGA kernel power envelope and an 80 W CPU power envelope, the kernel-level energy efficiency improves by approximately 67.4x. These results demonstrate that the proposed CPU-FPGA collaborative architecture enables low-latency and energy-efficient TCC-SOCS online aerial image reconstruction while preserving SOCS model fidelity and providing a transparent on-board validation path.

**Keywords:** computational lithography; TCC-SOCS; partially coherent imaging; aerial image reconstruction; hardware/software co-design; FPGA; HLS; energy-efficient acceleration.

## 1. Introduction

Computational lithography has become a critical bridge between optical imaging, process-window control, and layout optimization in advanced semiconductor manufacturing. As technology nodes continue to scale, improving resolution solely through projection optics becomes increasingly difficult. Optical proximity correction (OPC), source-mask optimization (SMO), and inverse lithography technology (ILT) are therefore widely used to compensate for proximity effects, optimize illumination and mask geometries, and enlarge manufacturable process windows [1]-[11]. These methods typically operate as model-feedback or inverse-optimization frameworks and repeatedly invoke aerial image computation across many layout windows, process conditions, and optimization iterations [2], [3], [5], [8]. As the process window shrinks, mask complexity increases, and optimization variables evolve from edge movement to pixelated or curvilinear geometries, the latency, throughput, and energy consumption of aerial image computation become major bottlenecks in computational lithography deployment.

Partially coherent lithographic imaging is commonly formulated based on Hopkins theory. The transmission cross coefficient (TCC) represents the illumination source, projection pupil, aberrations, thin-film effects, and mask spectrum, also referred to as the diffracted-order map, as a unified frequency-domain operator [12]-[18]. When optical parameters are fixed, the TCC can be precomputed offline and reused across different mask patterns. Direct TCC imaging, however, involves dense coupling between frequency pairs and therefore incurs high computation and storage costs. The sum of coherent systems (SOCS) method applies eigendecomposition or low-rank approximation to the TCC, transforming partially coherent imaging into a weighted summation of coherent eigen-kernels. This preserves the physical interpretability of the model while reducing online computational complexity [12], [13], [16], [19]. Consequently, TCC-SOCS has become an important approach for balancing accuracy and efficiency in aerial image reconstruction.

SOCS converts the dense frequency coupling in direct TCC imaging into a finite weighted sum of coherent systems. Nevertheless, the online reconstruction stage after offline decomposition remains a high-frequency computational hotspot. For each SOCS eigen-kernel, online reconstruction requires pointwise multiplication between the mask spectrum and the eigen-kernel, 2-D IFFT, magnitude-square evaluation, eigenvalue weighting, and multi-kernel intensity accumulation. These operations are amplified by the number of eigen-kernels, the number of layout windows, and the number of OPC/SMO/ILT iterations [19]-[23]. In batched window evaluation, process-window scanning, or iterative mask optimization, SOCS eigen-kernels, eigenvalues, and control configurations can be reused across many windows under the same optical condition. Reducing the latency and energy per online reconstruction is therefore valuable not only for single-frame acceleration but also for batched layout-window pipelines, distributed critical-dimension verification, and system-level energy budgets.

Prior work has improved lithography simulation or mask optimization through numerical approximation, symmetry exploitation, CPU/GPU acceleration, and learning-based models [20], [22]-[30]. CPUs provide high programmability, but their energy efficiency is limited for regular frequency-domain workloads. GPUs provide high throughput in large-batch scenarios, but their hundreds-of-watts system power, host-side scheduling overhead, and nondeterministic interrupt latency can limit applicability in low-power critical-dimension verification, distributed edge nodes, or compact hardware-integrated systems. FPGAs offer a complementary option for TCC-SOCS online reconstruction. Complex multiplication, 2-D IFFT, magnitude-square computation, and accumulation exhibit regular datapaths and explicit dependencies, making them suitable for deterministic hardware pipelines. On-chip BRAM can cache intermediate matrices to reduce DDR round trips, while multiple AXI-MM interfaces can mitigate memory access contention among the mask spectrum, SOCS eigen-kernels, eigenvalues, and output buffers.

Existing studies have demonstrated the potential of FPGAs for lithographic aerial image simulation and 2-D FFT acceleration, and have shown that external memory access, row-column transposition, parallelism, and energy efficiency are central trade-offs in FFT-style hardware accelerators [31]-[38]. However, most computational lithography acceleration studies focus on algorithmic approximation, CPU/GPU implementation, learning-based OPC/ILT, or FPGA lithography simulation outside the TCC-SOCS online-reconstruction setting. Dedicated FPGA/HLS datapaths, Host-FPGA XDMA data movement, real hardware control paths, and end-to-end error validation for TCC-SOCS online reconstruction remain insufficiently integrated in the literature. In other words, how to balance latency, bandwidth, precision, resources, and system-integration overhead for TCC-SOCS online reconstruction under limited FPGA resources has not been fully addressed.

This paper focuses on the online reconstruction stage after offline TCC construction and SOCS eigen-kernel extraction. It does not attempt to accelerate full TCC generation, eigendecomposition, or a complete OPC/SMO optimization toolchain. For this online stage, we present a CPU-FPGA collaborative architecture. The CPU performs offline TCC construction, eigendecomposition, SOCS eigen-kernel generation, and host-side Fourier interpolation, while the FPGA performs online frequency-domain embedding, $128 \times 128$ 2-D IFFT, weighted accumulation, FFTshift, and result write-back. PCIe XDMA is used to integrate Host-FPGA data movement, control configuration, and result readback into a full-platform validation flow. The main contributions are as follows:

1. A CPU-FPGA collaborative computing framework is proposed for TCC-SOCS online reconstruction, explicitly decoupling offline optical preprocessing, online FPGA reconstruction, and host-side post-processing.
2. A reconstruction pipeline is designed for embedding $17 \times 17$ SOCS eigen-kernels into a $128 \times 128$ FFT grid, followed by 2-D IFFT, intensity accumulation, FFTshift, and result write-back.
3. An area-optimized 2-D complex IFFT processor is developed for the DUV cutoff-frequency regime and a fixed FFT grid. It combines block floating-point dynamic range compensation, conflict-free transposition memory, LUT-mapped multiplication, BRAM buffering, and multiple independent AXI-MM interfaces to reduce resource usage and memory access contention.
4. A PCIe XDMA-based Host-FPGA full-platform validation flow is implemented, covering input-data writes, AXI-Lite control configuration, FPGA execution, $128 \times 128$ result readback, and $1024 \times 1024$ host-side Fourier interpolation.

The proposed architecture is evaluated through C simulation, C/RTL co-simulation, PCIe on-board validation, ICCAD multi-pattern testing, and software baseline comparison. Experimental results show that the 10-kernel SOCS online reconstruction kernel achieves a latency of 10.57 ms at 250 MHz, a 3.37x speedup over the C++ SOCS baseline, and an estimated kernel-level energy-efficiency improvement of approximately 67.4x. The PCIe single-window on-board validation path takes 66.89 ms; this profile provides end-to-end profiling visibility from DMA transfer and control configuration to hardware execution and host-side Fourier interpolation, and is analyzed in Section 3 as a system-integration overhead that can be amortized in batched processing. The C/RTL RMSE is $8.324 \times 10^{-7}$, the PCIe on-board $128 \times 128$ output RMSE is $2.93 \times 10^{-8}$, and the resource utilization is 17% LUTs, 9% FFs, 2% DSPs, and 42% BRAMs. The rest of this paper is organized as follows. Section 2 introduces the TCC-SOCS model and the proposed architecture. Section 3 presents the experimental results. Section 4 concludes the paper.

## 2. Principles and Methods

### 2.1 TCC-SOCS Imaging Model and Problem Definition

For partially coherent lithographic imaging, the Hopkins formulation expresses aerial image intensity as a bilinear form of the mask spectrum. The transmission cross coefficient encodes the illumination and projection optical system:

$$
TCC(f',g';f'',g'') =
\iint S(f_s,g_s)P(f'+f_s,g'+g_s)P^*(f''+f_s,g''+g_s)df_sdg_s.
\tag{1}
$$

Here, $S$ denotes the source distribution and $P$ denotes the pupil function. When optical parameters are fixed, the TCC can be precomputed. The corresponding image intensity is

$$
I(x,y)=
\iiiint TCC(f',g';f'',g'')\hat{O}(f',g')\hat{O}^*(f'',g'')
e^{j2\pi[(f'-f'')x+(g'-g'')y]}df'dg'df''dg''.
\tag{2}
$$

Here, $\hat{O}$ is the mask spectrum. This direct form is accurate, but it is computationally expensive because it involves dense coupling among frequency pairs.

The TCC matrix is Hermitian and can usually be approximated by a low-rank decomposition:

$$
TCC \approx \sum_{k=1}^{N_k}\sigma_k\Phi_k\Phi_k^*.
\tag{3}
$$

Here, $\sigma_k$ is the $k$-th eigenvalue and $\Phi_k$ is the corresponding frequency-domain coherent eigen-kernel. $N_k$ denotes the number of retained SOCS eigen-kernels. Substituting this decomposition into the Hopkins formulation gives

$$
I(x,y) \approx
\sum_{k=1}^{N_k}\sigma_k
\left|\mathcal{F}^{-1}\{M(f_x,f_y)\Phi_k(f_x,f_y)\}\right|^2.
\tag{4}
$$

Here, $M(f_x,f_y)$ denotes the mask spectrum. Thus, the online computation for each eigen-kernel consists of frequency-domain multiplication, IFFT, magnitude-square computation, and weighted accumulation.

In the discrete implementation, the effective TCC frequency range is determined by the pupil cutoff frequency:

$$
N_x=N_y=\left\lfloor \frac{L_x\cdot NA\cdot(1+\sigma_{out})}{\lambda}\right\rfloor .
\tag{5}
$$

The corresponding 2-D eigen-kernel size is $(2N_x+1)\times(2N_y+1)$, and the discrete TCC matrix size is $N_f\times N_f$, where $N_f=(2N_x+1)(2N_y+1)$. Under the default DUV configuration with $L_x=L_y=1024$, $NA=0.8$, $\lambda=193$ nm, and $\sigma_{out}=0.9$, we have $N_x=N_y=8$. Therefore, the eigen-kernel size is $17 \times 17$ and $N_f=289$. This quantitative mapping determines the input window size of the subsequent FPGA frequency-domain embedding module.

In this paper, the offline stage on the CPU computes the SOCS eigen-kernels and their eigenvalues. The FPGA performs the following online mapping:

$$
\hat{I}=f_{\mathrm{FPGA}}(M,\{\Phi_k,\sigma_k\}_{k=1}^{N_k}).
\tag{6}
$$

Here, $\hat{I}$ is the reconstructed aerial image. The optimization objective is to reduce online computation latency and energy consumption while keeping hardware implementation error small relative to the software SOCS reference.

### 2.2 CPU-FPGA Collaborative Framework and Online Pipeline

The proposed framework separates optical-system preprocessing from online reconstruction. The CPU handles configuration parsing, source generation, mask FFT, TCC construction, eigendecomposition, and data formatting. The generated mask spectrum, SOCS eigen-kernels, and eigenvalues are written to FPGA-side DDR through PCIe XDMA. The FPGA reads these data through AXI-MM interfaces and executes online reconstruction, then writes the $128 \times 128$ intermediate aerial image back to DDR. The host reads back this result through PCIe and performs Fourier interpolation to recover the $1024 \times 1024$ aerial image.

This partitioning follows the distinct execution characteristics of the two stages. TCC construction and eigendecomposition require high-precision matrix operations but are only executed when optical parameters change. In contrast, online SOCS reconstruction is repeatedly invoked for different mask windows and is dominated by regular FFT and accumulation operations. Migrating the online stage to the FPGA therefore maps the high-frequency computational hotspot to a deterministic pipeline while retaining CPU flexibility for offline preprocessing.

Table 1 summarizes the task boundary in the collaborative framework. Optical-system information, including the source, pupil, and aberrations, is encapsulated in the SOCS eigen-kernels and eigenvalues. The FPGA online stage only processes the mask spectrum, eigen-kernels, and eigenvalues.

**Table 1** Task partitioning in the CPU-FPGA collaborative framework

| Execution side | Main tasks | Computational characteristics |
| --- | --- | --- |
| Host CPU | Parameter parsing, source generation, mask FFT, TCC construction, eigendecomposition, eigen-kernel export, Fourier-interpolation post-processing | High-precision matrix operations, executed when optical parameters change |
| PCIe/XDMA | Write mask spectrum, eigen-kernels, and eigenvalues to DDR; configure AXI-Lite control registers; read back $128 \times 128$ result | Data movement and control between host and FPGA |
| FPGA | Frequency-domain embedding, 2-D IFFT, intensity accumulation, FFTshift, result write-back | Regular dataflow, frequently executed for mask windows |

![Fig. 1 TCC-SOCS computational flow](../image/论文/ch3_fig1_hopkins_workflow.png)

**Fig. 1** TCC-SOCS computational flow. Offline optical-system processing generates SOCS eigen-kernels and eigenvalues, while the online reconstruction stage computes the aerial image for each mask window.

**Host-FPGA PCIe on-board validation flow.** The on-board system uses Xilinx XDMA character devices for data transfer between the host and FPGA. The host writes the real and imaginary parts of the $1024 \times 1024$ mask spectrum, the real and imaginary parts of ten $17 \times 17$ SOCS eigen-kernels, and ten eigenvalues into specified DDR addresses. It then writes $N_k$, $N_x$, $N_y$, $L_x$, $L_y$, and buffer addresses through AXI-Lite registers, and asserts the start signal to launch the HLS IP. After computation, the host reads back the $128 \times 128$ scaled aerial image $I_{128\times128}$ and performs Fourier interpolation to obtain the $1024 \times 1024$ aerial image. This flow covers data writing, control configuration, computation launch, result readback, host-side post-processing, and reference comparison, thereby validating system-level deployability.

**Table 2** PCIe/XDMA platform-level data layout

| Data object | Direction | Address | Data size | Data type | Description |
| --- | --- | --- | ---: | --- | --- |
| Mask spectrum real part $M_{\mathrm{Re}}$ | H2C | `0x40000000` | 4,194,304 B | IEEE-754 single-precision float | $1024 \times 1024$ real part |
| Mask spectrum imaginary part $M_{\mathrm{Im}}$ | H2C | `0x40400000` | 4,194,304 B | IEEE-754 single-precision float | $1024 \times 1024$ imaginary part |
| Eigenvalues $\sigma_k$ | H2C | `0x40800000` | 40 B | IEEE-754 single-precision float | Ten SOCS eigenvalues |
| Eigen-kernel real part $\Phi_{\mathrm{Re}}$ | H2C | `0x40880000` | 11,560 B | IEEE-754 single-precision float | $10 \times 17 \times 17$ real part |
| Eigen-kernel imaginary part $\Phi_{\mathrm{Im}}$ | H2C | `0x40900000` | 11,560 B | IEEE-754 single-precision float | $10 \times 17 \times 17$ imaginary part |
| Intermediate accumulation image $I_{\mathrm{acc}}$ | H2C | `0x40980000` | 65,536 B zero-fill | IEEE-754 single-precision float | $128 \times 128$ accumulation buffer |
| Output image $I_{128\times128}$ | H2C | `0x40990000` | 65,536 B zero-fill | IEEE-754 single-precision float | $128 \times 128$ output buffer |
| FPGA output $I_{128\times128}$ | C2H | `0x40990000` | 65,536 B | IEEE-754 single-precision float | Read back and interpolated by the host to $1024 \times 1024$ |

**FPGA online reconstruction pipeline.** The FPGA pipeline consists of five stages:

1. **Frequency-domain embedding:** The SOCS eigen-kernel is multiplied with the corresponding central window of the mask spectrum, and the product is embedded into a fixed $128 \times 128$ FFT grid.
2. **2-D IFFT:** A block floating-point 2-D complex IFFT processor performs row-wise FFT, conflict-free matrix transposition, and column-wise FFT sequentially.
3. **Weighted accumulation:** The design computes $|E_k|^2$ and accumulates $\sigma_k|E_k|^2$ into a temporary image buffer.
4. **FFTshift:** Quadrant swapping moves the zero-frequency component to the image center.
5. **Output write-back:** The final $128 \times 128$ image is written back to DDR through AXI-MM burst transactions.

From the data-dependency perspective, the online stage only depends on the mask spectrum, SOCS eigen-kernels, and eigenvalues. Optical information such as the source, pupil, and aberrations has already been compressed into $\Phi_k$ and $\sigma_k$ in the offline stage. For the $k$-th eigen-kernel, the FPGA first performs pointwise complex multiplication between the mask spectrum and the eigen-kernel within the effective frequency window, then center-embeds the result into a $128 \times 128$ frequency grid:

$$
F_k(c_x+u,c_y+v)=M(c_x+u,c_y+v)\Phi_k(u,v),
\quad |u|\leq N_x,\ |v|\leq N_y .
\tag{7}
$$

The 2-D IFFT then produces the coherent field $E_k$, and its intensity $|E_k|^2$ is accumulated into the output buffer with eigenvalue weight $\sigma_k$. After all retained eigen-kernels have been processed, the hardware applies FFTshift to the accumulated image and writes it to external memory. Therefore, the FPGA online stage can be expressed as

$$
\hat{I}=\mathrm{FFTshift}\left(\sum_{k=1}^{N_k}\sigma_k
\left|\mathcal{F}^{-1}\{F_k\}\right|^2\right).
\tag{8}
$$

For the default 10-kernel configuration, the five-stage path is executed sequentially for each eigen-kernel. This inter-kernel time-multiplexing strategy reuses a single 2-D IFFT engine across eigen-kernels and avoids excessive BRAM consumption. Full kernel-level parallelism could provide higher throughput, but each parallel kernel would require an independent $128 \times 128$ 2-D IFFT instance and transposition buffer. Based on the resource model of the current HLS FFT IP, 10-kernel full parallelism would require approximately 3000 BRAM_18K blocks, about 3.1 times the 960 BRAM_18K blocks available on the xcku5p. Therefore, we adopt a balanced design that combines inter-kernel time multiplexing with intra-kernel pipelining.

The host configures the number of kernels $N_k$, effective frequency ranges $N_x,N_y$, mask dimensions $L_x,L_y$, and all AXI-MM pointer addresses through the AXI-Lite interface. Under the fixed $128 \times 128$ IFFT grid, different effective kernel sizes can reuse the same hardware configuration through zero-padding and center embedding, avoiding repeated synthesis for different optical configurations.

![Fig. 2 Proposed FPGA acceleration architecture](../image/论文/ch4_fig1_fpga_architecture.png)

**Fig. 2** FPGA acceleration architecture for online TCC-SOCS aerial image reconstruction.

### 2.3 Key Hardware Modules and Memory Architecture

Under the default configuration, the effective eigen-kernel size is $17 \times 17$, corresponding to $N_x=N_y=8$. Each eigen-kernel requires 289 complex multiplications between the mask spectrum and the SOCS eigen-kernel. The embedding module maps the multiplication results to the center of a $128 \times 128$ frequency grid and zero-pads all other locations. Complex multiplication is defined as

$$
(a+jb)(c+jd)=(ac-bd)+j(ad+bc).
\tag{9}
$$

The corresponding HLS loop is pipelined with an initiation interval close to 1. Runtime parameters define the actual effective eigen-kernel region, while the $17 \times 17$ maximum window is used as the synthesis boundary to improve HLS scheduling determinism. The embedding location is referenced to the mask-spectrum center $L_x/2,L_y/2$, ensuring spatial phase alignment between the FPGA output and the CPU reference.

The control interface receives runtime parameters including $N_k$, $N_x$, $N_y$, $L_x$, and $L_y$. The real and imaginary parts of the mask spectrum, the real and imaginary parts of the eigen-kernels, the eigenvalues, the intermediate image, and the output image are mapped to independent AXI-MM access channels. Initialization, frequency-domain embedding, intensity accumulation, and write-back are all pipelined. Major 2-D buffers are bound to dual-port BRAM to support FFT transposition access and on-chip accumulation.

**2-D IFFT engine.** The 2-D IFFT is the dominant computational stage. We construct an area-optimized 2-D complex IFFT processor for a fixed $128 \times 128$ grid and implement it through row-column decomposition. The input matrix first undergoes row-wise 1-D IFFT. The results are then converted to a column-wise access sequence through an on-chip conflict-free transposition memory architecture, followed by column-wise 1-D IFFT. The transposition memory separates real and imaginary parts and binds them to dual-port BRAM, enabling row writes and column reads under a fixed access pattern without port conflicts while reducing external DDR round trips.

The underlying 1-D FFT is configured for 128-point transform length, natural-order output, block floating-point scaling, and LUT-mapped multiplication. Inputs and outputs use the `ap_fixed<32,1>` fixed-point format. Under Xilinx HLS semantics, this is a 32-bit two's-complement fixed-point format with a total width of 32, an integer width of 1, a fractional width of 31, and a quantization step of approximately $2^{-31}$. This quantization semantics covers the typical numerical ranges of the mask spectrum, kernel coefficients, frequency-domain products, and IFFT outputs in this design. Block floating-point dynamic range compensation shifts data only when overflow risk exists and outputs an accumulated scaling exponent. The final conversion compensates according to

$$
E_{\mathrm{float}}=E_{\mathrm{fixed}}\cdot 2^{\mathrm{blk\_exp}}.
\tag{10}
$$

Compared with fixed stage-by-stage scaling, this strategy preserves more effective precision for the odd FFT depth of $\log_2 128=7$. Multiplications in butterfly operations are mapped to LUTs to reduce DSP usage.

The 2-D IFFT uses row-column decomposition. First, the 1-D FFT IP is invoked sequentially for 128 rows, and the results are written to the on-chip transposition buffer. The buffer is then read column by column and the 1-D FFT IP is invoked again. The scaling exponent returned by each 1-D FFT is accumulated into the total 2-D transform scaling exponent and compensated uniformly during output conversion. Therefore, block floating-point scaling error does not continuously accumulate through fixed right shifts at every stage.

**Memory architecture.** The top-level HLS IP uses seven independent AXI-MM master interfaces:

| Channel | Data path | Data | Depth | Access |
| --- | --- | --- | ---: | --- |
| 0 | Mask spectrum real part | Mask spectrum real part | 1,048,576 | Read |
| 1 | Mask spectrum imaginary part | Mask spectrum imaginary part | 1,048,576 | Read |
| 2 | Eigenvalues | SOCS eigenvalues | 32 | Read |
| 3 | Eigen-kernel real part | Eigen-kernel real part | 76,832 | Read |
| 4 | Eigen-kernel imaginary part | Eigen-kernel imaginary part | 76,832 | Read |
| 5 | Intermediate image buffer | Intermediate image | 16,384 | Write |
| 6 | Final image buffer | Final image | 16,384 | Write |

Major on-chip buffers are bound to BRAM to reduce repeated DDR access during FFT and accumulation. This memory design separates large streaming inputs, small scalar parameters, and output buffers, reducing bus congestion and improving burst-access efficiency.

For reproducibility, Table 3 lists the key HLS and interface configurations. External DDR data use IEEE-754 single-precision floating point. Inside the FFT IP, complex data use the `ap_fixed<32,1>` fixed-point format, whose quantization semantics is a 32-bit two's-complement representation with 1 integer bit and 31 fractional bits. The block floating-point mode records the total scaling amount through `blk_exp`, and compensates the result back to floating point according to (10).

**Table 3** Key HLS/IP implementation configuration

| Category | Configuration |
| --- | --- |
| Top module | TCC-SOCS online reconstruction HLS kernel |
| FFT IP | 128-point fixed length, $\log_2 N=7$, single channel, pipelined streaming I/O, natural-order output |
| FFT numerical format | Input/output `ap_fixed<32,1>`, 32-bit two's-complement fixed point, 1-bit integer width and 31-bit fractional precision; phase factor width 24; block floating-point scaling; truncation rounding |
| FFT storage and multiplication | Data, twiddle-factor, and reorder memories use BRAM; complex multiplication and butterfly operations are mapped to LUTs |
| Main loop optimizations | Frequency-domain zeroing, embedding, intensity accumulation, FFTshift, format conversion, and DDR write-back loops use `PIPELINE II=1` |
| Kernel-loop constraint | SOCS kernel loop uses the default 10-kernel synthesis boundary and supports runtime kernel-count configuration |
| On-chip buffers | Input/output complex buffers, accumulated image buffer, scaled image buffer, and FFT intermediate transposition buffer are bound to dual-port BRAM |
| AXI-MM read channels | Mask-spectrum real/imaginary read burst length 64, outstanding 8; eigen-kernel real/imaginary read burst length 32, outstanding 4 |
| AXI-MM write channels | Intermediate accumulated image and output image write burst length 64, outstanding 4 |
| AXI-Lite control | $N_k$, $N_x$, $N_y$, $L_x$, $L_y$, and all AXI-MM pointer addresses are configured by the host |

## 3. Simulation and Experiments

### 3.1 Experimental Setup and Accuracy Validation

The proposed design is evaluated using Vitis HLS 2025.2 and Vivado 2025.2. The target FPGA is a Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e device. The default optical configuration is $L_x=L_y=1024$, $NA=0.8$, $\lambda=193$ nm, annular illumination, $\sigma_{in}=0.6$, and $\sigma_{out}=0.9$, with 10 retained SOCS eigen-kernels. The online FFT grid is fixed at $128 \times 128$.

The CPU baseline platform uses an Intel Xeon Platinum 8163 server running Ubuntu 24.04 LTS with GCC 13.3.0. The C++ reference program uses C++17, single-precision floating point, and the O2 optimization option, and links FFTW 3.x, FFTW threads, LAPACK, BLAS, and OpenMP runtime libraries. MATLAB and C++ implementations serve as software baselines. MATLAB provides a high-precision reference and direct TCC/SOCS reference results, while the C++ implementation provides a more optimized software comparison. For performance comparison, this paper reports two scopes. The first is the FPGA online reconstruction kernel latency, used for comparison with the same 10-kernel online SOCS computation stage in the C++ baseline. The second is the PCIe single-window on-board validation path, used to evaluate the system-integration overhead formed by input transfer, control configuration, on-board computation polling, result readback, and host-side Fourier interpolation. MATLAB full TCC results primarily demonstrate the physical reference chain and algorithm-level difference, and are not used as the primary like-for-like hardware acceleration baseline. The main experimental configuration is shown in Table 4.

**Table 4** Experimental platform and default optical configuration

| Category | Configuration |
| --- | --- |
| FPGA | Xilinx Kintex UltraScale+ xcku5p-ffvb676-2-e |
| FPGA resources | 960 BRAM_18K, 1,824 DSPs, 433,920 FFs, 216,960 LUTs |
| HLS/Vivado | Vitis HLS 2025.2 / Vivado 2025.2 |
| Frequency | 250 MHz timing-robust operating point |
| PCIe link | Xilinx XDMA Reference Driver v2020.2.2; XDMA interface configured as PCIe Gen3 x8; latest on-board test negotiated Gen3 x8 |
| Software baseline CPU | Intel Xeon Platinum 8163 @ 2.50 GHz, 48 cores / 96 threads, 93 GB DDR4 |
| On-board validation host | Intel Xeon E3-1200 v3/4th Gen Haswell platform, ASUS Z97 motherboard |
| Optical configuration | $L_x=L_y=1024$, $NA=0.8$, $\lambda=193$ nm, annular source, $\sigma_{in}=0.6$, $\sigma_{out}=0.9$ |
| SOCS order | Default 10 kernels; truncation-error comparison uses 50 and 400 kernels |
| FFT grid | $128 \times 128$ |

RMSE, PSNR, and SSIM are used to evaluate numerical error and image consistency, as defined in (11)-(13). Given a verified image $X$ and a reference image $Y$, RMSE is defined as

$$
\mathrm{RMSE}(X,Y)=\sqrt{\frac{1}{N}\sum_{i=1}^{N}(X_i-Y_i)^2}.
\tag{11}
$$

PSNR is normalized by the peak reference-image value $Y_{\max}=\max_i |Y_i|$:

$$
\mathrm{PSNR}=20\log_{10}\frac{Y_{\max}}{\mathrm{RMSE}}.
\tag{12}
$$

SSIM is computed using standard luminance, contrast, and structural terms:

$$
\mathrm{SSIM}(X,Y)=
\frac{(2\mu_X\mu_Y+C_1)(2\sigma_{XY}+C_2)}
{(\mu_X^2+\mu_Y^2+C_1)(\sigma_X^2+\sigma_Y^2+C_2)}.
\tag{13}
$$

The design is validated at four levels: C simulation, C/RTL co-simulation, PCIe on-board $128 \times 128$ output validation, and PCIe+Host FI $1024 \times 1024$ final aerial-image validation. C simulation confirms algorithm-level correctness. C/RTL co-simulation verifies the timing behavior and AXI transactions of the generated RTL. PCIe on-board validation further confirms real data movement, control-register access, and hardware output correctness. The validation results are shown in Table 5.

**Table 5** Error validation results for the FPGA/HLS implementation. All errors are compared with reference results under the same SOCS configuration unless otherwise specified.

| Validation mode | Comparison target | RMSE | Main error source |
| --- | --- | ---: | --- |
| C simulation | Double-precision CPU SOCS reference | $2.93 \times 10^{-8}$ | HLS C-model data format and operator implementation |
| C/RTL co-simulation | Double-precision CPU SOCS reference | $8.324 \times 10^{-7}$ | RTL scheduling, block floating-point scaling, and FFT rounding |
| PCIe on-board $128 \times 128$ output | $128 \times 128$ software reference image $I_{128}$ | $2.93 \times 10^{-8}$ | XDMA data transfer, AXI-Lite configuration, and hardware write-back |
| PCIe+Host FI $1024 \times 1024$ | 10-kernel SOCS reference aerial image | $2.95 \times 10^{-8}$ | On-board output readback and host-side Fourier-interpolation format conversion |

The errors in Table 5 are hardware numerical errors or system-integration errors. They mainly originate from fixed-point quantization, block floating-point scaling, FFT butterfly rounding, and data-format conversion between the host and FPGA. All RMSE values are below $10^{-5}$, indicating that hardware implementation error is sufficiently small in the tested SOCS reconstruction flow.

It is important to distinguish hardware implementation error from SOCS truncation error. Hardware implementation error compares the FPGA output with the software reference under the same SOCS configuration. SOCS truncation error compares finite-kernel SOCS results with full direct TCC imaging, and depends on the number of retained eigen-kernels. Truncation errors under different SOCS kernel counts are shown in Table 6. If the final PCIe+Host FI aerial image is directly compared with the full TCC result, the RMSE is $5.474 \times 10^{-3}$; this error is dominated by 10-kernel SOCS low-rank truncation rather than FPGA numerical implementation.

**Table 6** Imaging error relative to full TCC under different SOCS kernel counts. The default optical configuration is annular DUV, and direct full TCC imaging is used as the reference.

| Number of SOCS kernels | RMSE relative to full TCC | Interpretation |
| ---: | ---: | --- |
| 10 | $5.474 \times 10^{-3}$ | Low latency, moderate truncation error |
| 50 | $0.927 \times 10^{-3}$ | Higher accuracy, increased latency |
| 400 | $2.57 \times 10^{-6}$ | Close to full-rank reference |

The default on-board validation uses the 10-kernel configuration to keep online kernel latency at 10.57 ms while preserving the major aerial image structures. The 50-kernel and 400-kernel results demonstrate that SOCS truncation error decreases as the number of kernels increases. Because the current FPGA datapath uses inter-kernel time multiplexing, larger kernel counts would approximately linearly increase online reconstruction latency. Visual comparisons also show that the FPGA output preserves the major aerial image structures, while the residual error is mainly distributed around high-frequency mask edges.

In addition to RMSE, image-quality metrics are used to evaluate the consistency between the FPGA output and the MATLAB high-precision reference. Under the 10-kernel configuration, C simulation reports an RMSE of $2.93 \times 10^{-8}$, C/RTL co-simulation reports an RMSE of $8.324 \times 10^{-7}$, the maximum absolute error is below $1.0 \times 10^{-5}$, PSNR exceeds 120 dB, and SSIM is above 0.9999. With a threshold of $I_{th}=0.225$, the binarized contour consistency exceeds 99.99%. These results indicate that hardware quantization and block floating-point scaling errors are far smaller than SOCS truncation error and do not alter the primary imaging structures.

To evaluate robustness across different mask geometries, ten ICCAD 2013 benchmark cases are also examined. Table 7 reports quantitative results under the same optical configuration and 10-kernel SOCS setting, while Fig. 3 shows the corresponding mask patterns, SOCS imaging results, and full TCC references. The RMSE ranges from $5.38 \times 10^{-3}$ to $5.61 \times 10^{-3}$, PSNR is above 48 dB for all cases, and SSIM is above 0.996 for all cases. The runtime variation across different benchmarks is below 2%, indicating that the online-stage complexity is mainly determined by the kernel count and FFT grid size rather than the specific mask geometry.

**Table 7** Generalization results of 10-kernel SOCS imaging across different mask patterns. All results use the same optical configuration and are compared with full TCC reference results.

| Benchmark | RMSE ($\times10^{-3}$) | PSNR (dB) | SSIM |
| --- | ---: | ---: | ---: |
| T1 | 5.47 | 48.37 | 0.9966 |
| T2 | 5.52 | 48.21 | 0.9964 |
| T3 | 5.38 | 48.52 | 0.9968 |
| T4 | 5.61 | 48.05 | 0.9962 |
| T5 | 5.44 | 48.41 | 0.9967 |
| T6 | 5.55 | 48.18 | 0.9963 |
| T7 | 5.49 | 48.32 | 0.9965 |
| T8 | 5.58 | 48.12 | 0.9961 |
| T9 | 5.42 | 48.45 | 0.9967 |
| T10 | 5.51 | 48.25 | 0.9964 |

Across the ten cases, the average RMSE is $5.497 \times 10^{-3}$ with a standard deviation of $0.068 \times 10^{-3}$; the average PSNR is 48.29 dB with a standard deviation of 0.14 dB; and the average SSIM is 0.99647 with a standard deviation of 0.00022. These statistics indicate that 10-kernel SOCS truncation error varies only slightly across different mask patterns, and that the accuracy variation in the current experiments is mainly caused by the low-rank approximation order rather than the FPGA datapath.

![Fig. 3(a) ICCAD 2013 benchmark mask patterns](../image/论文/ch5_fig7_mask_patterns.png)

![Fig. 3(b) 10-kernel SOCS aerial image results](../image/论文/ch5_fig7_socs_aerial.png)

![Fig. 3(c) Full TCC reference aerial image results](../image/论文/ch5_fig7_tcc_aerial.png)

**Fig. 3** Generalization validation across different mask patterns. (a) ICCAD 2013 benchmark mask patterns; (b) 10-kernel SOCS aerial image results; (c) full TCC reference aerial image results.

Fig. 4 further compares the MATLAB high-precision reference, FPGA output, and error distribution for the default benchmark. The FPGA output preserves the major aerial image structures, and residuals are concentrated in high-frequency regions around mask edges, consistent with the spatial distribution of SOCS truncation error.

![Fig. 4 Visual comparison between reference result and FPGA output](../image/论文/ch5_fig4_visual_comparison.png)

**Fig. 4** Comparison of reference aerial image, FPGA output, and error distribution.

### 3.2 Runtime and Speedup Analysis

At 250 MHz, the HLS estimated latency is 2,643,645 cycles, corresponding to 10.57 ms. The C/RTL co-simulation report gives 2,651,856 cycles, which is close to the synthesis estimate. The small difference mainly comes from protocol and scheduling overhead. The latency breakdown is shown in Table 8 and Fig. 5.

**Table 8** Latency breakdown of the 10-kernel SOCS online reconstruction kernel. This table reports kernel-only latency at 250 MHz and excludes PCIe transfer and host-side FI.

| Stage | Cycles | Time at 250 MHz | Ratio |
| --- | ---: | ---: | ---: |
| Frequency-domain embedding, 10 kernels | 167,450 | 0.67 ms | 6.3% |
| 2-D IFFT, 10 kernels | 2,262,250 | 9.05 ms | 85.6% |
| Accumulation, 10 kernels | 164,250 | 0.66 ms | 6.2% |
| FFTshift | 16,389 | 0.066 ms | 0.6% |
| DDR output | 16,389 | 0.066 ms | 0.6% |
| **Total** | **2,643,645** | **10.57 ms** | **100%** |

The 2-D IFFT accounts for approximately 85.6% of the total latency, indicating that FFT optimization is the primary performance lever. The latency reported here refers to the FPGA online reconstruction kernel and excludes offline TCC construction, eigendecomposition, Host-FPGA PCIe transfer, and host-side Fourier interpolation. This scope is used to evaluate the computational efficiency of the HLS datapath itself.

To guarantee deterministic timing closure and robust operation under process-voltage-temperature (PVT) variations, the target clock is conservatively constrained to 250 MHz. This frequency serves as the unified performance scope for C/RTL co-simulation, synthesis reports, and on-board validation, ensuring that block floating-point exponent compensation, fixed-to-floating conversion, and transposition access paths are evaluated under deployable hardware constraints. The reported 10.57 ms is therefore an online-kernel latency under timing-closure constraints, rather than a theoretical frequency estimate that ignores implementation boundaries.

![Fig. 5 Latency breakdown of the proposed FPGA pipeline](../image/论文/ch5_fig2_latency_breakdown.png)

**Fig. 5** Latency breakdown of the proposed FPGA online reconstruction pipeline.

Furthermore, PCIe on-board validation is performed on real hardware, as reported in Table 9. The table gives a single-window profiling path, including input write, output-buffer zero-fill, AXI-Lite configuration, on-board computation polling, result readback, and host-side Fourier interpolation. Configuration loading, reference-data loading, file saving, reference comparison, and visualization are validation-script overheads and are excluded from the application path. The on-board XDMA interface is configured as PCIe Gen3 x8. In the latest test, the host enumeration reports an actual negotiated link of Gen3 x8, with LaneErrStat equal to 0. The measured H2C input write transfers 8.02 MiB in 13.82 ms, and the $128 \times 128$ result readback transfers 64 KiB in 0.29 ms. This timing is measured from a single application path in the Python validation script and includes system calls, XDMA character-device access, and host scheduling overhead; it should not be interpreted as a controlled x2-versus-x8 raw-link bandwidth comparison. The host-polling computation stage is measured as 20.52 ms and includes time quantization induced by a 10 ms polling interval, so its meaning differs from the HLS kernel-cycle estimate in Table 8.

**Table 9** Latency breakdown of the PCIe single-window on-board validation path. The negotiated on-board link in this test is PCIe Gen3 x8.

| Stage | Data size | Measured time | Ratio in application path |
| --- | ---: | ---: | ---: |
| PCIe H2C input data write | 8.02 MiB | 13.82 ms | 20.7% |
| PCIe H2C output-buffer zero-fill | 128.00 KiB | 0.23 ms | 0.3% |
| AXI-Lite configuration | - | 1.02 ms | 1.5% |
| FPGA computation and host polling | - | 20.52 ms | 30.7% |
| PCIe C2H readback of $128 \times 128$ output | 64.00 KiB | 0.29 ms | 0.4% |
| Host-side Fourier interpolation | $128 \times 128$ to $1024 \times 1024$ image | 31.00 ms | 46.4% |
| **Application-path total** | - | **66.89 ms** | **100%** |

The single-window profile in Table 9 shows that host-side Fourier interpolation and on-board computation polling together account for 77.0% of the application path. PCIe H2C transfer is dominated by two 4 MiB mask-spectrum arrays, whose throughput in this single-run application profile is 696.2 MiB/s and 512.5 MiB/s, respectively. Eigen-kernels and eigenvalues are much smaller and have limited impact on total latency. The latest link status is Gen3 x8, confirming normal link negotiation and data correctness. Nevertheless, the transfer times in Table 9 should be interpreted as end-to-end script timings under the specific host, driver, and XDMA software path, rather than as raw PCIe x2/x8 bandwidth comparisons.

From a decoupled analysis perspective, the 66.89 ms in Table 9 is a single-window on-board diagnostic profile. It provides end-to-end visibility from host data transfer and control configuration to hardware computation and host-side FI. The kernel speedup still uses the kernel-only latency from Table 8. In practical OPC/SMO batched-window processing, SOCS eigen-kernels, eigenvalues, and AXI-Lite configurations under the same optical setting can be reused across windows. Mask-spectrum inputs can be streamed in batches, and host-side Fourier interpolation can overlap with FPGA computation for the next window. Let $T_{\mathrm{setup}}$ denote one-time configuration and reusable data-write overhead, $T_{\mathrm{stream}}$ denote unavoidable per-window input/output streaming overhead, and $T_{\mathrm{kernel}}=10.57$ ms denote FPGA online reconstruction kernel latency. The amortized latency for processing $B$ windows can be written as

$$
T_{\mathrm{avg}}(B)=\frac{T_{\mathrm{setup}}}{B}
+\max\left(T_{\mathrm{kernel}},T_{\mathrm{stream}},T_{\mathrm{FI}}\right).
\tag{14}
$$

As $B$ increases, $T_{\mathrm{setup}}/B$ approaches zero, and system throughput is determined by the slowest pipeline stage. Based on the current measurements, if host-side Fourier interpolation remains at 31.00 ms, the upper bound of the batched path without FI migration is approximately 32.3 windows/s. If FI post-processing is migrated to the FPGA or overlapped with other CPU-side tasks, the dominant throughput term can return to the 10.57 ms online kernel, corresponding to approximately 94.6 windows/s. Therefore, 66.89 ms is better interpreted as a system-integration overhead and a guide for future optimization, whereas the 3.37x kernel speedup and 67.4x kernel-level energy-efficiency improvement reflect the hardware benefit of the online SOCS reconstruction operator itself.

Table 10 compares FPGA kernel runtime with MATLAB and C++ baselines. The table compares either the same online reconstruction operator or a physical reference computation stage. All speedups are computed using the 10.57 ms FPGA kernel-only latency. The 66.89 ms PCIe single-window validation path is analyzed as system-integration overhead in Table 9 and (14), and is not mixed into the pure software computation speedup table.

**Table 10** Runtime and speedup comparison between FPGA and software baselines. Speedups are computed using the 10.57 ms FPGA kernel-only latency.

| Baseline | Software compute time | FPGA kernel time | Kernel speedup |
| --- | ---: | ---: | ---: |
| MATLAB full TCC direct imaging | 479 ms | 10.57 ms | 45.3x |
| MATLAB 10-kernel SOCS | 287 ms | 10.57 ms | 27.1x |
| C++ full TCC direct imaging | 45.176 ms | 10.57 ms | 4.28x |
| C++ 10-kernel SOCS | 35.6 ms | 10.57 ms | 3.37x |

The comparison with C++ SOCS is the strictest software baseline because both evaluate the same online reconstruction scope. Although the 3.37x speedup is smaller than the MATLAB-based comparison, OPC/SMO flows invoke aerial image computation at high frequency across many layout windows, allowing the kernel-level benefit to accumulate in batched throughput and energy efficiency.

From a platform-positioning perspective, the advantage of the FPGA primarily lies in low power and deterministic latency, rather than absolute throughput against high-end GPUs. For a single 10-kernel SOCS imaging kernel, MATLAB CPU takes approximately 287 ms, C++ CPU takes approximately 35.6 ms, and the proposed FPGA takes 10.57 ms. Representative GPU lithography platforms can often achieve higher absolute throughput, but their power commonly reaches hundreds of watts. The proposed FPGA design achieves a kernel throughput of 94.6 frames/s under an estimated 4 W power envelope, making it more suitable for low-power, low-latency, or embedded deployment scenarios. The PCIe single-window validation result further indicates that host-side post-processing, polling granularity, and data-reuse strategies should be co-optimized during system deployment.

### 3.3 Resources, Energy Efficiency, and Limitations

The final resource utilization on xcku5p is shown in Table 11.

**Table 11** FPGA resource utilization on xcku5p. Results are from Vitis HLS/Vivado implementation reports for the target device.

| Resource | Used | Available | Utilization |
| --- | ---: | ---: | ---: |
| LUT | 36,931 | 216,960 | 17% |
| FF | 38,703 | 433,920 | 9% |
| DSP | 34 | 1,824 | 2% |
| BRAM_18K | 399 | 960 | 42% |

Two resource optimizations are critical to the deployability of the architecture. First, replacing direct DFT with the HLS FFT IP reduces DSP usage from 8,064 to 34, a 99.6% reduction. Second, time-multiplexing SOCS eigen-kernels and sharing the 2-D IFFT engine reduces BRAM usage from 1,366 to 399, a 70.8% reduction. The HLS memory report shows that each of ten major $128 \times 128$ IEEE-754 single-precision floating-point/fixed-point on-chip buffers consumes 30 BRAM_18K blocks, the 2-D FFT submodule consumes 86 BRAM_18K blocks, and AXI master interface buffers together consume approximately 12 BRAM_18K blocks. Thus, the current resource bottleneck is the 2-D FFT and its transposition/accumulation buffers, rather than DSP usage. These optimizations allow the design to fit into a mid-range Kintex UltraScale+ device.

![Fig. 6 Performance and resource summary](../image/论文/ch5_fig3_performance_resource.png)

**Fig. 6** Performance and resource summary of the proposed FPGA/HLS architecture.

**Energy-efficiency analysis.** The FPGA power is approximately 4 W, derived from Vivado/Vitis post-synthesis power estimation and device static-power estimation. The static power is approximately 1.5 W, and the dynamic power is approximately 2.5 W. Because a complete power measurement based on SAIF/VCD activity files or on-board sensors is not yet available, this value is strictly treated as the estimated power of the FPGA online reconstruction kernel. If Vivado Power Analyzer is used in the final submission version, the tool version, target clock, junction temperature, voltage, default or measured toggle rate, and whether simulation activity files are used should be reported. Table 12 compares this kernel-level scope with the C++ CPU SOCS baseline. The C++ CPU baseline platform power is estimated from a server-processor operating range of 65-80 W, and Table 12 uses 80 W as the power scope for energy-efficiency calculation. Based on the 10.57 ms FPGA kernel latency, the FPGA throughput is approximately 94.6 frames/s. Compared with the C++ SOCS throughput of approximately 28.09 frames/s under 80 W, the FPGA kernel-level energy efficiency improves by approximately 67.4x. The PCIe single-window on-board validation path has completed latency and correctness validation, but full-system energy efficiency must further include the Host CPU, XDMA/PCIe, DDR, and host-side Fourier-interpolation power. This paper does not extrapolate the 4 W kernel power to full-system power.

**Table 12** Energy-efficiency comparison between FPGA and C++ CPU SOCS. FPGA power is kernel-level estimated power and does not represent Host+PCIe+DDR full-system power.

| Platform | Runtime | Power | Throughput | Energy efficiency |
| --- | ---: | ---: | ---: | ---: |
| C++ CPU SOCS | 35.6 ms | approx. 80 W | 28.09 frames/s | 0.351 frames/J |
| FPGA SOCS kernel | 10.57 ms | approx. 4 W | 94.6 frames/s | 23.7 frames/J |
| PCIe single-window validation path | 66.89 ms | To be measured | 14.95 frames/s | To be measured |

This result indicates that the proposed FPGA architecture is particularly suitable for energy-constrained or latency-sensitive lithography simulation workloads.

Table 13 positions the proposed design relative to other computing platforms.

**Table 13** Latency and energy-efficiency positioning of different computing platforms. The GPU row uses public platform-level metrics and is included only for positioning, not as a same-code-path measurement.

| Platform | Latency | Power | Energy efficiency | Note |
| --- | ---: | ---: | ---: | --- |
| MATLAB CPU SOCS | 287 ms | approx. 80 W | 0.044 frames/J | Double-precision software baseline |
| C++ CPU SOCS | 35.6 ms | approx. 80 W | 0.351 frames/J | Single-precision software baseline |
| Representative GPU lithography platform | approx. 5 ms | approx. 300 W | approx. 0.67 frames/J | High throughput, high power; public platform-level metric |
| Proposed FPGA SOCS kernel | 10.57 ms | approx. 4 W | 23.7 frames/J | Low latency, low power, deterministic execution |
| Proposed PCIe single-window validation path | 66.89 ms | To be measured | To be measured | Includes PCIe transfer and host-side FI |

The GPU row is used only for platform-level positioning and is not a strict same-code-path or same-hardware-environment comparison.

**Discussion and limitations.** The current design has several limitations. First, SOCS eigen-kernels are processed through time multiplexing rather than full inter-kernel parallelism; this choice is constrained by the BRAM usage of the FFT IP. Resource estimation indicates that xcku5p can support roughly a parallelism of two, while higher parallelism requires a device with larger BRAM capacity. Second, the FFT grid is fixed at $128 \times 128$. This reduces online-path reconfiguration for the current $1024 \times 1024$ DUV configuration, but it may waste resources for smaller eigen-kernels and may be insufficient for larger High-NA EUV configurations. Third, although Host-FPGA PCIe integration has been completed, the current system still performs Fourier interpolation from $128 \times 128$ to $1024 \times 1024$ on the host, and on-board runtime is affected by host polling granularity. Future work can migrate FI post-processing to the FPGA, replace coarse polling with interrupts, or use double-buffered DMA and multi-board parallelism to hide transfer overhead in batched-window processing. Fourth, this work has not integrated a complete OPC/SMO optimization loop. The current validation target remains a single-window aerial image reconstruction and host post-processing path. For full-chip applications, the proposed kernel should be considered as a low-power online operator in a batched-window pipeline rather than as a competitor to GPU clusters in absolute full-chip simulation throughput. Fifth, the reported power is an estimated kernel-level value. Future work should refine it through post-implementation power analysis and on-board measurements, and should include Host, PCIe, and DDR power for full-system reporting. Sixth, the experiments mainly cover an annular source and typical DUV parameters; future studies should extend the evaluation to dipole, quasar, High-NA EUV, and larger production-grade mask layouts.

These limitations mainly affect system-level throughput, generalization across optical configurations, and absolute power accuracy. They do not change the main conclusion regarding the online reconstruction kernel and Host-FPGA integration: under fixed optical configurations and batched small-window repeated evaluation, the main online operator of TCC-SOCS can be mapped to a regular, low-power, and deterministic-latency FPGA datapath, and can be integrated into a host-side dataflow through PCIe XDMA. The results show that, with appropriate management of FFT resource consumption, memory bandwidth, batched data reuse, and host-side post-processing overhead, TCC-SOCS online reconstruction can be efficiently mapped through HLS into a deployable CPU-FPGA collaborative system.

## 4. Conclusion

This paper presents a low-latency and energy-efficient FPGA/HLS architecture for TCC-SOCS aerial image reconstruction. The central idea is to retain optical-system-dependent TCC construction and SOCS eigen-kernel extraction on the CPU, while mapping the high-frequency online reconstruction stage in OPC/SMO-style flows to a dedicated FPGA datapath. PCIe XDMA connects input writes, AXI-Lite control, result readback, and host-side Fourier interpolation into an integrated execution path. Through this partitioning, the complex optical model is compressed into reusable eigen-kernels and eigenvalues, while the FPGA only needs to execute regular computations such as frequency-domain embedding, 2-D IFFT, weighted accumulation, FFTshift, and result write-back. The TCC-SOCS online hotspot is thereby transformed into a pipelinable and deployable hardware task.

Experimental results validate this partitioning from three perspectives: hardware numerical error, SOCS truncation error, and the PCIe single-window on-board validation path. With block floating-point FFT scaling, LUT-mapped FFT multiplication, multi-port AXI-MM access, and BRAM buffering, the design achieves a 10-kernel online reconstruction kernel latency of 10.57 ms at 250 MHz on the xcku5p FPGA. Hardware-related RMSE values in C/RTL, PCIe on-board output, and PCIe+Host FI validation are all below $10^{-5}$. The PCIe single-window validation path takes 66.89 ms, with host-side Fourier interpolation and on-board computation polling together accounting for 77.0%; in batched-window scenarios, this overhead can be further amortized through configuration reuse, DMA double buffering, FI migration, and pipeline overlap. The final architecture uses 17% LUTs, 9% FFs, 2% DSPs, and 42% BRAMs, and achieves a 3.37x kernel-level speedup and an approximately 67.4x estimated kernel-level energy-efficiency improvement over the C++ CPU SOCS baseline. For the large number of repeated small-window aerial image evaluations in computational lithography, these results indicate that FPGAs are well suited for low-power and deterministic-latency TCC-SOCS online reconstruction kernels, and can be integrated into host-side validation flows through PCIe.

The current validation target is the TCC-SOCS online reconstruction kernel and its PCIe on-board profiling path. This paper does not claim end-to-end acceleration of a complete OPC/SMO toolchain. Future deployment can increase inter-kernel parallelism on devices with larger BRAM capacity, support adaptive FFT grid sizes, integrate Fourier interpolation and post-processing into the FPGA pipeline, optimize PCIe batched transfer and interrupt control, and extend the evaluation to dipole, quasar, High-NA EUV, and larger production-grade mask layouts. The proposed architecture can thus evolve from online reconstruction kernel validation toward an energy-efficient CPU-FPGA collaborative acceleration system for practical computational lithography flows.

## References

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
17. Kohle R. Fast TCC algorithm for the model building of high NA lithography simulation[C]//Optical Microlithography XVIII. SPIE, 2005, 5754: 918-929.
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
