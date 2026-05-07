/**
 * @file socs_2048_stream_refactored_v8_debug_direct_dft.cpp
 * @brief SOCS HLS implementation - DEBUG VERSION: Force Direct DFT for Co-Simulation
 * 
 * DEBUG PURPOSE: Verify if HLS FFT IP causes C/RTL mismatch
 * 
 * This version forces direct DFT for both C simulation and RTL simulation
 * to isolate the issue with HLS FFT IP.
 * 
 * Expected result: Co-Simulation should PASS with direct DFT
 * If PASS: HLS FFT IP has issues (scaling, precision, or configuration)
 * If FAIL: Other parts of the code have issues
 */

#include "socs_2048.h"
// #include "hls_fft_config_2048_corrected.h"  // DISABLED: Force direct DFT
#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// Stream depth configuration (optimized for BRAM)
// Original: 16384 (128×128) - 348 BRAM for 6 streams
// Optimized: 4096 (64×64) - 90 BRAM for 2 streams, saves 258 BRAM
// Trade-off: Slightly reduced buffering, but still sufficient for DATAFLOW
const int STREAM_DEPTH = 32768;  // 128×128×2 (sufficient for 128×128 FFT)

// Parallelization factor (reduced to 1 for BRAM optimization)
const int PARALLEL_FACTOR = 1;

// ============================================================================
// Stage 1: Embed kernel × mask (Stream output)
// ============================================================================

void embed_kernel_to_stream(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    hls::stream<cmpx_2048_t> &embed_stream,
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
) {
    #pragma HLS INLINE off
    
    int kerX_actual = 2 * nx_actual + 1;
    int kerY_actual = 2 * ny_actual + 1;
    
    // Embedding position (matching Golden CPU reference)
    int embed_x = (MAX_FFT_X * (64 - kerX_actual)) / 64;
    int embed_y = (MAX_FFT_Y * (64 - kerY_actual)) / 64;
    
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    int kernel_offset = kernel_idx * kerX_actual * kerY_actual;
    
    // Process FFT array row by row
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            cmpx_2048_t val(0.0f, 0.0f);
            
            int ky = y - embed_y;
            int kx = x - embed_x;
            
            if (ky >= 0 && ky < kerY_actual && kx >= 0 && kx < kerX_actual) {
                int y_offset = ky - ny_actual;
                int x_offset = kx - nx_actual;
                
                int mask_y = Lyh + y_offset;
                int mask_x = Lxh + x_offset;
                
                if (mask_y >= 0 && mask_y < Ly && mask_x >= 0 && mask_x < Lx) {
                    float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
                    float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
                    
                    float ms_r = mskf_r[mask_y * Lx + mask_x];
                    float ms_i = mskf_i[mask_y * Lx + mask_x];
                    
                    float prod_r = kr_r * ms_r - kr_i * ms_i;
                    float prod_i = kr_r * ms_i + kr_i * ms_r;
                    
                    val = cmpx_2048_t(prod_r, prod_i);
                }
            }
            
            embed_stream.write(val);
        }
    }
}

// ============================================================================
// Stage 2: 2D FFT (Stream-based wrapper)
// ============================================================================

// ============================================================================
// Direct DFT Implementation (Forced for Debug)
// ============================================================================

/**
 * 1D FFT/IFFT using Direct DFT (Debug Version)
 * 
 * IMPORTANT: IFFT uses 1/N scaling to match FFTW BACKWARD convention
 */
void fft_1d_direct_debug(
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
        
        // Apply scaling for IFFT
        out[k] = is_inverse ? sum * scale : sum;
    }
}

/**
 * 2D FFT/IFFT using Direct DFT (Debug Version)
 */
void fft_2d_direct_debug(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    cmpx_2048_t row_temp[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t col_temp[MAX_FFT_X];
    cmpx_2048_t col_out[MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=row_temp core=RAM_2P_BRAM
    
    // Step 1: Row-wise FFT
    for (int y = 0; y < MAX_FFT_Y; y++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        fft_1d_direct_debug(input[y], row_temp[y], is_inverse);
    }
    
    // Step 2: Column-wise FFT
    for (int x = 0; x < MAX_FFT_X; x++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        // Extract column
        for (int y = 0; y < MAX_FFT_Y; y++) {
            col_temp[y] = row_temp[y][x];
        }
        
        // FFT on column
        fft_1d_direct_debug(col_temp, col_out, is_inverse);
        
        // Store result
        for (int y = 0; y < MAX_FFT_Y; y++) {
            output[y][x] = col_out[y];
        }
    }
}

void fft_2d_stream_wrapper(
    hls::stream<cmpx_2048_t> &input_stream,
    hls::stream<cmpx_2048_t> &output_stream,
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    
    // Read from stream to array
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = input_stream.read();
        }
    }
    
    // DEBUG: Force direct DFT (not HLS FFT IP)
    fft_2d_direct_debug(fft_input, fft_output, is_inverse);
    
    // Write from array to stream
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            output_stream.write(fft_output[y][x]);
        }
    }
}

// ============================================================================
// Stage 3: Accumulate intensity to DDR (BRAM optimized)
// ============================================================================

void accumulate_to_ddr(
    hls::stream<cmpx_2048_t> &fft_out_stream,
    float* tmpImg_ddr,
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            cmpx_2048_t val = fft_out_stream.read();
            
            float intensity = val.real() * val.real() + val.imag() * val.imag();
            
            // Read current value from DDR
            int idx = y * MAX_FFT_X + x;
            float current = tmpImg_ddr[idx];
            
            // Accumulate and write back to DDR
            tmpImg_ddr[idx] = current + scale * intensity;
        }
    }
}

// ============================================================================
// Stage 5: FFTshift + Output (DDR-based, no BRAM arrays)
// ============================================================================

void fftshift_and_output_from_ddr(
    float* tmpImg_ddr,
    float* output,
    int nx_actual,
    int ny_actual
) {
    #pragma HLS INLINE off
    
    // FFTshift parameters
    int halfX = MAX_FFT_X / 2;
    int halfY = MAX_FFT_Y / 2;
    
    // Direct DDR to output with FFTshift
    // No intermediate BRAM arrays - saves ~200 BRAM
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // FFTshift: swap quadrants
            int src_y = (y + halfY) % MAX_FFT_Y;
            int src_x = (x + halfX) % MAX_FFT_X;
            
            // Read from DDR with shifted index
            int src_idx = src_y * MAX_FFT_X + src_x;
            
            // Write to output with original index
            int out_idx = y * MAX_FFT_X + x;
            
            output[out_idx] = tmpImg_ddr[src_idx];
        }
    }
}

// ============================================================================
// Main function with multi-FFT instance parallelization
// ============================================================================

void calc_socs_2048_hls_stream_refactored(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,  // DDR storage for tmpImg_final
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
) {
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem2 \
        depth=810 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem3 \
        depth=810 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem4 \
        depth=16 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=tmpImg_ddr offset=slave bundle=gmem5 \
        depth=16384 latency=32 num_read_outstanding=8 max_read_burst_length=64 \
        num_write_outstanding=8 max_write_burst_length=64
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem6 \
        depth=16384 latency=32 num_write_outstanding=8 max_write_burst_length=64
    
    #pragma HLS INTERFACE s_axilite port=return
    #pragma HLS INTERFACE s_axilite port=nk
    #pragma HLS INTERFACE s_axilite port=nx_actual
    #pragma HLS INTERFACE s_axilite port=ny_actual
    #pragma HLS INTERFACE s_axilite port=Lx
    #pragma HLS INTERFACE s_axilite port=Ly
    
    // Stream channels for single instance
    hls::stream<cmpx_2048_t> embed_stream("embed_stream");
    hls::stream<cmpx_2048_t> fft_out_stream("fft_out_stream");
    
    #pragma HLS STREAM variable=embed_stream depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream depth=STREAM_DEPTH
    
    // Initialize DDR accumulator
    init_loop: for (int i = 0; i < MAX_FFT_Y * MAX_FFT_X; i++) {
        #pragma HLS PIPELINE II=1
        tmpImg_ddr[i] = 0.0f;
    }
    
    // Process kernels sequentially (UNROLL factor=1)
    kernel_loop: for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=5 max=10
        
        // Sequential processing: embed → FFT → accumulate
        embed_kernel_to_stream(
            mskf_r, mskf_i, krn_r, krn_i,
            embed_stream,
            nx_actual, ny_actual, k, Lx, Ly
        );
        
        fft_2d_stream_wrapper(embed_stream, fft_out_stream, true);
        
        accumulate_to_ddr(fft_out_stream, tmpImg_ddr, scales[k]);
    }
    
    // Stage 5: FFTshift + Output (from DDR)
    fftshift_and_output_from_ddr(tmpImg_ddr, output, nx_actual, ny_actual);
}
