/**
 * SOCS 2048 Stream Refactored v15 - HLS FFT IP with Higher Precision
 * 
 * PURPOSE: Fix precision issue in v14 (ap_fixed<24,1> insufficient)
 * 
 * CHANGES from v14:
 * - Increase FFT precision: ap_fixed<24,1> → ap_fixed<32,8>
 * - Integer part: 1 bit → 8 bits (dynamic range: -128 to +127)
 * - Fractional part: 23 bits → 24 bits (precision: 2^-24 ≈ 6e-8)
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT)
 */

#include "socs_2048_stream_refactored_v15_hls_fft.h"
#include "hls_fft_config_2048_v15.h"
#include <hls_stream.h>
#include <cmath>

// ============================================================================
// Type Conversion Functions (v15: Higher Precision)
// ============================================================================

/**
 * Convert float complex to ap_fixed complex (v15: 32-bit precision)
 */
void convert_float_to_apfixed_v15(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_fft_in_t output[128][128]
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            float r = input[y][x].real();
            float i = input[y][x].imag();
            
            output[y][x] = cmpx_fft_in_t(data_fft_in_t(r), data_fft_in_t(i));
        }
    }
}

/**
 * Convert ap_fixed complex back to float complex (v15: Higher Precision)
 * 
 * HLS FFT scaled mode (scaling=0x055, nfft=7):
 * - 4 stages of 1/2 shift → 1/16 per 1D FFT
 * - 2D IFFT: total scaling = (1/16)² = 1/256
 * 
 * FFTW BACKWARD (unnormalized):
 * - NO scaling
 * 
 * Compensation: multiply by 256 to match FFTW BACKWARD
 */
void convert_apfixed_to_float_v15(
    cmpx_fft_out_t input[128][128],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    const float SCALE_COMPENSATION = 256.0f;
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            float r = (float)input[y][x].real() * SCALE_COMPENSATION;
            float i = (float)input[y][x].imag() * SCALE_COMPENSATION;
            
            output[y][x] = cmpx_2048_t(r, i);
        }
    }
}

// ============================================================================
// 2D FFT Wrapper (v15: Higher Precision)
// ============================================================================

/**
 * 2D IFFT using HLS FFT IP (v15: Higher Precision)
 * 
 * IMPORTANT: This implementation uses HLS FFT IP for BOTH C Simulation and Co-Simulation
 * - NO conditional compilation (#ifdef __SYNTHESIS__)
 * - Ensures behavioral consistency between C Sim and Co-Sim
 * 
 * Scaling:
 * - HLS FFT scaled mode: 1/16 per 1D FFT → 1/256 for 2D
 * - FFTW BACKWARD (unnormalized): NO scaling
 * - Compensation (×256) applied in convert_apfixed_to_float_v15()
 */
void fft_2d_full_2048_hls_fft_v15(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    cmpx_fft_in_t input_fixed[128][128];
    cmpx_fft_out_t output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // Step 1: Convert float to ap_fixed (v15: 32-bit precision)
    convert_float_to_apfixed_v15(input, input_fixed);
    
    // Step 2: Call HLS FFT IP (128×128, v15: Higher Precision)
    fft_2d_hls_128_v15(input_fixed, output_fixed, is_inverse);
    
    // Step 3: Convert ap_fixed back to float (with compensation)
    convert_apfixed_to_float_v15(output_fixed, output);
}

// ============================================================================
// Main Function: SOCS 2048 Stream Refactored v15 (HLS FFT IP, Higher Precision)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v15(
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
    // AXI-MM interfaces (matching v13 configuration with FIXED depth)
    // CRITICAL: depth must match actual data size for Co-Simulation
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
    
    // AXI-Lite control interface
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS INTERFACE s_axilite port=nk bundle=control
    #pragma HLS INTERFACE s_axilite port=nx_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=ny_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=Lx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ly bundle=control
    
    // Internal buffers
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
        
        // Stage 1: Embed kernel into mask spectrum
        embed_kernel_mask_padded_2048(
            mskf_r, mskf_i,
            krn_r, krn_i,
            fft_input,
            nx_actual, ny_actual,
            k,  // kernel_idx
            Lx, Ly
        );
        
        // Stage 2: 2D IFFT using HLS FFT IP (v15: Higher Precision)
        fft_2d_full_2048_hls_fft_v15(
            fft_input,
            fft_output,
            true  // is_inverse = true for IFFT
        );
        
        // Stage 3: Accumulate intensity
        accumulate_intensity_2048(
            fft_output,
            tmpImg,
            scales[k]
        );
    }
    
    // Stage 4: FFTshift
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
