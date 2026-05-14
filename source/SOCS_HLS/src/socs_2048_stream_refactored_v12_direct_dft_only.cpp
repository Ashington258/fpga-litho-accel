/**
 * @file socs_2048_stream_refactored_v12_direct_dft_only.cpp
 * @brief SOCS HLS implementation - Force Direct DFT for both C Sim and Co-Sim
 * 
 * DEBUG PURPOSE: Isolate Co-Simulation NaN issue
 * 
 * This version forces direct DFT for BOTH C simulation and RTL simulation
 * by removing the __SYNTHESIS__ conditional compilation.
 * 
 * Expected result: Co-Simulation should PASS (same as C Simulation)
 */

#include "socs_2048.h"
// #include "hls_fft_config_2048_corrected.h"  // DISABLED: Force direct DFT
#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// Stream depth configuration
const int STREAM_DEPTH = 32768;  // 128×128×2

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
    
    int embed_x = (MAX_FFT_X * (64 - kerX_actual)) / 64;
    int embed_y = (MAX_FFT_Y * (64 - kerY_actual)) / 64;
    
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    int kernel_offset = kernel_idx * kerX_actual * kerY_actual;
    
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
// Stage 2: 2D FFT (Direct DFT - NO conditional compilation)
// ============================================================================

void fft_2d_stream_wrapper_direct_dft(
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
    
    // Call Direct DFT (from socs_2048.cpp)
    // This is the C Simulation path, but we force it for Co-Sim too
    extern void fft_2d_full_2048(
        cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
        cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
        bool is_inverse
    );
    
    fft_2d_full_2048(fft_input, fft_output, is_inverse);
    
    // Write from array to stream
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            output_stream.write(fft_output[y][x]);
        }
    }
}

// ============================================================================
// Stage 3: Accumulate intensity to BRAM
// ============================================================================

void accumulate_to_bram(
    hls::stream<cmpx_2048_t> &fft_out_stream,
    float tmpImg_local[MAX_FFT_Y][MAX_FFT_X],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            cmpx_2048_t val = fft_out_stream.read();
            
            float intensity = val.real() * val.real() + val.imag() * val.imag();
            
            tmpImg_local[y][x] += scale * intensity;
        }
    }
}

// ============================================================================
// Stage 3b: Write BRAM to DDR
// ============================================================================

void write_bram_to_ddr(
    float tmpImg_local[MAX_FFT_Y][MAX_FFT_X],
    float* tmpImg_ddr
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            int idx = y * MAX_FFT_X + x;
            tmpImg_ddr[idx] = tmpImg_local[y][x];
        }
    }
}

// ============================================================================
// Stage 5: FFTshift + Output (DDR-based)
// ============================================================================

void fftshift_and_output_from_ddr(
    float* tmpImg_ddr,
    float* output,
    int nx_actual,
    int ny_actual
) {
    #pragma HLS INLINE off
    
    int halfX = MAX_FFT_X / 2;
    int halfY = MAX_FFT_Y / 2;
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            int src_y = (y + halfY) % MAX_FFT_Y;
            int src_x = (x + halfX) % MAX_FFT_X;
            
            int src_idx = src_y * MAX_FFT_X + src_x;
            int out_idx = y * MAX_FFT_X + x;
            
            output[out_idx] = tmpImg_ddr[src_idx];
        }
    }
}

// ============================================================================
// Main function (v12 - Direct DFT only)
// ============================================================================

void calc_socs_2048_hls_stream_refactored_v12(
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
    
    hls::stream<cmpx_2048_t> embed_stream("embed_stream");
    hls::stream<cmpx_2048_t> fft_out_stream("fft_out_stream");
    
    #pragma HLS STREAM variable=embed_stream depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream depth=STREAM_DEPTH
    
    float tmpImg_local[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=tmpImg_local core=RAM_2P_BRAM
    
    // Initialize BRAM accumulator
    init_bram: for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_local[y][x] = 0.0f;
        }
    }
    
    // Process kernels sequentially
    kernel_loop: for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=5 max=10
        
        embed_kernel_to_stream(
            mskf_r, mskf_i, krn_r, krn_i,
            embed_stream,
            nx_actual, ny_actual, k, Lx, Ly
        );
        
        fft_2d_stream_wrapper_direct_dft(embed_stream, fft_out_stream, true);
        
        accumulate_to_bram(fft_out_stream, tmpImg_local, scales[k]);
    }
    
    write_bram_to_ddr(tmpImg_local, tmpImg_ddr);
    
    fftshift_and_output_from_ddr(tmpImg_ddr, output, nx_actual, ny_actual);
}
