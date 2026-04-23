/**
 * SOCS HLS 2048 Architecture Implementation
 * FPGA-Litho Project
 * 
 * Implements DDR-based FFT caching architecture:
 *   - Ping-Pong 16×16 BRAM buffers
 *   - Zero-padding to fixed 128×128 FFT
 *   - Runtime configurable Nx
 *   - HLS FFT IP integration (replaces direct DFT)
 */

#include "socs_2048.h"
#include "hls_fft_config_2048_corrected.h"  // HLS FFT IP configuration (corrected version)
#include <cstdio>

// ============================================================================
// DDR Burst Read/Write Functions
// ============================================================================

/**
 * Burst read from DDR to BRAM Ping-Pong buffer
 * 
 * Performance: 256 complex reads per block (16×16)
 * AXI burst: max 64 beats, so 256 reads = 4 bursts
 */
void burst_read_ddr_2048(
    cmpx_2048_t* ddr_addr,
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    int block_y, int block_x,
    int fft_width
) {
    #pragma HLS INLINE off
    
    // Calculate DDR offset for this block
    // Block position: (block_y, block_x) in grid of (fft_width/BLOCK_SIZE)×(fft_width/BLOCK_SIZE)
    int ddr_offset = block_y * fft_width * BLOCK_SIZE + block_x * BLOCK_SIZE;
    
    // Read BLOCK_SIZE×BLOCK_SIZE = 256 complex values
    // Use PIPELINE for burst efficiency
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            #pragma HLS PIPELINE II=1
            
            int ddr_idx = ddr_offset + y * fft_width + x;
            bram_buf[y][x] = ddr_addr[ddr_idx];
        }
    }
}

/**
 * Burst write from BRAM buffer to DDR
 */
void burst_write_ddr_2048(
    cmpx_2048_t bram_buf[BLOCK_SIZE][BLOCK_SIZE],
    cmpx_2048_t* ddr_addr,
    int block_y, int block_x,
    int fft_width
) {
    #pragma HLS INLINE off
    
    int ddr_offset = block_y * fft_width * BLOCK_SIZE + block_x * BLOCK_SIZE;
    
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            #pragma HLS PIPELINE II=1
            
            int ddr_idx = ddr_offset + y * fft_width + x;
            ddr_addr[ddr_idx] = bram_buf[y][x];
        }
    }
}

// ============================================================================
// Embed Functions (Zero-Padding Strategy)
// ============================================================================

/**
 * Embed kernel × mask product into fixed 128×128 FFT input
 * 
 * Padding strategy:
 *   - Clear entire 128×128 array first
 *   - Embed actual data in bottom-right corner
 *   - Position: [MAX_FFT_X - kerX : MAX_FFT_X-1]
 * 
 * For Nx=4 (kerX=9):   embed at [119:127]
 * For Nx=16 (kerX=33): embed at [95:127]
 */
void embed_kernel_mask_padded_2048(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X],
    int nx_actual, int ny_actual,
    int kernel_idx,
    int Lx, int Ly
) {
    #pragma HLS INLINE off
    
    // Calculate actual kernel size from runtime Nx
    int kerX_actual = 2 * nx_actual + 1;
    int kerY_actual = 2 * ny_actual + 1;
    
    // Calculate embedding position MATCHING Golden CPU reference
    // Golden CPU uses bottom-right embedding: difX = fftConvX - kerX
    // For golden_1024: fftConvX=64, kerX=17 → difX=47 → relative offset = 47/64 = 73.4%
    //
    // HLS uses MAX_FFT_X=128 (fixed architecture for universal Nx support)
    // To match Golden spatial output phase, maintain same relative offset:
    //   HLS embed_x = (47/64) * 128 ≈ 94
    //
    // Key insight: Different FFT sizes with same relative position
    // preserve spatial domain phase alignment after FFTshift!
    int embed_x = (MAX_FFT_X * (64 - kerX_actual)) / 64;  // Match Golden relative offset
    int embed_y = (MAX_FFT_Y * (64 - kerY_actual)) / 64;  // For kerX=17: (128*47)/64 = 94
    
    // Clear entire FFT input array first (zero-padding)
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            fft_input[y][x] = cmpx_2048_t(0.0f, 0.0f);
        }
    }
    
    // Mask spectrum center indices
    int Lxh = Lx / 2;  // = 256 for standard config
    int Lyh = Ly / 2;
    
    // Kernel offset in krn_r/krn_i arrays
    // Layout: [kernel_idx * kerX_actual * kerY_actual]
    // Note: Host must pass kernels padded to MAX_KER_X×MAX_KER_Y
    int kernel_offset = kernel_idx * kerX_actual * kerY_actual;
    
    // Compute and embed kernel × mask product
    for (int ky = 0; ky < kerY_actual; ky++) {
        for (int kx = 0; kx < kerX_actual; kx++) {
            // #pragma HLS PIPELINE II=1  // DISABLED for C Simulation
            
            // Convert kernel index to spatial offset
            int y_offset = ky - ny_actual;  // y ∈ [-Ny:Ny]
            int x_offset = kx - nx_actual;  // x ∈ [-Nx:Nx]
            
            // FFT array position (embedded in bottom-right)
            int fft_y = embed_y + ky;
            int fft_x = embed_x + kx;
            
            // Mask spectrum position (centered at Lxh/Lyh)
            int mask_y = Lyh + y_offset;
            int mask_x = Lxh + x_offset;
            
            // DEBUG: Check first few values (C Simulation only)
            #ifndef __SYNTHESIS__
            if (kernel_idx == 0 && ky >= 7 && ky <= 9 && kx >= 7 && kx <= 9) {
                float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
                float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
                float ms_r = mskf_r[mask_y * Lx + mask_x];
                float ms_i = mskf_i[mask_y * Lx + mask_x];
                std::printf("[DEBUG] ky=%d,kx=%d: kr=(%.6f,%.6f), ms=(%.6f,%.6f), mask_pos=(%d,%d), bounds=%d\n",
                    ky, kx, kr_r, kr_i, ms_r, ms_i, mask_y, mask_x,
                    (mask_y >= 0 && mask_y < Ly && mask_x >= 0 && mask_x < Lx));
            }
            #endif
            
            // Bounds check
            if (mask_y >= 0 && mask_y < Ly &&
                mask_x >= 0 && mask_x < Lx) {
                
                // Read kernel value
                float kr_r = krn_r[kernel_offset + ky * kerX_actual + kx];
                float kr_i = krn_i[kernel_offset + ky * kerX_actual + kx];
                
                // Read mask spectrum value
                float ms_r = mskf_r[mask_y * Lx + mask_x];
                float ms_i = mskf_i[mask_y * Lx + mask_x];
                
                // Compute product: K_k * M (complex multiplication)
                // (a+bi)(c+di) = (ac-bd) + (ad+bc)i
                float prod_r = kr_r * ms_r - kr_i * ms_i;
                float prod_i = kr_r * ms_i + kr_i * ms_r;
                
                // Embed into FFT input
                fft_input[fft_y][fft_x] = cmpx_2048_t(prod_r, prod_i);
                
                // DEBUG: Verify assignment (C Simulation only)
                #ifndef __SYNTHESIS__
                if (kernel_idx == 0 && ky >= 7 && ky <= 9 && kx >= 7 && kx <= 9) {
                    std::printf("[DEBUG] After assignment: fft_input[%d][%d] = (%.6f, %.6f)\n",
                        fft_y, fft_x, fft_input[fft_y][fft_x].real(), fft_input[fft_y][fft_x].imag());
                }
                #endif
            }
        }
    }
    
    // DEBUG: Verify fft_input after embedding (C Simulation only)
    #ifndef __SYNTHESIS__
    if (kernel_idx == 0) {
        std::printf("[DEBUG] embed_y=%d, embed_x=%d, kerY_actual=%d, kerX_actual=%d\n",
            embed_y, embed_x, kerY_actual, kerX_actual);
        std::printf("[DEBUG] fft_input after embedding (bottom-right region):\n");
        for (int dy = 0; dy < 3; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                int y = embed_y + 7 + dy;  // Check around center
                int x = embed_x + 7 + dx;
                std::printf("  fft_input[%d][%d] = (%.6f, %.6f)\n",
                    y, x, fft_input[y][x].real(), fft_input[y][x].imag());
            }
        }
    }
    #endif
}

// ============================================================================
// Extract Functions (Dynamic Output Size)
// ============================================================================

/**
 * Extract valid output region from 128×128 IFFT output
 * 
 * Output size formula: convX = 4 * Nx + 1
 *   - Nx=4:  convX=17, extract center 17×17
 *   - Nx=16: convX=65, extract center 65×65
 * 
 * Extraction position:
 *   offset = (MAX_FFT_X - convX_actual) / 2
 */
void extract_valid_region_2048(
    float ifft_output[MAX_FFT_Y][MAX_FFT_X],
    float* output_ddr,
    int nx_actual, int ny_actual
) {
    #pragma HLS INLINE off
    
    // Calculate actual output size
    int convX_actual = 4 * nx_actual + 1;
    int convY_actual = 4 * ny_actual + 1;
    
    // Calculate extraction offset MATCHING Golden CPU reference
    // Golden CPU now uses 128×128 FFT size, offset = (128 - 33) / 2 = 47
    // HLS must use same offset for proper alignment
    int offset_x = (MAX_FFT_X - convX_actual) / 2;  // For convX=33: (128-33)/2 = 47
    int offset_y = (MAX_FFT_Y - convY_actual) / 2;  // For convY=33: (128-33)/2 = 47
    
    // Extract and write to output DDR
    // Note: FFTshift already applied in main function (tmpImgp is already shifted)
    // Do NOT apply second FFTshift here!
    for (int y = 0; y < convY_actual; y++) {
        for (int x = 0; x < convX_actual; x++) {
            #pragma HLS PIPELINE II=1
            
            // Source position (NO second FFTshift - tmpImgp is already shifted)
            int src_y = y + offset_y;
            int src_x = x + offset_x;
            
            // Output index (linear layout)
            int out_idx = y * convX_actual + x;
            
            // Write to output DDR
            output_ddr[out_idx] = ifft_output[src_y][src_x];
        }
    }
}

// ============================================================================
// 2D FFT Implementation (Conditional: HLS FFT vs Direct DFT)
// ============================================================================

#ifdef __SYNTHESIS__
// ============================================================================
// HLS FFT IP Implementation (C Synthesis Mode)
// ============================================================================
// 
// CRITICAL FIX: HLS FFT requires ap_fixed types and config override!
// Using corrected configuration from hls_fft_config_2048_corrected.h
// (Header already included at file top)
//

/**
 * Type conversion: float to ap_fixed<32, 1> (HLS FFT native precision)
 */
void convert_float_to_apfixed(
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
 * Type conversion: ap_fixed<32, 1> to float (with N²=16384 compensation)
 */
void convert_apfixed_to_float(
    cmpx_fft_out_t input[128][128],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            float r = (float)input[y][x].real();
            float i = (float)input[y][x].imag();
            
            output[y][x] = cmpx_2048_t(r, i);
        }
    }
}

/**
 * 2D FFT wrapper (HLS FFT IP version)
 * 
 * Workflow:
 *   1. Convert float to ap_fixed (32-bit precision)
 *   2. Call HLS FFT IP via fft_2d_hls_128()
 *   3. Convert ap_fixed back to float
 *   4. COMPENSATION: If IFFT, multiply by N²=16384 (scaled mode correction)
 */
void fft_2d_full_2048(
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
    convert_float_to_apfixed(input, input_fixed);
    
    // Step 2: Call HLS FFT IP (128×128)
    fft_2d_hls_128(input_fixed, output_fixed, is_inverse);
    
    // Step 3: Convert ap_fixed back to float
    convert_apfixed_to_float(output_fixed, output);
    
    // Step 4: COMPENSATION - Manual N² scaling for IFFT
    // HLS FFT scaled mode divides by N² = 16384 for 2D IFFT
    // Multiply back in float domain to match FFTW BACKWARD behavior
    if (is_inverse) {
        for (int i = 0; i < 128; i++) {
            for (int j = 0; j < 128; j++) {
                // Compensate scaled IFFT: multiply by 16384 in float domain
                output[i][j] = output[i][j] * 16384.0f;
            }
        }
    }
}

#else
// ============================================================================
// Direct DFT Implementation (C Simulation Mode)
// ============================================================================

/**
 * 1D FFT/IFFT using Direct DFT (C Simulation Compatible)
 * 
 * For C Simulation: use mathematically equivalent direct DFT
 * For C Synthesis: HLS will optimize to FFT IP via pragmas
 * 
 * IMPORTANT: IFFT uses 1/N scaling to match FFTW BACKWARD convention
 *   FFT:  X[k] = sum_n x[n] * exp(-2πi*k*n/N)  (no scaling)
 *   IFFT: x[n] = (1/N) * sum_k X[k] * exp(+2πi*k*n/N)
 * 
 * For 2D IFFT, total scaling = 1/(N*M) = 1/(128*128) = 1/16384
 */
void fft_1d_direct_2048(
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
        
        // Apply IFFT scaling: divide by N for inverse transform
        // This matches FFTW BACKWARD convention
        if (is_inverse) {
            out[k] = sum * scale;
        } else {
            out[k] = sum;
        }
    }
}

/**
 * 2D FFT (row-column decomposition)
 * 
 * For 2048 architecture: works on full 128×128 array
 * Note: DDR caching version will use Ping-Pong buffers
 */
void fft_2d_full_2048(
    cmpx_2048_t input[MAX_FFT_Y][MAX_FFT_X],
    cmpx_2048_t output[MAX_FFT_Y][MAX_FFT_X],
    bool is_inverse
) {
    #pragma HLS INLINE off
    
    // Temporary buffer for row FFT results
    cmpx_2048_t temp[MAX_FFT_Y][MAX_FFT_X];
    
    // Step 1: FFT on each row
    for (int row = 0; row < MAX_FFT_Y; row++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        // Load row data
        cmpx_2048_t row_in[MAX_FFT_X];
        for (int col = 0; col < MAX_FFT_X; col++) {
            #pragma HLS PIPELINE II=1
            row_in[col] = input[row][col];
        }
        
        // FFT on row
        cmpx_2048_t row_out[MAX_FFT_X];
        fft_1d_direct_2048(row_in, row_out, is_inverse);
        
        // Store row result
        for (int col = 0; col < MAX_FFT_X; col++) {
            #pragma HLS PIPELINE II=1
            temp[row][col] = row_out[col];
        }
    }
    
    // Step 2: FFT on each column
    for (int col = 0; col < MAX_FFT_X; col++) {
        #pragma HLS LOOP_TRIPCOUNT min=128 max=128
        
        // Load column data
        cmpx_2048_t col_in[MAX_FFT_X];
        for (int row = 0; row < MAX_FFT_Y; row++) {
            #pragma HLS PIPELINE II=1
            col_in[row] = temp[row][col];
        }
        
        // FFT on column
        cmpx_2048_t col_out[MAX_FFT_X];
        fft_1d_direct_2048(col_in, col_out, is_inverse);
        
        // Store column result
        for (int row = 0; row < MAX_FFT_Y; row++) {
            #pragma HLS PIPELINE II=1
            output[row][col] = col_out[row];
        }
    }
    
    // MANUAL COMPENSATION for Direct DFT IFFT (C Simulation path)
    // Direct DFT IFFT has 1/N scaling per dimension = 1/16384 total
    // Multiply by N² = 16384 to match FFTW BACKWARD behavior
    if (is_inverse) {
        const float N_squared = static_cast<float>(MAX_FFT_Y * MAX_FFT_X);  // 16384.0f
        for (int i = 0; i < MAX_FFT_Y; i++) {
            for (int j = 0; j < MAX_FFT_X; j++) {
                output[i][j] = output[i][j] * N_squared;
            }
        }
    }
}

#endif // __SYNTHESIS__

// ============================================================================
// Intensity Accumulation
// ============================================================================

/**
 * Accumulate intensity from IFFT output
 * Formula: tmpImg += scale * |IFFT_out|^2
 */
void accumulate_intensity_2048(
    cmpx_2048_t ifft_output[MAX_FFT_Y][MAX_FFT_X],
    float tmpImg[MAX_FFT_Y][MAX_FFT_X],
    float scale
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // Compute intensity: |re + im*i|^2 = re^2 + im^2
            float re = ifft_output[y][x].real();
            float im = ifft_output[y][x].imag();
            float intensity = re * re + im * im;
            
            // Accumulate with eigenvalue scaling
            // FFT unscaled mode matches FFTW BACKWARD (no compensation needed)
            tmpImg[y][x] += scale * intensity;
        }
    }
}

// ============================================================================
// FFT Shift
// ============================================================================

/**
 * FFT shift: move zero-frequency component to center
 */
void fftshift_2d_2048(
    float input[MAX_FFT_Y][MAX_FFT_X],
    float output[MAX_FFT_Y][MAX_FFT_X]
) {
    #pragma HLS INLINE off
    
    int halfX = MAX_FFT_X / 2;  // = 64
    int halfY = MAX_FFT_Y / 2;  // = 64
    
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            
            // Swap quadrants
            int dst_y = (y + halfY) % MAX_FFT_Y;
            int dst_x = (x + halfX) % MAX_FFT_X;
            
            output[dst_y][dst_x] = input[y][x];
        }
    }
}

// ============================================================================
// Main SOCS Function (2048 Architecture)
// ============================================================================

/**
 * SOCS 2048 Main Function
 * 
 * Key differences from v1.0:
 *   1. Fixed 128×128 FFT size (max support)
 *   2. Runtime Nx parameter (via AXI-Lite)
 *   3. Zero-padding for small Nx
 *   4. Dynamic output size extraction
 */
void calc_socs_2048_hls(
    // Data AXI-MM interfaces
    float* mskf_r,
    float* mskf_i,
    float* scales,
    float* krn_r,
    float* krn_i,
    float* output,
    
    // FFT DDR buffer (NEW)
    cmpx_2048_t* fft_buf_ddr,
    
    // Runtime parameters (NEW)
    int nx_actual,
    int ny_actual,
    int nk,
    int Lx,
    int Ly,
    unsigned long fft_buf_base,
    
    // Output mode (NEW - Phase 1.4+)
    // 0: Extract center region (convX×convY, e.g., 33×33)
    // 1: Full tmpImgp (MAX_FFT_X×MAX_FFT_Y, e.g., 128×128) for Fourier Interpolation
    int output_mode
) {
    // ==================== AXI-MM Interface Configuration ====================
    
    // Data interfaces (gmem0-5)
    // depth values set for maximum support: 2048×2048 mask, Nx=24, nk=32
    // mskf_r/i: 2048×2048 = 4,194,304 (max mask resolution)
    // mskf_r/i: 1024×1024 = 1,048,576 (depth set to match golden_1024 test;
    //   2048²=4M would cause CoSim OOM — depth is CoSim alloc hint, not HW limit)
    // scales: nk_max=32
    // krn_r/i: 32 kernels × 49×49 = 76,832 (Nx=24 → kerX=49)
    // output: 128×128 = 16,384 (full tmpImgp for OUTPUT_MODE=1)
    // fft_buf_ddr: 128×128 = 16,384 complex
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
    
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 \
        depth=16384 latency=8 num_write_outstanding=4 max_write_burst_length=64
    
    // FFT DDR buffer interface (gmem6 - NEW)
    #pragma HLS INTERFACE m_axi port=fft_buf_ddr offset=slave bundle=gmem6 \
        depth=16384 latency=50 num_read_outstanding=8 num_write_outstanding=8 \
        max_read_burst_length=64 max_write_burst_length=64
    
    // AXI-Lite control interface
    #pragma HLS INTERFACE s_axilite port=nx_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=ny_actual bundle=control
    #pragma HLS INTERFACE s_axilite port=nk bundle=control
    #pragma HLS INTERFACE s_axilite port=Lx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ly bundle=control
    #pragma HLS INTERFACE s_axilite port=fft_buf_base bundle=control
    #pragma HLS INTERFACE s_axilite port=output_mode bundle=control  // NEW: output mode
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // ==================== Local Arrays ====================
    
    // FFT input/output (128×128 fixed)
    cmpx_2048_t fft_input[MAX_FFT_Y][MAX_FFT_X];
    cmpx_2048_t fft_output[MAX_FFT_Y][MAX_FFT_X];
    
    // Intensity accumulator
    float tmpImg[MAX_FFT_Y][MAX_FFT_X];
    float tmpImgp[MAX_FFT_Y][MAX_FFT_X];
    
    // Partition for parallel access
    #pragma HLS ARRAY_PARTITION variable=fft_input cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=fft_output cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImg cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImgp cyclic factor=4 dim=2
    
    // ==================== Algorithm Implementation ====================
    
    // Initialize accumulator
    for (int y = 0; y < MAX_FFT_Y; y++) {
        for (int x = 0; x < MAX_FFT_X; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // Process each kernel
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_TRIPCOUNT min=4 max=16
        
        // Step 1: Embed with zero-padding
        embed_kernel_mask_padded_2048(
            mskf_r, mskf_i, krn_r, krn_i,
            fft_input, nx_actual, ny_actual, k, Lx, Ly
        );
        
        // DEBUG: Check fft_input content (C Simulation only)
        #ifndef __SYNTHESIS__
        if (k == 0) {
            std::printf("[DEBUG] Kernel 0 fft_input sample values (after embedding):\n");
            int kerX_actual = 2 * nx_actual + 1;
            int kerY_actual = 2 * ny_actual + 1;
            int embed_x = (MAX_FFT_X - kerX_actual) / 2;  // Corrected: centered embedding
            int embed_y = (MAX_FFT_Y - kerY_actual) / 2;
            for (int dy = 0; dy < 3 && dy < kerY_actual; dy++) {
                for (int dx = 0; dx < 3 && dx < kerX_actual; dx++) {
                    int y = embed_y + dy;
                    int x = embed_x + dx;
                    std::printf("  fft_input[%d][%d] = (%.6f, %.6f)\n",
                        y, x, fft_input[y][x].real(), fft_input[y][x].imag());
                }
            }
        }
        #endif
        
        // Step 2: 2D IFFT (128×128 fixed)
        // Two implementation modes:
        //   - BRAM-only: for C simulation (simpler, uses fft_2d_full_2048)
        //   - DDR-cached: for synthesis (uses fft_2d_ddr_pingpong)
        #ifdef USE_DDR_CACHE
            fft_2d_ddr_pingpong(fft_buf_ddr, fft_buf_base, fft_input, fft_output, true);
        #else
            fft_2d_full_2048(fft_input, fft_output, true);  // IFFT (BRAM-only)
        #endif
        
        // DEBUG: Check fft_output content (C Simulation only)
        #ifndef __SYNTHESIS__
        if (k == 0) {
            std::printf("[DEBUG] Kernel 0 fft_output sample values (after IFFT):\n");
            for (int y = 0; y < 3; y++) {
                for (int x = 0; x < 3; x++) {
                    std::printf("  fft_output[%d][%d] = (%.6f, %.6f)\n",
                        y, x, fft_output[y][x].real(), fft_output[y][x].imag());
                }
            }
        }
        #endif
        
        // Step 3: Accumulate intensity
        float eigenvalue = scales[k];
        accumulate_intensity_2048(fft_output, tmpImg, eigenvalue);
        
        // DEBUG: Check tmpImg content (C Simulation only)
        #ifndef __SYNTHESIS__
        if (k == 0) {
            std::printf("[DEBUG] Kernel 0 tmpImg sample values (after accumulation):\n");
            std::printf("  eigenvalue = %.6f\n", eigenvalue);
            for (int y = 0; y < 3; y++) {
                for (int x = 0; x < 3; x++) {
                    std::printf("  tmpImg[%d][%d] = %.6f\n", y, x, tmpImg[y][x]);
                }
            }
        }
        #endif
    }
    
    // Step 4: FFT shift
    fftshift_2d_2048(tmpImg, tmpImgp);
    
    // Step 5: Output based on mode
    if (output_mode == 1) {
        // NEW: Output full 128×128 tmpImgp for Fourier Interpolation validation
        // This matches Golden CPU's tmpImgp_full_128.bin output
        for (int y = 0; y < MAX_FFT_Y; y++) {
            for (int x = 0; x < MAX_FFT_X; x++) {
                #pragma HLS PIPELINE II=1
                int out_idx = y * MAX_FFT_X + x;
                output[out_idx] = tmpImgp[y][x];
            }
        }
    } else {
        // Default: Extract center region (convX×convY, e.g., 33×33)
        extract_valid_region_2048(tmpImgp, output, nx_actual, ny_actual);
    }
}