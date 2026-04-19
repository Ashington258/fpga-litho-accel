/**
 * SOCS HLS Full Implementation with Real FFT Integration
 * FPGA-Litho Project
 * 
 * This replaces the placeholder implementation with actual calcSOCS algorithm
 * Configuration: Nx=16, IFFT 128×128, Output 65×65 (MIGRATED from Nx=4)
 */

#include "socs_fft.h"

/**
 * SOCS Core Calculation with Real FFT
 * 
 * Algorithm:
 * 1. For each kernel k:
 *    a. Extract 33×33 region from mask spectrum (centered) - MIGRATED from 9×9
 *    b. Multiply with SOCS kernel
 *    c. Embed product into 128×128 padded array (centered padding) - MIGRATED from 32×32
 *    d. Perform 128×128 2D IFFT - MIGRATED from 32×32
 *    e. Extract valid 65×65 region - MIGRATED from 17×17
 *    f. Accumulate intensity: scale_k × (re² + im²)
 * 2. Apply fftshift to accumulated result
 * 3. Output tmpImgp[65×65] - MIGRATED from 17×17
 * 
 * @param mskf_r: Mask spectrum real part (512×512)
 * @param mskf_i: Mask spectrum imaginary part (512×512)
 * @param scales: Eigenvalue weights (nk=10)
 * @param krn_r: SOCS kernels real part (nk × 9×9)
 * @param krn_i: SOCS kernels imaginary part (nk × 9×9)
 * @param output: Output tmpImgp (17×17)
 */
void calc_socs_hls(
    float *mskf_r,      // AXI-MM: Mask spectrum real (512×512)
    float *mskf_i,      // AXI-MM: Mask spectrum imaginary (512×512)
    float *scales,      // AXI-MM: Eigenvalues (10)
    float *krn_r,       // AXI-MM: Kernels real (10×33×33) - MIGRATED from 10×9×9
    float *krn_i,       // AXI-MM: Kernels imaginary (10×33×33) - MIGRATED from 10×9×9
    float *output       // AXI-MM: Output (65×65) - MIGRATED from 17×17
) {
    // ==================== m_axi 接口配置（推荐方案 B） ====================
    // 每个接口独立 bundle（保证物理上多个 AXI Master 端口）
    // offset=slave：生成统一的 s_axi_control 接口，地址寄存器全部放在里面
    // depth 使用具体数值（避免宏展开问题）- 注意：depth 是元素数量，不是字节数
    // latency / outstanding / burst 参数显著提升 AXI 吞吐量
    
    // mskf_r: 512×512 = 262144 elements, 1 MB
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    // mskf_i: 512×512 = 262144 elements, 1 MB
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    // scales: nk=10 elements, 40 bytes
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 \
        depth=10 latency=16 num_read_outstanding=2 max_read_burst_length=4
    
    // krn_r: nk×kerX×kerY = 10×33×33 = 10890 elements, 43.56 KB - MIGRATED
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 \
        depth=10890 latency=16 num_read_outstanding=4 max_read_burst_length=64
    
    // krn_i: nk×kerX×kerY = 10×33×33 = 10890 elements, 43.56 KB - MIGRATED
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 \
        depth=10890 latency=16 num_read_outstanding=4 max_read_burst_length=64
    
    // output: convX×convY = 65×65 = 4225 elements, 16.9 KB - MIGRATED
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 \
        depth=4225 latency=8 num_write_outstanding=2 max_write_burst_length=32
    
    // ==================== 控制接口（必须保留） ====================
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // Local buffers for kernels (can be cached)
    float local_krn_r[nk * kerX * kerY];
    float local_krn_i[nk * kerX * kerY];
    float local_scales[nk];
    
    #pragma HLS ARRAY_PARTITION variable=local_krn_r cyclic factor=4
    #pragma HLS ARRAY_PARTITION variable=local_krn_i cyclic factor=4
    #pragma HLS ARRAY_PARTITION variable=local_scales cyclic factor=2
    
    // Load kernels and scales from DDR (optimization: could be done once per IP call)
    // For nk=10 kernels of 9×9 = 81 complex = 162 floats
    for (int i = 0; i < nk * kerX * kerY; i++) {
        #pragma HLS PIPELINE
        local_krn_r[i] = krn_r[i];
        local_krn_i[i] = krn_i[i];
    }
    
    for (int i = 0; i < nk; i++) {
        #pragma HLS PIPELINE
        local_scales[i] = scales[i];
    }
    
    // Mask spectrum center (512×512 -> center at 256, 256)
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    // Accumulated intensity buffer on FULL 128×128 array (matching litho.cpp)
    // litho.cpp: vector<double> tmpImg(fftConvX * fftConvY, 0.0);
    float tmpImg_128[fftConvY][fftConvX] = {0.0f};
    float tmpImgp_128[fftConvY][fftConvX];
    
    #pragma HLS ARRAY_PARTITION variable=tmpImg_128 cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImgp_128 cyclic factor=4 dim=2
    
    // Process each kernel
    for (int k = 0; k < nk; k++) {
        #pragma HLS UNROLL factor=2
        
        // Buffers for this kernel's computation
        cmpxData_t padded[fftConvY][fftConvX];
        cmpxData_t ifft_out[fftConvY][fftConvX];
        
        // REMOVED: ARRAY_PARTITION causes FFT port mismatch with HLS FFT IP
        // FFT IP core provides single unified port, but partition generates _0_, _1_, _2_, _3_ suffix ports
        // Original pragmas:
        // #pragma HLS ARRAY_PARTITION variable=padded cyclic factor=4 dim=2
        // #pragma HLS ARRAY_PARTITION variable=ifft_out cyclic factor=4 dim=2
        
        // Step 1: Build padded IFFT input (kernel × mask product) - MIGRATED
        build_padded_ifft_input_128(
            mskf_r, mskf_i,
            &local_krn_r[k * kerX * kerY],
            &local_krn_i[k * kerX * kerY],
            padded, Lxh, Lyh
        );
        
        // Step 2: 128×128 2D IFFT - MIGRATED
        ifft_2d_128x128(padded, ifft_out);
        
        // Step 3: Accumulate intensity on FULL 128×128 array (matching litho.cpp) - MIGRATED
        // litho.cpp: tmpImg[i] += scales[k] * (re*re + im*im)
        accumulate_intensity_128x128(ifft_out, tmpImg_128, local_scales[k]);
    }
    
    // Step 4: Apply fftshift on 128×128 (matching litho.cpp) - MIGRATED
    // litho.cpp: myShift(tmpImg, tmpImgp, fftConvX, fftConvY, true, true)
    fftshift_128x128(tmpImg_128, tmpImgp_128);
    
    // Step 5: Extract center 65×65 from shifted 128×128 (matching litho.cpp golden output) - MIGRATED
    float tmpImgp_65[convY][convX];
    
    // REMOVED: ARRAY_PARTITION for consistency (not used by FFT)
    
    extract_center_65x65_from_128(tmpImgp_128, tmpImgp_65);
    
    // Write output to DDR
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE
            int idx = y * convX + x;
            output[idx] = tmpImgp_65[y][x];
        }
    }
}