/**
 * SOCS 2048 Stream Refactored v14 - HLS FFT IP Integration
 * 
 * PURPOSE: Replace Direct DFT with HLS FFT IP to resolve RTL simulation timeout
 * 
 * KEY CHANGES from v13:
 * - Uses HLS FFT IP (hls::fft) for BOTH C Simulation and Co-Simulation
 * - No conditional compilation (#ifdef __SYNTHESIS__) - single code path
 * - O(N log N) complexity instead of O(N²) → ~18× faster
 * - Expected RTL simulation time: ~70ms per kernel (vs 1.262s with Direct DFT)
 * 
 * SCALING BEHAVIOR:
 * - HLS FFT scaled mode (scaling=0x155): divides by 16 per dimension (4 stages × 1/2)
 * - 2D IFFT total scaling: 1/(16×16) = 1/256
 * - FFTW BACKWARD (unnormalized): NO scaling → need ×256 compensation
 * - Compensation applied in convert_apfixed_to_float_v14()
 * 
 * REUSED FUNCTIONS (from socs_2048.cpp):
 * - embed_kernel_mask_padded_2048()
 * - accumulate_intensity_2048()
 * - fftshift_2d_2048()
 */

#include "socs_2048_stream_refactored_v14_hls_fft.h"
#include "hls_fft_config_2048_corrected.h"
#include <hls_stream.h>
#include <cmath>

// ============================================================================
// Float ↔ ap_fixed Type Conversion (from socs_2048.cpp SYNTHESIS branch)
// ============================================================================

/**
 * Convert float complex to ap_fixed complex for HLS FFT IP
 */
void convert_float_to_apfixed_v14(
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
 * Convert ap_fixed complex back to float complex
 * 
 * NOTE: HLS FFT scaled mode (0x155, nfft=7) scales by 1/2 at 4 stages
 * → 1D scaling = 1/16, 2D scaling = 1/256
 * The FFTW BACKWARD (unnormalized) IFFT has NO scaling.
 * To match: need to multiply by 256 (= 16 × 16) for 2D IFFT.
 */
void convert_apfixed_to_float_v14(
    cmpx_fft_out_t input[128][128],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    // HLS FFT scaled mode compensation:
    // scaling=0x155 for nfft=7 → 4 stages of 1/2 shift → 1/16 per 1D
    // For 2D IFFT: total scaling = (1/16)² = 1/256
    // To match FFTW BACKWARD (unnormalized): multiply by 256
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
// 2D FFT Implementation using HLS FFT IP (NO conditional compilation)
// ============================================================================

/**
 * 2D FFT/IFFT using HLS FFT IP (128×128)
 * 
 * Workflow:
 *   1. Convert float to ap_fixed<24,1>
 *   2. Call HLS FFT IP via fft_2d_hls_128()
 *   3. Convert ap_fixed back to float (with scaling compensation)
 * 
 * SCALING: HLS FFT scaled mode (scaling=0x155, nfft=7)
 *   - 1D IFFT: divides by 2^4=16 (4 stages × 1/2 shift)
 *   - 2D IFFT: total scaling = 1/(16×16) = 1/256
 *   - FFTW BACKWARD (unnormalized): NO scaling → need ×256 compensation
 *   - Compensation is applied in convert_apfixed_to_float_v14()
 * 
 * IMPORTANT: NO #ifdef __SYNTHESIS__ - this code path is used for BOTH
 * C Simulation and Co-Simulation, ensuring behavioral consistency.
 */
void fft_2d_full_2048_hls_fft(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS DATAFLOW
    
    // Temporary buffers for ap_fixed type conversion
    cmpx_fft_in_t input_fixed[128][128];
    cmpx_fft_out_t output_fixed[128][128];
    
    #pragma HLS RESOURCE variable=input_fixed core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_fixed core=RAM_2P_BRAM
    
    // Step 1: Convert float to ap_fixed
    convert_float_to_apfixed_v14(input, input_fixed);
    
    // Step 2: Call HLS FFT IP (128×128)
    fft_2d_hls_128(input_fixed, output_fixed, is_inverse);
    
    // Step 3: Convert ap_fixed back to float
    // NOTE: HLS FFT scaled mode divides by N²=16384 for 2D IFFT
    // This matches FFTW BACKWARD behavior, no manual compensation needed
    convert_apfixed_to_float_v14(output_fixed, output);
}

// ============================================================================
// Main Function: SOCS 2048 Stream Refactored v14 (HLS FFT IP)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v14(
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
    // mskf_r/i: 1024×1024 = 1,048,576 (golden_1024 config)
    // scales: nk=10
    // krn_r/i: 10 kernels × 17×17 = 2,890 (depth=76832 for safety)
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
        
        // Stage 1: Embed kernel into mask spectrum (REUSE from socs_2048.cpp)
        embed_kernel_mask_padded_2048(
            mskf_r, mskf_i,
            krn_r, krn_i,
            fft_input,
            nx_actual, ny_actual,
            k,  // kernel_idx
            Lx, Ly
        );
        
        // Stage 2: 2D IFFT using HLS FFT IP (REPLACES Direct DFT)
        // HLS FFT scaled mode: 1/16 per dim → 1/256 for 2D
        // Compensation (×256) applied in convert_apfixed_to_float_v14()
        fft_2d_full_2048_hls_fft(
            fft_input,
            fft_output,
            true  // is_inverse = true for IFFT
        );
        
        // Stage 3: Accumulate intensity (REUSE from socs_2048.cpp)
        accumulate_intensity_2048(
            fft_output,
            tmpImg,
            scales[k]
        );
    }
    
    // Stage 4: FFTshift (REUSE from socs_2048.cpp)
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
