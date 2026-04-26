/**
 * SOCS 2048 Stream Refactored v13 - Direct DFT Only (NO Conditional Compilation)
 * 
 * PURPOSE: Isolate NaN issue in Co-Simulation
 * 
 * STRATEGY: Reuse v12 functions, only replace FFT implementation
 * - Reuses embed_kernel_mask_padded_2048() from socs_2048.cpp
 * - Reuses accumulate_intensity_2048() from socs_2048.cpp
 * - Reuses fftshift_2d_2048() from socs_2048.cpp
 * - ONLY replaces fft_2d_full_2048() with Direct DFT version
 * 
 * CRITICAL: This version FORCES Direct DFT for both C Sim and Co-Sim
 * - Does NOT use #ifdef __SYNTHESIS__
 * - Directly implements Direct DFT (copied from socs_2048.cpp)
 * - Bypasses fft_2d_full_2048() which has conditional compilation
 * 
 * EXPECTED BEHAVIOR:
 * - C Simulation: Should pass (same as v12)
 * - Co-Simulation: If NaN still occurs, issue is NOT in FFT
 *                  If NaN disappears, issue IS in HLS FFT IP
 */

#include "socs_2048_stream_refactored_v13_direct_dft_only.h"
#include <hls_stream.h>
#include <cmath>

// ============================================================================
// Direct DFT Implementation (copied from socs_2048.cpp, unconditional)
// ============================================================================

/**
 * 1D Direct DFT/IDFT
 * 
 * IMPORTANT: IFFT uses 1/N scaling to match FFTW BACKWARD convention
 *   FFT:  X[k] = sum_n x[n] * exp(-2πi*k*n/N)  (no scaling)
 *   IFFT: x[n] = (1/N) * sum_k X[k] * exp(+2πi*k*n/N)
 * 
 * For 2D IFFT, total scaling = 1/(N*M) = 1/(128*128) = 1/16384
 */
void fft_1d_direct_2048_local(
    cmpx_2048_t in[MAX_FFT_X],
    cmpx_2048_t out[MAX_FFT_X],
    bool is_inverse
) {
    const int N = MAX_FFT_X;
    const float pi = 3.14159265358979323846f;
    const float scale = 1.0f / N;  // IFFT scaling factor
    
    // Direct DFT/IDFT computation
    for (int k = 0; k < N; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        cmpx_2048_t sum(0.0f, 0.0f);
        for (int n = 0; n < N; n++) {
            #pragma HLS LOOP_TRIPCOUNT min=128 max=128
            
            // Twiddle factor: exp(±2πi*k*n/N)
            float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / N;
            cmpx_2048_t twiddle(std::cos(angle), std::sin(angle));
            sum += in[n] * twiddle;
        }
        
        // Apply IFFT scaling: divide by N for inverse transform
        // This matches FFTW BACKWARD convention
        if (is_inverse) {
            out[k] = sum * scale;
        } else {
            out[k] = sum;
        }
    }
}

/**
 * 2D Direct DFT/IDFT (NO conditional compilation)
 * 
 * MANUAL COMPENSATION for Direct DFT IFFT:
 * Direct DFT IFFT has 1/N scaling per dimension = 1/16384 total
 * Multiply by N² = 16384 to match FFTW BACKWARD behavior
 */
void fft_2d_full_2048_direct_dft(
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    cmpx_2048_t temp[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=temp core=RAM_2P_BRAM
    
    // Step 1: FFT on each row (Direct DFT)
    for (int row = 0; row < MAX_FFT_Y; row++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        cmpx_2048_t row_in[MAX_FFT_X];
        cmpx_2048_t row_out[MAX_FFT_X];
        
        // Load row
        for (int col = 0; col < MAX_FFT_X; col++) {
            #pragma HLS PIPELINE II=1
            row_in[col] = fft_input[row][col];
        }
        
        // Direct DFT on row
        fft_1d_direct_2048_local(row_in, row_out, is_inverse);
        
        // Store row result
        for (int col = 0; col < MAX_FFT_X; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_out[col];
        }
    }
    
    // Step 2: FFT on each column (Direct DFT)
    for (int col = 0; col < MAX_FFT_X; col++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        cmpx_2048_t col_in[MAX_FFT_X];
        cmpx_2048_t col_out[MAX_FFT_X];
        
        // Load column
        for (int row = 0; row < MAX_FFT_Y; row++) {
            #pragma HLS PIPELINE II=1
            col_in[row] = temp[row][col];
        }
        
        // Direct DFT on column
        fft_1d_direct_2048_local(col_in, col_out, is_inverse);
        
        // Store column result
        for (int row = 0; row < MAX_FFT_Y; row++) {
            #pragma HLS PIPELINE II=1
            fft_output[row][col] = col_out[row];
        }
    }
    
    // MANUAL COMPENSATION for Direct DFT IFFT
    // Direct DFT IFFT has 1/N scaling per dimension = 1/16384 total
    // Multiply by N² = 16384 to match FFTW BACKWARD behavior
    if (is_inverse) {
        const float N_squared = static_cast<float>(MAX_FFT_Y * MAX_FFT_X);  // 16384.0f
        
        #ifndef __SYNTHESIS__
        printf("[DEBUG DFT 2D] Before compensation (is_inverse=%d):\n", is_inverse);
        printf("  fft_output[101][101] = (%.6f, %.6f)\n", fft_output[101][101].real(), fft_output[101][101].imag());
        printf("  fft_output[102][102] = (%.6f, %.6f)\n", fft_output[102][102].real(), fft_output[102][102].imag());
        printf("  fft_output[103][103] = (%.6f, %.6f)\n", fft_output[103][103].real(), fft_output[103][103].imag());
        #endif
        
        for (int i = 0; i < MAX_FFT_Y; i++) {
            for (int j = 0; j < MAX_FFT_X; j++) {
                #pragma HLS PIPELINE II=1
                fft_output[i][j] = fft_output[i][j] * N_squared;
            }
        }
        
        #ifndef __SYNTHESIS__
        printf("[DEBUG DFT 2D] After compensation:\n");
        printf("  fft_output[101][101] = (%.6f, %.6f)\n", fft_output[101][101].real(), fft_output[101][101].imag());
        printf("  fft_output[102][102] = (%.6f, %.6f)\n", fft_output[102][102].real(), fft_output[102][102].imag());
        printf("  fft_output[103][103] = (%.6f, %.6f)\n", fft_output[103][103].real(), fft_output[103][103].imag());
        #endif
    }
}

// ============================================================================
// Main Function: SOCS 2048 Stream Refactored v13 (Direct DFT Only)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v13(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
) {
    // AXI-MM interfaces (matching v12 configuration)
    // CRITICAL: depth must match actual data size for Co-Simulation
    // mskf_r/i: 1024×1024 = 1,048,576 (golden_1024 config)
    // scales: nk=10
    // krn_r/i: 10 kernels × 17×17 = 2,890
    // tmpImg_ddr/output: 128×128 = 16,384
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 \
        depth=32 latency=16 num_read_outstanding=2 max_read_burst_length=4
    
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 \
        depth=76832 latency=16 num_read_outstanding=4 max_read_burst_length=32
    
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 \
        depth=76832 latency=16 num_read_outstanding=4 max_read_burst_length=32
    
    #pragma HLS INTERFACE m_axi port=tmpImg_ddr offset=slave bundle=gmem5 \
        depth=16384 latency=8 num_write_outstanding=4 max_write_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem6 \
        depth=16384 latency=8 num_write_outstanding=4 max_write_burst_length=64
    #pragma HLS INTERFACE s_axilite port=nk bundle=control
    #pragma HLS INTERFACE s_axilite port=nx_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=ny_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=Lx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ly bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // Local arrays for computation
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X];
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    
    // Initialize tmpImg to 0
    init_tmpimg:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // Process each kernel
    kernel_loop:
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=10 max=10
        
        // Stage 1: Embed kernel into mask spectrum (REUSE v12 function)
        embed_kernel_mask_padded_2048(
            mskf_r, mskf_i,
            krn_r, krn_i,
            fft_input,
            nx_actual, ny_actual,
            k,  // kernel_idx
            Lx, Ly
        );
        
        // Stage 2: 2D IFFT (Direct DFT - NO conditional compilation)
        fft_2d_full_2048_direct_dft(
            fft_input,
            fft_output,
            true  // is_inverse = true for IFFT
        );
        
        // Stage 3: Accumulate intensity (REUSE v12 function)
        accumulate_intensity_2048(
            fft_output,
            tmpImg,
            scales[k]
        );
    }
    
    // Stage 4: FFTshift (REUSE v12 function)
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
    fftshift_2d_2048(tmpImg, tmpImgp);
    
    // Stage 5: Copy to DDR output
    copy_to_ddr:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_ddr[y * MAX_FFT_X + x] = tmpImgp[y][x];
            output[y * MAX_FFT_X + x] = tmpImgp[y][x];
        }
    }
}
