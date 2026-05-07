/**
 * @file socs_2048_stream_refactored_v4.cpp
 * @brief SOCS HLS implementation with Multi-FFT Instance Parallelization + BRAM Optimization
 * 
 * Phase 3优化：多FFT实例并行（UNROLL factor=2）+ Stream深度优化
 * 
 * v4优化内容：
 * - Stream深度从16384降至4096，节省258 BRAM
 * - 保持DATAFLOW流水线重叠性能
 * - 支持2048×2048 mask输入（Nx=16）
 * 
 * 设计目标：
 * - 使用UNROLL factor=2实现2个kernel并行处理
 * - 每个并行实例有独立的tmpImg_local，避免数据竞争
 * - 最终合并所有tmpImg_local到最终tmpImg
 * - 预期：吞吐量提升约2倍，BRAM占用率<75%
 * 
 * 架构：
 * - kernel_loop: UNROLL factor=2（2个kernel并行）
 * - 每个并行实例：embed → FFT → accumulate_to_local
 * - 合并阶段：merge_all_locals_to_final
 * - 输出阶段：FFTshift + output
 */

#include "socs_2048.h"
#include "hls_fft_config_2048_corrected.h"
#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// Stream depth configuration (optimized for BRAM)
// Original: 16384 (128×128) - 348 BRAM for 6 streams
// Optimized: 4096 (64×64) - 90 BRAM for 6 streams, saves 258 BRAM
// Trade-off: Slightly reduced buffering, but still sufficient for DATAFLOW
const int STREAM_DEPTH = 4096;  // 64×64

// Parallelization factor
const int PARALLEL_FACTOR = 2;

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
// Stage 3: Accumulate intensity to local array (for parallel instances)
// ============================================================================

void accumulate_to_local(
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
            
            tmpImg_local[y][x] = scale * intensity;  // Store scaled intensity
        }
    }
}

// ============================================================================
// Stage 4: Merge local arrays to final accumulator
// ============================================================================

void merge_local_to_final(
    float tmpImg_local[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg_final[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_final[y][x] += tmpImg_local[y][x];
        }
    }
}

// ============================================================================
// Stage 5: FFTshift + Output
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
// Main function with multi-FFT instance parallelization
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
    
    // Stream channels for parallel instances
    hls::stream<cmpx_2048_t> embed_stream_0("embed_stream_0");
    hls::stream<cmpx_2048_t> embed_stream_1("embed_stream_1");
    hls::stream<cmpx_2048_t> fft_out_stream_0("fft_out_stream_0");
    hls::stream<cmpx_2048_t> fft_out_stream_1("fft_out_stream_1");
    
    #pragma HLS STREAM variable=embed_stream_0 depth=STREAM_DEPTH
    #pragma HLS STREAM variable=embed_stream_1 depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream_0 depth=STREAM_DEPTH
    #pragma HLS STREAM variable=fft_out_stream_1 depth=STREAM_DEPTH
    
    // Local accumulator arrays for parallel instances
    float tmpImg_local_0[MAX_FFT_Y][MAX_FFT_X];
    float tmpImg_local_1[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImg_local_0 core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImg_local_1 core=RAM_2P_BRAM
    #pragma HLS ARRAY_PARTITION variable=tmpImg_local_0 cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImg_local_1 cyclic factor=4 dim=2
    
    // Final accumulator array
    float tmpImg_final[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImg_final core=RAM_2P_BRAM
    #pragma HLS ARRAY_PARTITION variable=tmpImg_final cyclic factor=4 dim=2
    
    // Initialize final accumulator
    init_loop: for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_final[y][x] = 0.0f;
        }
    }
    
    // Process kernels in pairs (UNROLL factor=2)
    int k = 0;
    kernel_pair_loop: for (k = 0; k < nk - 1; k += 2) {
        #pragma HLS LOOP_TRIPCOUNT min=2 max=8
        #pragma HLS DATAFLOW
        
        // Parallel instance 0: kernel k
        {
            embed_kernel_to_stream(
                mskf_r, mskf_i, krn_r, krn_i,
                embed_stream_0,
                nx_actual, ny_actual, k, Lx, Ly
            );
            
            fft_2d_stream_wrapper(embed_stream_0, fft_out_stream_0, true);
            
            accumulate_to_local(fft_out_stream_0, tmpImg_local_0, scales[k]);
        }
        
        // Parallel instance 1: kernel k+1
        {
            embed_kernel_to_stream(
                mskf_r, mskf_i, krn_r, krn_i,
                embed_stream_1,
                nx_actual, ny_actual, k+1, Lx, Ly
            );
            
            fft_2d_stream_wrapper(embed_stream_1, fft_out_stream_1, true);
            
            accumulate_to_local(fft_out_stream_1, tmpImg_local_1, scales[k+1]);
        }
        
        // Merge both local arrays to final
        merge_local_to_final(tmpImg_local_0, tmpImg_final);
        merge_local_to_final(tmpImg_local_1, tmpImg_final);
    }
    
    // Handle odd kernel (if nk is odd)
    if (k < nk) {
        #pragma HLS LOOP_TRIPCOUNT min=0 max=1
        
        hls::stream<cmpx_2048_t> embed_stream_odd("embed_stream_odd");
        hls::stream<cmpx_2048_t> fft_out_stream_odd("fft_out_stream_odd");
        
        #pragma HLS STREAM variable=embed_stream_odd depth=STREAM_DEPTH
        #pragma HLS STREAM variable=fft_out_stream_odd depth=STREAM_DEPTH
        
        float tmpImg_local_odd[MAX_FFT_Y][MAX_FFT_X];
        #pragma HLS RESOURCE variable=tmpImg_local_odd core=RAM_2P_BRAM
        
        embed_kernel_to_stream(
            mskf_r, mskf_i, krn_r, krn_i,
            embed_stream_odd,
            nx_actual, ny_actual, k, Lx, Ly
        );
        
        fft_2d_stream_wrapper(embed_stream_odd, fft_out_stream_odd, true);
        
        accumulate_to_local(fft_out_stream_odd, tmpImg_local_odd, scales[k]);
        
        merge_local_to_final(tmpImg_local_odd, tmpImg_final);
    }
    
    // Stage 5: FFTshift + Output
    fftshift_and_output_from_array(tmpImg_final, output, nx_actual, ny_actual);
}
