/**
 * @file socs_2048_stream_refactored.cpp
 * @brief SOCS HLS implementation with TRUE DATAFLOW using stream interfaces
 * 
 * Phase 3优化：Stream接口重构
 * 
 * 关键改进：
 * 1. 使用hls::stream替代数组接口
 * 2. 创建独立的stream通道实现流水线重叠
 * 3. 解决tmpImg累积依赖问题
 * 
 * 架构设计：
 * - embed_stream: embed → FFT (FIFO缓冲)
 * - fft_out_stream: FFT → accumulate (FIFO缓冲)
 * - tmpImg_in_stream: 上一个kernel的累积结果 → accumulate
 * - tmpImg_out_stream: accumulate → 下一个kernel (或output)
 * 
 * 预期效果：
 * - Interval降低至单个kernel延迟
 * - 吞吐量提升接近nk倍
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
    hls::stream<cmpx_2048_t> &embed_stream,  // Stream output
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
) {
    #pragma HLS INLINE off
    
    // Calculate actual kernel size
    int kerX_actual = 2 * nx_actual + 1;
    int kerY_actual = 2 * ny_actual + 1;
    
    // Embedding position (matching Golden CPU reference)
    int embed_x = (MAX_FFT_X * (64 - kerX_actual)) / 64;
    int embed_y = (MAX_FFT_Y * (64 - kerY_actual)) / 64;
    
    // Mask spectrum center indices
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    // Kernel offset in krn_r/krn_i arrays
    int kernel_offset = kernel_idx * kerX_actual * kerY_actual;
    
    // Process FFT array row by row
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            cmpx_2048_t val(0.0f, 0.0f);
            
            // Check if this position is within kernel embedding region
            int ky = y - embed_y;
            int kx = x - embed_x;
            
            if (ky >= 0 && ky < kerY_actual && kx >= 0 && kx < kerX_actual) {
                // Convert kernel index to spatial offset
                int y_offset = ky - ny_actual;
                int x_offset = kx - nx_actual;
                
                // Mask spectrum position
                int mask_y = Lyh + y_offset;
                int mask_x = Lxh + x_offset;
                
                // Bounds check
                if (mask_y >= 0 && mask_y < Ly && mask_x >= 0 && mask_x < Lx) {
                    // Read kernel value
                    float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
                    float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
                    
                    // Read mask spectrum value
                    float ms_r = mskf_r[mask_y * Lx + mask_x];
                    float ms_i = mskf_i[mask_y * Lx + mask_x];
                    
                    // Compute product: K_k * M
                    float prod_r = kr_r * ms_r - kr_i * ms_i;
                    float prod_i = kr_r * ms_i + kr_i * ms_r;
                    
                    val = cmpx_2048_t(prod_r, prod_i);
                }
            }
            
            // Write to stream
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
    
    // Temporary arrays for FFT processing
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
// Stage 3: Accumulate intensity (Stream-based with tmpImg stream)
// ============================================================================

void accumulate_from_stream(
    hls::stream<cmpx_2048_t> &fft_out_stream,
    hls::stream<float> &tmpImg_in_stream,   // 上一个kernel的累积结果
    hls::stream<float> &tmpImg_out_stream,  // 当前kernel的累积结果
    float scale
) {
    #pragma HLS INLINE off
    
    // Process each pixel
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // Read FFT output from stream
            cmpx_2048_t val = fft_out_stream.read();
            
            // Compute intensity: |val|^2
            float intensity = val.real() * val.real() + val.imag() * val.imag();
            
            // Read previous accumulated value
            float prev_val = tmpImg_in_stream.read();
            
            // Accumulate with scale
            float new_val = prev_val + scale * intensity;
            
            // Write to output stream
            tmpImg_out_stream.write(new_val);
        }
    }
}

// ============================================================================
// Stage 4: FFTshift + Output (Stream-based)
// ============================================================================

void fftshift_and_output_from_stream(
    hls::stream<float> &tmpImg_stream,
    float* output,
    int nx_actual,
    int ny_actual
) {
    #pragma HLS INLINE off
    
    // Temporary arrays for FFTshift
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
    // Read from stream to array
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = tmpImg_stream.read();
        }
    }
    
    // FFTshift
    fftshift_2d_2048(tmpImg, tmpImgp);
    
    // Extract valid region
    int convX_actual = 4 * nx_actual + 1;
    int convY_actual = 4 * ny_actual + 1;
    int start_x = (MAX_FFT_X - convX_actual) / 2;
    int start_y = (MAX_FFT_Y - convY_actual) / 2;
    
    for (int y = 0; y < convY_actual; y++) {
        for (int x = 0; x < convX_actual; x++) {
            #pragma HLS PIPELINE II=1
            output[y * convX_actual + x] = tmpImgp[start_y + y][start_x + x];
        }
    }
}

// ============================================================================
// Helper function: Initialize tmpImg stream with zeros
// ============================================================================

void init_tmpImg_stream(
    hls::stream<float> &tmpImg_stream
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_stream.write(0.0f);
        }
    }
}

// ============================================================================
// Main function with TRUE DATAFLOW optimization
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
    hls::stream<float> tmpImg_stream_0("tmpImg_stream_0");
    hls::stream<float> tmpImg_stream_1("tmpImg_stream_1");
    
    #pragma HLS STREAM variable=embed_stream depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream depth=STREAM_DEPTH
    #pragma HLS STREAM variable=tmpImg_stream_0 depth=STREAM_DEPTH
    #pragma HLS STREAM variable=tmpImg_stream_1 depth=STREAM_DEPTH
    
    // Initialize tmpImg stream with zeros
    init_tmpImg_stream(tmpImg_stream_0);
    
    // Process each kernel with TRUE DATAFLOW
    process_kernels:
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=4 max=16
        #pragma HLS DATAFLOW
        
        // Select input/output streams based on kernel index
        hls::stream<float> &tmpImg_in = (k % 2 == 0) ? tmpImg_stream_0 : tmpImg_stream_1;
        hls::stream<float> &tmpImg_out = (k % 2 == 0) ? tmpImg_stream_1 : tmpImg_stream_0;
        
        // Stage 1: Embed (Stream output)
        embed_kernel_to_stream(
            mskf_r, mskf_i, krn_r, krn_i,
            embed_stream,
            nx_actual, ny_actual, k, Lx, Ly
        );
        
        // Stage 2: IFFT (Stream-based)
        fft_2d_stream_wrapper(embed_stream, fft_out_stream, true);
        
        // Stage 3: Accumulate (Stream-based with tmpImg stream)
        accumulate_from_stream(
            fft_out_stream,
            tmpImg_in,
            tmpImg_out,
            scales[k]
        );
    }
    
    // Stage 4: FFTshift + Output
    // Use the final tmpImg stream (depends on nk parity)
    hls::stream<float> &final_tmpImg = (nk % 2 == 0) ? tmpImg_stream_0 : tmpImg_stream_1;
    fftshift_and_output_from_stream(final_tmpImg, output, nx_actual, ny_actual);
}
