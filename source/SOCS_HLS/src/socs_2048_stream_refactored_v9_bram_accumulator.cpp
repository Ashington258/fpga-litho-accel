// ============================================================================
// SOCS 2048 Stream Refactored v9 - BRAM Accumulator (Fix for Co-Sim NaN)
// ============================================================================
// Purpose: Fix Co-Simulation NaN issue by using BRAM accumulator instead of DDR
// 
// Key Changes from v8:
// 1. Use local BRAM array for accumulation (avoid DDR read-modify-write)
// 2. Write final result to DDR after all kernels processed
// 3. This ensures stable RTL simulation
//
// Resource Impact:
// - BRAM: +29 (for tmpImg_local buffer)
// - Total BRAM: ~457 (63%, still within limit)
// ============================================================================

#include <hls_stream.h>
#include <ap_fixed.h>
#include <complex>
#include <cmath>

// Configuration
const int MAX_FFT_X = 128;
const int MAX_FFT_Y = 128;
const int STREAM_DEPTH = 32768;  // Increased to avoid stream overflow

// FFT precision
typedef ap_fixed<24, 1> data_fft_in_t;
typedef std::complex<data_fft_in_t> cmpx_2048_t;

// ============================================================================
// Stage 1: Embed kernel into mask spectrum (Stream-based)
// ============================================================================

void embed_kernel_to_stream(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    hls::stream<cmpx_2048_t> &output_stream,
    int nx_actual, int ny_actual, int kernel_idx,
    int Lx, int Ly
) {
    #pragma HLS INLINE off
    
    int kerX = 2 * nx_actual + 1;
    int kerY = 2 * ny_actual + 1;
    
    // Process each FFT point
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // Map FFT coordinates to mask coordinates
            int mask_x = x % Lx;
            int mask_y = y % Ly;
            
            // Get mask spectrum value
            int mask_idx = mask_y * Lx + mask_x;
            float mask_r = mskf_r[mask_idx];
            float mask_i = mskf_i[mask_idx];
            
            // Get kernel value (with zero-padding)
            float krn_val_r = 0.0f;
            float krn_val_i = 0.0f;
            
            int ker_start_x = (MAX_FFT_X - kerX) / 2;
            int ker_start_y = (MAX_FFT_Y - kerY) / 2;
            
            if (x >= ker_start_x && x < ker_start_x + kerX &&
                y >= ker_start_y && y < ker_start_y + kerY) {
                int ker_x = x - ker_start_x;
                int ker_y = y - ker_start_y;
                int krn_idx = kernel_idx * kerX * kerY + ker_y * kerX + ker_x;
                krn_val_r = krn_r[krn_idx];
                krn_val_i = krn_i[krn_idx];
            }
            
            // Complex multiplication: mask × kernel
            float result_r = mask_r * krn_val_r - mask_i * krn_val_i;
            float result_i = mask_r * krn_val_i + mask_i * krn_val_r;
            
            // Convert to fixed-point and write to stream
            cmpx_2048_t val(result_r, result_i);
            output_stream.write(val);
        }
    }
}

// ============================================================================
// Stage 2: 2D FFT (Direct DFT for debugging)
// ============================================================================

void fft_1d_direct_2048(
    cmpx_2048_t input[MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    const float pi = 3.14159265358979323846f;
    const float scale = 1.0f / MAX_FFT_X;
    
    for (int k = 0; k < MAX_FFT_X; k++) {
        #pragma HLS PIPELINE II=1
        
        float sum_r = 0.0f;
        float sum_i = 0.0f;
        
        for (int n = 0; n < MAX_FFT_X; n++) {
            #pragma HLS UNROLL factor=4
            
            // Twiddle factor: exp(±2πi*k*n/N)
            // IFFT: positive sign (+2πi*k*n/N)
            // FFT: negative sign (-2πi*k*n/N)
            float angle = (is_inverse ? +2.0f : -2.0f) * pi * k * n / MAX_FFT_X;
            float cos_val = std::cos(angle);
            float sin_val = std::sin(angle);
            
            // Complex multiplication: input[n] * twiddle
            float in_r = input[n].real().to_float();
            float in_i = input[n].imag().to_float();
            
            sum_r += in_r * cos_val - in_i * sin_val;
            sum_i += in_r * sin_val + in_i * cos_val;
        }
        
        // Apply scaling for IFFT
        if (is_inverse) {
            output[k] = cmpx_2048_t(sum_r * scale, sum_i * scale);
        } else {
            output[k] = cmpx_2048_t(sum_r, sum_i);
        }
    }
}

void fft_2d_direct_debug(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Temporary arrays
    cmpx_2048_t row_fft[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=row_fft core=RAM_2P_BRAM
    
    // Row-wise FFT
    for (int y = 0; y < MAX_FFT_Y; y++) {
        #pragma HLS PIPELINE II=1
        fft_1d_direct_2048(input[y], row_fft[y], is_inverse);
    }
    
    // Column-wise FFT
    for (int x = 0; x < MAX_FFT_X; x++) {
        #pragma HLS PIPELINE II=1
        
        cmpx_2048_t col_in[MAX_FFT_Y];
        cmpx_2048_t col_out[MAX_FFT_Y];
        
        // Read column
        for (int y = 0; y < MAX_FFT_Y; y++) {
            col_in[y] = row_fft[y][x];
        }
        
        // FFT on column
        fft_1d_direct_2048(col_in, col_out, is_inverse);
        
        // Write column
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
    
    // Arrays for FFT processing
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
// Stage 3: Accumulate intensity to BRAM (FIX: Avoid DDR read-modify-write)
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
            
            // Direct accumulation to BRAM (stable in RTL simulation)
            tmpImg_local[y][x] += scale * intensity;
        }
    }
}

// ============================================================================
// Stage 4: Write BRAM to DDR (one-time write, no read-modify-write)
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
    int nx_actual, int ny_actual
) {
    #pragma HLS INLINE off
    
    int shift_x = MAX_FFT_X / 2;
    int shift_y = MAX_FFT_Y / 2;
    
    int output_x = 2 * nx_actual + 1;
    int output_y = 2 * ny_actual + 1;
    
    int start_x = (MAX_FFT_X - output_x) / 2;
    int start_y = (MAX_FFT_Y - output_y) / 2;
    
    for (int y = 0; y < output_y; y++) {
        for (int x = 0; x < output_x; x++) {
            #pragma HLS PIPELINE II=1
            
            int src_y = (start_y + y + shift_y) % MAX_FFT_Y;
            int src_x = (start_x + x + shift_x) % MAX_FFT_X;
            
            int src_idx = src_y * MAX_FFT_X + src_x;
            int dst_idx = y * output_x + x;
            
            output[dst_idx] = tmpImg_ddr[src_idx];
        }
    }
}

// ============================================================================
// Top-level Function
// ============================================================================

extern "C" {
void calc_socs_2048_hls_stream_refactored_v9(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* tmpImg_ddr,
    float* output,
    int nk, int nx_actual, int ny_actual,
    int Lx, int Ly
) {
    // AXI-MM interfaces
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=1048576 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem2 \
        depth=16 latency=32 num_read_outstanding=8 max_read_burst_length=64
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem3 \
        depth=16 latency=32 num_read_outstanding=8 max_read_burst_length=64
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
    
    // CRITICAL FIX: Use BRAM for accumulation (avoid DDR read-modify-write)
    float tmpImg_local[MAX_FFT_Y][MAX_FFT_X];
    #pragma HLS RESOURCE variable=tmpImg_local core=RAM_2P_BRAM
    
    // Initialize BRAM accumulator
    init_loop: for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg_local[y][x] = 0.0f;
        }
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
        
        accumulate_to_bram(fft_out_stream, tmpImg_local, scales[k]);
    }
    
    // Write BRAM to DDR (one-time write, no read-modify-write)
    write_bram_to_ddr(tmpImg_local, tmpImg_ddr);
    
    // Stage 5: FFTshift + Output (from DDR)
    fftshift_and_output_from_ddr(tmpImg_ddr, output, nx_actual, ny_actual);
}
}
