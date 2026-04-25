/**
 * @file socs_2048_ddr_optimized.cpp
 * @brief SOCS HLS implementation with DDR access optimization
 * 
 * 优化策略：
 * - Step 2.1: 增大Burst长度和Outstanding请求
 *   - max_read_burst_length: 64 → 128
 *   - num_read_outstanding: 8 → 16
 *   - 预期带宽提升: 2倍
 * 
 * - Step 2.2: 优化mask数据访问模式
 *   - 当前问题: 每个kernel重复读取相同mask数据
 *   - 优化方案: 使用局部缓存减少DDR访问
 * 
 * 基于版本: socs_2048_stream_optimized.cpp (Step 1完成版本)
 */

#include "socs_2048.h"
#include "hls_fft_config_2048.h"
#include <hls_stream.h>
#include <complex>

using namespace std;

// Type definitions
typedef complex<float> cmpx_2048_t;

// ============================================================================
// Stage 1: Embed kernel × mask (Optimized DDR access)
// ============================================================================

void embed_kernel_ddr_optimized(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
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
            
            // Write directly to array
            fft_input[y][x] = val;
        }
    }
}

// ============================================================================
// Stage 2: 2D FFT (Array-based, 保持原始效率)
// ============================================================================

// 使用原始socs_2048.cpp中的fft_2d_full_2048函数
// 该函数已优化，Interval = 83,586 cycles

// ============================================================================
// Stage 3: Accumulate intensity (Array-based)
// ============================================================================

void accumulate_array_ddr_optimized(
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            float intensity = fft_output[y][x].real() * fft_output[y][x].real() +
                              fft_output[y][x].imag() * fft_output[y][x].imag();
            tmpImg[y][x] += intensity * scale;
        }
    }
}

// ============================================================================
// Stage 4: FFTshift + Output
// ============================================================================

void fftshift_and_output_ddr_optimized(
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float* output,
    int nx_actual,
    int ny_actual
) {
    #pragma HLS INLINE off
    
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    
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
// Main function with DDR access optimization
// ============================================================================

void calc_socs_2048_hls_ddr_optimized(
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
    // ========================================================================
    // Step 2.1: 增大Burst长度和Outstanding请求
    // ========================================================================
    
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=1048576 latency=32 \
        num_read_outstanding=16 \
        max_read_burst_length=128
    
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=1048576 latency=32 \
        num_read_outstanding=16 \
        max_read_burst_length=128
    
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem2 \
        depth=810 latency=32 \
        num_read_outstanding=16 \
        max_read_burst_length=128
    
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem3 \
        depth=810 latency=32 \
        num_read_outstanding=16 \
        max_read_burst_length=128
    
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem4 \
        depth=16 latency=32 \
        num_read_outstanding=16 \
        max_read_burst_length=128
    
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 \
        depth=289 latency=32 \
        num_write_outstanding=16 \
        max_write_burst_length=128
    
    #pragma HLS INTERFACE s_axilite port=return
    #pragma HLS INTERFACE s_axilite port=nk
    #pragma HLS INTERFACE s_axilite port=nx_actual
    #pragma HLS INTERFACE s_axilite port=ny_actual
    #pragma HLS INTERFACE s_axilite port=Lx
    #pragma HLS INTERFACE s_axilite port=Ly
    
    // Internal arrays
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X];
    
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    
    // Initialize tmpImg
    init_tmpImg:
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // Process each kernel with DATAFLOW
    process_kernels:
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=4 max=16
        #pragma HLS DATAFLOW
        
        // Stage 1: Embed
        embed_kernel_ddr_optimized(
            mskf_r, mskf_i, krn_r, krn_i,
            fft_input, nx_actual, ny_actual, k, Lx, Ly
        );
        
        // Stage 2: IFFT
        fft_2d_full_2048(fft_input, fft_output, true);
        
        // Stage 3: Accumulate
        accumulate_array_ddr_optimized(fft_output, tmpImg, scales[k]);
    }
    
    // Stage 4: FFTshift + Output
    fftshift_and_output_ddr_optimized(tmpImg, output, nx_actual, ny_actual);
}
