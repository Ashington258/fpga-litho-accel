/**
 * @file socs_2048_stream_refactored_v2.cpp
 * @brief SOCS HLS implementation with TRUE DATAFLOW - Fixed version
 * 
 * Phase 3优化：Stream接口重构（修正版）
 * 
 * 问题诊断：
 * - 原版本在DATAFLOW循环内使用条件stream引用（tmpImg_in/tmpImg_out）
 * - HLS无法在编译时确定数据流路径，导致DATAFLOW失效
 * 
 * 修正方案：
 * - 使用数组进行累积（避免stream依赖问题）
 * - 保持embed和FFT的stream优化（部分流水线收益）
 * - 最终目标：验证功能正确性，然后探索更好的并行化方案
 */

#include "socs_2048.h"
#include "hls_fft_config_2048_corrected.h"
#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// Stream depth configuration
const int STREAM_DEPTH = MAX_FFT_X * MAX_FFT_Y;  // 128×128 = 16384

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
    
    // Call original FFT function
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
// Stage 3: Accumulate intensity (Array-based for correctness)
// ============================================================================

void accumulate_from_stream_to_array(
    hls::stream<cmpx_2048_t> &fft_out_stream,
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            cmpx_2048_t val = fft_out_stream.read();
            
            float intensity = val.real() * val.real() + val.imag() * val.imag();
            
            tmpImg[y][x] += scale * intensity;
        }
    }
}

// ============================================================================
// Stage 4: FFTshift + Output
// ============================================================================

void fftshift_and_output_from_array(
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float* output,
    int nx_actual,
    int ny_actual
) {
    #pragma HLS INLINE off
    
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
    // FFTshift
    int halfX = MAX_FFT_X / 2;
    int halfY = MAX_FFT_Y / 2;
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            int src_y = (y + halfY) % MAX_FFT_Y;
            int src_x = (x + halfX) % MAX_FFT_X;
            
            tmpImgp[y][x] = tmpImg[src_y][src_x];
        }
    }
    
    // Output full 128×128
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            int out_idx = y * MAX_FFT_X + x;
            output[out_idx] = tmpImgp[y][x];
        }
    }
}

// ============================================================================
// Main function with stream optimization (partial DATAFLOW)
// ============================================================================

void calc_socs_2048_hls_stream_refactored(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
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
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 \
        depth=289 latency=32 num_write_outstanding=8 max_write_burst_length=64
    
    #pragma HLS INTERFACE s_axilite port=return
    #pragma HLS INTERFACE s_axilite port=nk
    #pragma HLS INTERFACE s_axilite port=nx_actual
    #pragma HLS INTERFACE s_axilite port=ny_actual
    #pragma HLS INTERFACE s_axilite port=Lx
    #pragma HLS INTERFACE s_axilite port=Ly
    
    // Stream channels
    hls::stream<cmpx_2048_t> embed_stream("embed_stream");
    hls::stream<cmpx_2048_t> fft_out_stream("fft_out_stream");
    
    #pragma HLS STREAM variable=embed_stream depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream depth=STREAM_DEPTH
    
    // Accumulator array (BRAM-based)
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    #pragma HLS ARRAY_PARTITION variable=tmpImg cyclic factor=4 dim=2
    
    // Initialize accumulator
    init_loop: for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // Process each kernel
    kernel_loop: for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=4 max=16
        #pragma HLS DATAFLOW
        
        // Stage 1: Embed (Stream output)
        embed_kernel_to_stream(
            mskf_r, mskf_i, krn_r, krn_i,
            embed_stream,
            nx_actual, ny_actual, k, Lx, Ly
        );
        
        // Stage 2: IFFT (Stream-based)
        fft_2d_stream_wrapper(embed_stream, fft_out_stream, true);
        
        // Stage 3: Accumulate (Array-based)
        accumulate_from_stream_to_array(fft_out_stream, tmpImg, scales[k]);
    }
    
    // Stage 4: FFTshift + Output
    fftshift_and_output_from_array(tmpImg, output, nx_actual, ny_actual);
}
