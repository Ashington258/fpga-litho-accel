/**
 * SOCS 2048 Stream Refactored v16 - HLS FFT IP with Block Floating Point
 * 
 * PURPOSE: Fix precision issue in v14 (ap_fixed<24,1> insufficient)
 * 
 * CHANGES from v14:
 * - Use block floating point mode (tracks scaling via blk_exp)
 * - Increase precision: ap_fixed<24,1> → ap_fixed<32,1>
 * - 31-bit fractional precision (vs 23-bit in v14)
 * 
 * CRITICAL CONSTRAINT:
 * - HLS FFT IP REQUIRES 1-bit integer part for ALL scaling modes
 * - Block floating point mode still uses ap_fixed<N, 1>
 * - Reason: HLS FFT uses normalized representation [-1, 1)
 * 
 * EXPECTED RESULT: RMSE < 1e-5 (matching v13 Direct DFT)
 */

#include "socs_2048_stream_refactored_v16_hls_fft.h"
#include "hls_fft_config_2048_v16.h"
#include <hls_stream.h>
#include <cmath>

// ============================================================================
// Type Conversion Functions (v16: Block Floating Point)
// ============================================================================

/**
 * Convert float complex to ap_fixed complex (v16: 32-bit precision)
 */
void convert_float_to_apfixed_v16(
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
 * Convert ap_fixed complex back to float complex (v16b: Block Floating Point Mode)
 * 
 * Block floating point mode:
 * - blk_exp tracks total scaling applied by FFT
 * - Compensation: output * 2^(blk_exp)
 * 
 * For IFFT:
 * - HLS FFT performs unnormalized IDFT
 * - blk_exp indicates bits shifted right to prevent overflow
 * - Compensation: output * 2^(blk_exp) to restore correct magnitude
 */
void convert_apfixed_to_float_v16(
    cmpx_fft_out_t input[128][128],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse,
    unsigned blk_exp
) {
    #pragma HLS INLINE off
    
    // Block floating point compensation
    // - blk_exp = total bits shifted right across all FFT stages
    // - Compensation factor = 2^(blk_exp)
    // - For IFFT: this restores the correct magnitude
    float compensation = powf(2.0f, (float)blk_exp);
    
    #ifndef __SYNTHESIS__
    printf("[DEBUG COMPENSATION] is_inverse=%d, blk_exp=%u, compensation=%.1f\n", 
           is_inverse, blk_exp, compensation);
    #endif
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            float r = (float)input[y][x].real() * compensation;
            float i = (float)input[y][x].imag() * compensation;
            
            output[y][x] = cmpx_2048_t(r, i);
        }
    }
}

// ============================================================================
// 2D FFT Wrapper (v16: Block Floating Point)
// ============================================================================

/**
 * 2D IFFT using HLS FFT IP (v16b: Block Floating Point Mode)
 * 
 * IMPORTANT: This implementation uses HLS FFT IP for BOTH C Simulation and Co-Simulation
 * - NO conditional compilation (#ifdef __SYNTHESIS__)
 * - Ensures behavioral consistency between C Sim and Co-Sim
 * 
 * Block floating point mode:
 * - blk_exp tracks total scaling applied
 * - Compensation: output * 2^(blk_exp)
 */
void fft_2d_full_2048_hls_fft_v16(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    cmpx_fft_in_t input_fixed[128][128];
    cmpx_fft_out_t output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // DEBUG: Print input values
    #ifndef __SYNTHESIS__
    printf("[DEBUG FFT 2D] is_inverse=%d\n", is_inverse);
    printf("[DEBUG FFT 2D] Input sample (bottom-right region):\n");
    printf("  input[101][101] = (%.6f, %.6f)\n", input[101][101].real(), input[101][101].imag());
    printf("  input[102][102] = (%.6f, %.6f)\n", input[102][102].real(), input[102][102].imag());
    printf("  input[103][103] = (%.6f, %.6f)\n", input[103][103].real(), input[103][103].imag());
    #endif
    
    // Step 1: Convert float to ap_fixed (v16: 32-bit precision)
    convert_float_to_apfixed_v16(input, input_fixed);
    
    // DEBUG: Print converted values
    #ifndef __SYNTHESIS__
    printf("[DEBUG FFT 2D] After conversion to ap_fixed:\n");
    printf("  input_fixed[101][101] = (%.8f, %.8f)\n", 
           (float)input_fixed[101][101].real(), (float)input_fixed[101][101].imag());
    printf("  input_fixed[102][102] = (%.8f, %.8f)\n", 
           (float)input_fixed[102][102].real(), (float)input_fixed[102][102].imag());
    printf("  input_fixed[103][103] = (%.8f, %.8f)\n", 
           (float)input_fixed[103][103].real(), (float)input_fixed[103][103].imag());
    printf("  input_fixed[64][64] = (%.8f, %.8f)\n", 
           (float)input_fixed[64][64].real(), (float)input_fixed[64][64].imag());
    #endif
    
    // Step 2: Call HLS FFT IP (128×128, v16b: Block Floating Point Mode)
    unsigned blk_exp;
    fft_2d_hls_128_v16(input_fixed, output_fixed, is_inverse, &blk_exp);
    
    // DEBUG: Print FFT output
    #ifndef __SYNTHESIS__
    printf("[DEBUG FFT 2D] After FFT (ap_fixed, blk_exp=%u):\n", blk_exp);
    printf("  output_fixed[101][101] = (%.8f, %.8f)\n", 
           (float)output_fixed[101][101].real(), (float)output_fixed[101][101].imag());
    printf("  output_fixed[102][102] = (%.8f, %.8f)\n", 
           (float)output_fixed[102][102].real(), (float)output_fixed[102][102].imag());
    printf("  output_fixed[103][103] = (%.8f, %.8f)\n", 
           (float)output_fixed[103][103].real(), (float)output_fixed[103][103].imag());
    printf("  output_fixed[64][64] = (%.8f, %.8f)\n", 
           (float)output_fixed[64][64].real(), (float)output_fixed[64][64].imag());
    #endif
    
    // Step 3: Convert ap_fixed back to float (with block floating point compensation)
    convert_apfixed_to_float_v16(output_fixed, output, is_inverse, blk_exp);
    
    // DEBUG: Print final output
    #ifndef __SYNTHESIS__
    printf("[DEBUG FFT 2D] After conversion to float:\n");
    printf("  output[101][101] = (%.6f, %.6f)\n", output[101][101].real(), output[101][101].imag());
    printf("  output[102][102] = (%.6f, %.6f)\n", output[102][102].real(), output[102][102].imag());
    printf("  output[103][103] = (%.6f, %.6f)\n", output[103][103].real(), output[103][103].imag());
    #endif
}

// ============================================================================
// Main Function: SOCS 2048 Stream Refactored v16 (HLS FFT IP, Scaled Mode)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v16(
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
        
        // Stage 2: 2D IFFT using HLS FFT IP (v16: Block Floating Point)
        fft_2d_full_2048_hls_fft_v16(
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
