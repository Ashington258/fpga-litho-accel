/**
 * SOCS HLS Full Implementation with Real FFT Integration
 * FPGA-Litho Project
 * 
 * This replaces the placeholder implementation with actual calcSOCS algorithm
 * Configuration: Nx=4, IFFT 32×32, Output 17×17
 */

#include "socs_fft.h"

/**
 * SOCS Core Calculation with Real FFT
 * 
 * Algorithm:
 * 1. For each kernel k:
 *    a. Extract 9×9 region from mask spectrum (centered)
 *    b. Multiply with SOCS kernel
 *    c. Embed product into 32×32 padded array (centered padding)
 *    d. Perform 32×32 2D IFFT
 *    e. Extract valid 17×17 region
 *    f. Accumulate intensity: scale_k × (re² + im²)
 * 2. Apply fftshift to accumulated result
 * 3. Output tmpImgp[17×17]
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
    float *krn_r,       // AXI-MM: Kernels real (10×9×9)
    float *krn_i,       // AXI-MM: Kernels imaginary (10×9×9)
    float *output       // AXI-MM: Output (17×17)
) {
    // HLS pragmas for AXI-MM interfaces
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 depth=Lx*Ly
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 depth=Lx*Ly
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 depth=nk
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 depth=nk*kerX*kerY
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 depth=nk*kerX*kerY
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 depth=convX*convY
    
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
    
    // Accumulated intensity buffer on FULL 32×32 array (matching litho.cpp)
    // litho.cpp: vector<double> tmpImg(fftConvX * fftConvY, 0.0);
    float tmpImg_32[fftConvY][fftConvX] = {0.0f};
    float tmpImgp_32[fftConvY][fftConvX];
    
    #pragma HLS ARRAY_PARTITION variable=tmpImg_32 cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImgp_32 cyclic factor=4 dim=2
    
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
        
        // Step 1: Build padded IFFT input (kernel × mask product)
        build_padded_ifft_input_32(
            mskf_r, mskf_i,
            &local_krn_r[k * kerX * kerY],
            &local_krn_i[k * kerX * kerY],
            padded, Lxh, Lyh
        );
        
        // Step 2: 32×32 2D IFFT
        ifft_2d_32x32(padded, ifft_out);
        
        // Step 3: Accumulate intensity on FULL 32×32 array (matching litho.cpp)
        // litho.cpp: tmpImg[i] += scales[k] * (re*re + im*im)
        accumulate_intensity_32x32(ifft_out, tmpImg_32, local_scales[k]);
    }
    
    // Step 4: Apply fftshift on 32×32 (matching litho.cpp)
    // litho.cpp: myShift(tmpImg, tmpImgp, fftConvX, fftConvY, true, true)
    fftshift_32x32(tmpImg_32, tmpImgp_32);
    
    // Step 5: Extract center 17×17 from shifted 32×32 (matching litho.cpp golden output)
    float tmpImgp_17[convY][convX];
    
    // REMOVED: ARRAY_PARTITION for consistency (not used by FFT)
    
    extract_center_17x17_from_32(tmpImgp_32, tmpImgp_17);
    
    // Write output to DDR
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE
            int idx = y * convX + x;
            output[idx] = tmpImgp_17[y][x];
        }
    }
}