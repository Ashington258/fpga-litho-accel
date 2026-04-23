/**
 * SOCS HLS Optimized Implementation with Vitis FFT IP Integration
 * FPGA-Litho Project
 * 
 * OPTIMIZATION: Replace direct DFT with Vitis HLS FFT IP
 *   - Direct DFT: O(N²) complexity → High latency (43M cycles)
 *   - HLS FFT IP: O(N log N) → Target latency <1M cycles
 *   - Expected acceleration: 50× compared to current implementation
 * 
 * Key Changes:
 *   1. Stream-based FFT API (hls::fft<config>)
 *   2. Array-to-Stream adapter for 2D FFT
 *   3. Full 32×32 tmpImgp output for Fourier Interpolation
 *   4. LUT-based FFT (DSP-free, precision RMSE < 1e-5)
 * 
 * Target Device: xcku3p-ffvb676-2-e
 * Clock: 200MHz (5ns period)
 */

#include "socs_config_optimized.h"
#include <hls_stream.h>
#include <cmath>
#include <cstdio>

// ============================================================================
// Stream-based FFT Helper Functions
// ============================================================================

/**
 * Convert 2D array row to stream for FFT IP
 */
void array_row_to_stream(
    cmpx_fft_t input[fftConvX],
    hls::stream<cmpx_fft_t> &output_stream
) {
    for (int i = 0; i < fftConvX; i++) {
        #pragma HLS PIPELINE II=1
        output_stream.write(input[i]);
    }
}

/**
 * Convert FFT stream output back to 2D array row
 */
void stream_to_array_row(
    hls::stream<cmpx_fft_t> &input_stream,
    cmpx_fft_t output[fftConvX]
) {
    for (int i = 0; i < fftConvX; i++) {
        #pragma HLS PIPELINE II=1
        output[i] = input_stream.read();
    }
}

/**
 * Single 1D FFT using Vitis HLS FFT IP (Array API for unscaled config)
 * 
 * This is the optimized version replacing direct DFT.
 * Latency: ~N log N cycles vs N² for direct DFT
 * 
 * API Reference (from tb_fft_minimal.cpp working example):
 *   hls::ip_fft::config_t<config> fft_config;
 *   hls::ip_fft::status_t<config> fft_status;
 *   fft_config.setDir(dir);  // 0=forward, 1=inverse
 *   hls::fft<config>(input, output, &fft_status, &fft_config);
 * 
 * NOTE: Using std::complex<float> to avoid fixed-point width constraints.
 *       Float FFT has no output_width alignment issue.
 */
void fft_1d_array(
    cmpx_fft_t input_array[FFT_LENGTH],
    cmpx_fft_t output_array[FFT_LENGTH],  // FLOAT: input/output both std::complex<float>
    ap_uint<1> dir           // 0=forward, 1=inverse
) {
    #pragma HLS INLINE off
    
    // Config and status objects for array-based API
    hls::ip_fft::config_t<fft_config_socs> fft_config;
    hls::ip_fft::status_t<fft_config_socs> fft_status;
    
    // Set direction: 0=forward (FFT), 1=inverse (IFFT)
    fft_config.setDir(dir);
    
    // Invoke Vitis HLS FFT IP (array-based, unscaled, FLOAT)
    hls::fft<fft_config_socs>(input_array, output_array, &fft_status, &fft_config);
}

/**
 * Optimized 2D FFT using row-column decomposition with HLS FFT IP
 * 
 * Algorithm:
 *   1. FFT on each row (stream-based)
 *   2. Transpose result
 *   3. FFT on each "new row" (originally columns)
 *   4. Transpose back
 * 
 * Latency estimate: 
 *   - Row FFTs: 32 rows × ~200 cycles = ~6,400 cycles
 *   - Transpose: ~1,000 cycles
 *   - Col FFTs: 32 cols × ~200 cycles = ~6,400 cycles
 *   - Total: ~14,000 cycles (vs 2,178,400 with direct DFT)
 */
void fft_2d_optimized(
    cmpx_fft_t input[fftConvY][fftConvX],
    cmpx_fft_t output[fftConvY][fftConvX],  // FLOAT: std::complex<float>
    bool is_inverse
) {
    #pragma HLS INLINE off
    // Removed DATAFLOW pragma for C-sim correctness (sequential execution)
    
    // Intermediate buffers - INITIALIZED TO ZERO (FLOAT FFT output)
    cmpx_fft_t temp1[fftConvY][fftConvX];
    cmpx_fft_t temp2[fftConvY][fftConvX];
    cmpx_fft_t temp3[fftConvY][fftConvX];
    
    #pragma HLS ARRAY_PARTITION variable=temp1 cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=temp3 cyclic factor=2 dim=2
    #pragma HLS RESOURCE variable=temp1 core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=temp2 core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=temp3 core=RAM_2P_BRAM
    
    // 1D array buffers for FFT IP (array-based API) - INITIALIZED TO ZERO
    cmpx_fft_t row_in[FFT_LENGTH];      // Input: std::complex<float>
    cmpx_fft_t row_out[FFT_LENGTH];     // Output: std::complex<float> (FLOAT FFT)
    
    #pragma HLS ARRAY_PARTITION variable=row_in cyclic factor=2
    #pragma HLS ARRAY_PARTITION variable=row_out cyclic factor=2
    
    // CRITICAL: Initialize all intermediate arrays to avoid NaN/inf propagation
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            temp1[y][x] = cmpx_fft_t(0.0f, 0.0f);
            temp2[y][x] = cmpx_fft_t(0.0f, 0.0f);
            temp3[y][x] = cmpx_fft_t(0.0f, 0.0f);
        }
    }
    for (int i = 0; i < FFT_LENGTH; i++) {
        #pragma HLS PIPELINE II=1
        row_in[i] = cmpx_fft_t(0.0f, 0.0f);
        row_out[i] = cmpx_fft_t(0.0f, 0.0f);
    }
    
    // Step 1: Row-wise FFT using HLS IP (array-based API)
    for (int row = 0; row < fftConvY; row++) {
        #pragma HLS LOOP_FLATTEN
        
        // Copy row to 1D array
        for (int col = 0; col < fftConvX; col++) {
            #pragma HLS PIPELINE II=1
            row_in[col] = input[row][col];
        }
        
        // Execute 1D FFT via HLS IP (array-based)
        fft_1d_array(row_in, row_out, is_inverse);
        
        // Copy result back to temp1
        for (int col = 0; col < fftConvX; col++) {
            #pragma HLS PIPELINE II=1
            temp1[row][col] = row_out[col];
        }
    }
    
    // Step 2: Transpose (rows → columns)
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            temp2[x][y] = temp1[y][x];  // Transpose
        }
    }
    
    // Step 3: Column-wise FFT (now treating columns as "rows")
    for (int col = 0; col < fftConvX; col++) {
        #pragma HLS LOOP_FLATTEN
        
        // Copy column (now as row in transposed array) to 1D array
        for (int row = 0; row < fftConvY; row++) {
            #pragma HLS PIPELINE II=1
            row_in[row] = temp2[col][row];
        }
        
        // Execute 1D FFT via HLS IP (array-based)
        fft_1d_array(row_in, row_out, is_inverse);
        
        // Copy result back
        for (int row = 0; row < fftConvY; row++) {
            #pragma HLS PIPELINE II=1
            temp3[col][row] = row_out[row];
        }
    }
    
    // Step 4: Transpose back to original orientation
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            output[y][x] = temp3[x][y];  // Transpose back
        }
    }
}

// ============================================================================
// SOCS Kernel Functions (unchanged from original)
// ============================================================================

// Data types for external interfaces (float for AXI-MM compatibility)
typedef float data_t;
typedef std::complex<float> cmpx_t;

/**
 * Embed kernel * mask product into padded FFT array
 */
void embed_kernel_mask_product(
    float* mskf_r,
    float* mskf_i,
    float* krn_r,
    float* krn_i,
    cmpx_fft_t fft_input[fftConvY][fftConvX],
    int kernel_idx,
    int Lxh,
    int Lyh
) {
    #pragma HLS INLINE off
    
    int difX = fftConvX - kerX;  // = 23
    int difY = fftConvY - kerY;  // = 23
    
    // Clear FFT input array (zero-padding)
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = cmpx_fft_t(0.0f, 0.0f);
        }
    }
    
    int kernel_offset = kernel_idx * kerX * kerY;
    
    for (int ky = 0; ky < kerY; ky++) {
        for (int kx = 0; kx < kerX; kx++) {
            #pragma HLS PIPELINE II=1
            
            int y_offset = ky - Ny;
            int x_offset = kx - Nx;
            
            int fft_y = difY + ky;
            int fft_x = difX + kx;
            
            int mask_y = Lyh + y_offset;
            int mask_x = Lxh + x_offset;
            
            if (fft_y >= 0 && fft_y < fftConvY &&
                fft_x >= 0 && fft_x < fftConvX &&
                mask_y >= 0 && mask_y < Ly &&
                mask_x >= 0 && mask_x < Lx) {
                
                float kr_r = krn_r[kernel_offset + ky * kerX + kx];
                float kr_i = krn_i[kernel_offset + ky * kerX + kx];
                
                float ms_r = mskf_r[mask_y * Lx + mask_x];
                float ms_i = mskf_i[mask_y * Lx + mask_x];
                
                // Complex multiplication: K * M
                float prod_r = kr_r * ms_r - kr_i * ms_i;
                float prod_i = kr_r * ms_i + kr_i * ms_r;
                
                fft_input[fft_y][fft_x] = cmpx_fft_t(prod_r, prod_i);
            }
        }
    }
}

/**
 * Accumulate intensity from IFFT output (unscaled mode - direct match with FFTW)
 * 
 * FFT mode: UNSCALED (matches FFTW BACKWARD behavior)
 *   - No scaling compensation needed
 *   - intensity = re^2 + im^2 (direct computation)
 */
void accumulate_intensity(
    cmpx_fft_t ifft_output[fftConvY][fftConvX],  // FLOAT: std::complex<float>
    float tmpImg[fftConvY][fftConvX],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            
            // Compute intensity (unscaled FFT, no compensation)
            float re = ifft_output[y][x].real();
            float im = ifft_output[y][x].imag();
            
            float intensity = re * re + im * im;
            
            tmpImg[y][x] += scale * intensity;
        }
    }
}

/**
 * FFT shift: move zero-frequency component to center
 */
void fftshift_2d(
    float input[fftConvY][fftConvX],
    float output[fftConvY][fftConvX]
) {
    #pragma HLS INLINE off
    
    int halfX = fftConvX / 2;
    int halfY = fftConvY / 2;
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            
            int dst_y = (y + halfY) % fftConvY;
            int dst_x = (x + halfX) % fftConvX;
            
            output[dst_y][dst_x] = input[y][x];
        }
    }
}

/**
 * Extract center region from shifted intensity map
 */
void extract_center_region(
    float tmpImgp[fftConvY][fftConvX],
    float output[convY][convX]
) {
    #pragma HLS INLINE off
    
    int offsetX = (fftConvX - convX) / 2;
    int offsetY = (fftConvY - convY) / 2;
    
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE II=1
            output[y][x] = tmpImgp[offsetY + y][offsetX + x];
        }
    }
}

// ============================================================================
// Main SOCS Function - Optimized Version
// ============================================================================

// Number of kernels
#define nk 4

/**
 * SOCS Aerial Image Computation - Optimized with HLS FFT IP
 * 
 * Expected Performance:
 *   - Latency: ~140,000 cycles (vs 43M with direct DFT)
 *   - Execution time: ~0.7ms @ 200MHz
 *   - Acceleration ratio: ~70× (vs CPU 50ms)
 */
void calc_socs_optimized_hls(
    float *mskf_r,      // AXI-MM: Mask spectrum real (512×512)
    float *mskf_i,      // AXI-MM: Mask spectrum imaginary
    float *scales,      // AXI-MM: Eigenvalues (4)
    float *krn_r,       // AXI-MM: Kernels real (4×9×9)
    float *krn_i,       // AXI-MM: Kernels imaginary
    float *output_full, // AXI-MM: Output tmpImgp FULL (32×32)
    float *output_center // AXI-MM: Output tmpImgp center (17×17)
) {
    // ==================== AXI-MM Interface Configuration ====================
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 \
        depth=4 latency=16 num_read_outstanding=2 max_read_burst_length=4
    
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 \
        depth=324 latency=16 num_read_outstanding=4 max_read_burst_length=16
    
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 \
        depth=324 latency=16 num_read_outstanding=4 max_read_burst_length=16
    
    #pragma HLS INTERFACE m_axi port=output_full offset=slave bundle=gmem5 \
        depth=1024 latency=8 num_write_outstanding=2 max_write_burst_length=16
    
    #pragma HLS INTERFACE m_axi port=output_center offset=slave bundle=gmem6 \
        depth=289 latency=8 num_write_outstanding=2 max_write_burst_length=8
    
    #pragma HLS INTERFACE s_axilite port=return
    
    // ==================== Local Arrays ====================
    cmpx_fft_t fft_input[fftConvY][fftConvX];      // Input: std::complex<float>
    cmpx_fft_t fft_output[fftConvY][fftConvX];     // Output: std::complex<float> (FLOAT FFT)
    
    float tmpImg[fftConvY][fftConvX];
    float tmpImgp[fftConvY][fftConvX];
    float output_local[convY][convX];
    
    #pragma HLS ARRAY_PARTITION variable=fft_input cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=fft_output cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImg cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImgp cyclic factor=2 dim=2
    
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
    // ==================== CRITICAL: Initialize All Arrays ====================
    // Must initialize BEFORE DATAFLOW region to avoid NaN/inf propagation
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = cmpx_fft_t(0.0f, 0.0f);
            fft_output[y][x] = cmpx_fft_t(0.0f, 0.0f);
            tmpImg[y][x] = 0.0f;
            tmpImgp[y][x] = 0.0f;
        }
    }
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE II=1
            output_local[y][x] = 0.0f;
        }
    }
    
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    // ==================== Process Each Kernel ====================
    // Sequential execution (removed DATAFLOW for C-sim correctness)
    
    for (int k = 0; k < nk; k++) {
        // Step 1: Embed kernel * mask product
        embed_kernel_mask_product(
            mskf_r, mskf_i, krn_r, krn_i,
            fft_input, k, Lxh, Lyh
        );
        
        // Step 2: 2D IFFT (OPTIMIZED - using HLS FFT IP)
        fft_2d_optimized(fft_input, fft_output, true);
        
        // Step 3: Accumulate intensity
        accumulate_intensity(fft_output, tmpImg, scales[k]);
    }
    
    // ==================== Post-processing ====================
    // Step 4: FFT shift
    fftshift_2d(tmpImg, tmpImgp);
    
    // Step 5: Write FULL 32×32 output
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            output_full[y * fftConvX + x] = tmpImgp[y][x];
        }
    }
    
    // Step 6: Extract and write center 17×17 region
    extract_center_region(tmpImgp, output_local);
    
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE II=1
            output_center[y * convX + x] = output_local[y][x];
        }
    }
}