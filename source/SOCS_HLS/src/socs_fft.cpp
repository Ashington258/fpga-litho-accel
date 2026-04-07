/**
 * SOCS HLS FFT Implementation
 * FPGA-Litho Project
 * 
 * Implements 32-point FFT/IFFT and 2D IFFT using hls::fft IP (array-based API)
 */

#include "socs_fft.h"

// For debug NaN/Inf check (C Simulation only)
#ifndef __SYNTHESIS__
#include <cmath>  // for std::abs
#endif

// ============================================================================
// 2D IFFT Implementation (Row-Column Decomposition using array-based FFT)
// ============================================================================

void ifft_2d_32x32(
    cmpxData_t in_matrix[fftConvY][fftConvX],
    cmpxData_t out_matrix[fftConvY][fftConvX]
) {
    // Temporary buffers for row FFT and transpose
    cmpxData_t row_fft_out[fftConvY][fftConvX];
    cmpxData_t transposed[fftConvX][fftConvY];
    cmpxData_t col_fft_out[fftConvX][fftConvY];
    
    #pragma HLS ARRAY_PARTITION variable=row_fft_out cyclic factor=4 dim=2
    #pragma HLS ARRAY_PARTITION variable=transposed cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=col_fft_out cyclic factor=4 dim=2
    
    // FFT buffers (array-based API requires explicit arrays)
    cmpxData_t fft_in[FFT_LENGTH];
    cmpxData_t fft_out[FFT_LENGTH];
    
    #pragma HLS ARRAY_PARTITION variable=fft_in cyclic factor=4
    #pragma HLS ARRAY_PARTITION variable=fft_out cyclic factor=4
    
    // FFT config and status
    hls::ip_fft::config_t<fft_config_32> fft_config;
    hls::ip_fft::status_t<fft_config_32> fft_status;
    
    // Step 1: Row-wise IFFT (32 rows, each 32 points)
    for (int row = 0; row < fftConvY; row++) {
        // Load row data into FFT input array
        for (int col = 0; col < fftConvX; col++) {
            #pragma HLS PIPELINE II=1
            fft_in[col] = in_matrix[row][col];
        }
        
        // Configure FFT for IFFT
        fft_config.setDir(1);  // 1 = inverse (IFFT)
        // Unscaled mode: no scaling schedule needed
        
        // DEBUG: Check for NaN/Inf in input (C Simulation only)
        #ifndef __SYNTHESIS__
        bool has_nan_inf = false;
        for (int col = 0; col < fftConvX; col++) {
            if (std::isnan(fft_in[col].real()) || std::isnan(fft_in[col].imag()) ||
                std::isinf(fft_in[col].real()) || std::isinf(fft_in[col].imag())) {
                has_nan_inf = true;
                std::cout << "  ❌ NaN/Inf in row FFT input[" << row << "][" << col << "]: " 
                          << fft_in[col] << std::endl;
            }
        }
        if (has_nan_inf) {
            std::cout << "  WARNING: Row " << row << " has NaN/Inf input - FFT will produce invalid output" << std::endl;
        }
        #endif
        
        // Perform 1D IFFT on this row (array-based API)
        hls::fft<fft_config_32>(fft_in, fft_out, &fft_status, &fft_config);
        
        // Read result back to row buffer
        for (int col = 0; col < fftConvX; col++) {
            #pragma HLS PIPELINE II=1
            row_fft_out[row][col] = fft_out[col];
        }
    }
    
    // Step 2: Transpose matrix
    transpose_32x32(row_fft_out, transposed);
    
    // Step 3: Column-wise IFFT (now rows after transpose)
    for (int row = 0; row < fftConvX; row++) {
        // Load "column" data (now a row after transpose) into FFT array
        for (int col = 0; col < fftConvY; col++) {
            #pragma HLS PIPELINE II=1
            fft_in[col] = transposed[row][col];
        }
        
        // Configure FFT for IFFT
        fft_config.setDir(1);  // 1 = inverse (IFFT)
        // Unscaled mode: no scaling schedule needed
        
        // Perform 1D IFFT on this column (array-based API)
        hls::fft<fft_config_32>(fft_in, fft_out, &fft_status, &fft_config);
        
        // Read result back
        for (int col = 0; col < fftConvY; col++) {
            #pragma HLS PIPELINE II=1
            col_fft_out[row][col] = fft_out[col];
        }
    }
    
    // Step 4: Transpose back to restore original layout
    transpose_32x32(col_fft_out, out_matrix);
}

// ============================================================================
// Matrix Transpose
// ============================================================================

void transpose_32x32(
    cmpxData_t in[fftConvY][fftConvX],
    cmpxData_t out[fftConvX][fftConvY]
) {
    #pragma HLS INLINE off
    
    for (int i = 0; i < fftConvY; i++) {
        for (int j = 0; j < fftConvX; j++) {
            #pragma HLS PIPELINE
            out[j][i] = in[i][j];
        }
    }
}

// ============================================================================
// Build Padded IFFT Input (Kernel × Mask Product Embedding)
// ============================================================================
// IMPORTANT: Uses "bottom-right" embedding to match litho.cpp Option B behavior
// litho.cpp: by = difY + ky, bx = difX + kx (difY = fftConvY - kerY)
// This places kernel at indices (difY, difX) to (fftConvY-1, fftConvX-1)
// ============================================================================

void build_padded_ifft_input_32(
    float* mskf_r,
    float* mskf_i,
    float* krn_r,
    float* krn_i,
    cmpxData_t padded[fftConvY][fftConvX],
    int Lxh, int Lyh
) {
    // Calculate embedding offset (BOTTOM-RIGHT padding, matching litho.cpp)
    // 32 - 9 = 23 (difY, difX)
    // Kernel is placed at indices (23,23) to (31,31)
    int difX = fftConvX - kerX;  // 23 for 32-9
    int difY = fftConvY - kerY;  // 23 for 32-9
    
    // Initialize padded array to zero
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE
            padded[y][x] = cmpxData_t(0.0f, 0.0f);
        }
    }
    
    // Embed kernel × mask product at BOTTOM-RIGHT corner
    // Kernel indices: -Ny to Ny, -Nx to Nx (stored as ky=0..kerY-1)
    // Mask indices: centered at (Lxh, Lyh)
    for (int ky = 0; ky < kerY; ky++) {
        for (int kx = 0; kx < kerX; kx++) {
            #pragma HLS PIPELINE
            
            // Convert kernel index to physical coordinates
            int phys_y = ky - Ny;  // -Ny to Ny
            int phys_x = kx - Nx;  // -Nx to Nx
            
            // Mask spectrum index (centered at Lxh, Lyh)
            int mask_y = Lyh + phys_y;
            int mask_x = Lxh + phys_x;
            
            // Bounds check (using passed Lxh, Lyh parameters, not global macros)
            // mskf array is 2*Lxh × 2*Lyh (512×512 when Lxh=256)
            int Lx_full = 2 * Lxh;  // 512
            int Ly_full = 2 * Lyh;  // 512
            
            if (mask_y >= 0 && mask_y < Ly_full && mask_x >= 0 && mask_x < Lx_full) {
                // Read kernel value
                float kr_r = krn_r[ky * kerX + kx];
                float kr_i = krn_i[ky * kerX + kx];
                
                // Read mask spectrum value
                int mask_idx = mask_y * Lx_full + mask_x;
                float ms_r = mskf_r[mask_idx];
                float ms_i = mskf_i[mask_idx];
                
                // Complex multiplication: kernel × mask
                float prod_r = kr_r * ms_r - kr_i * ms_i;
                float prod_i = kr_r * ms_i + kr_i * ms_r;
                
                // DEBUG: Check for NaN/Inf before denormal flush (C Simulation only)
                #ifndef __SYNTHESIS__
                if (std::isnan(prod_r) || std::isnan(prod_i) || std::isinf(prod_r) || std::isinf(prod_i)) {
                    std::cout << "[DEBUG] NaN/Inf in prod BEFORE scaling: ky=" << ky << " kx=" << kx 
                              << ", kr=(" << kr_r << "," << kr_i << ")"
                              << ", ms=(" << ms_r << "," << ms_i << ")"
                              << ", prod=(" << prod_r << "," << prod_i << ")" << std::endl;
                }
                #endif
                
                // CRITICAL: Pre-scale input to avoid HLS FFT numerical instability
                // HLS FFT may produce NaN when processing very small values (< 1e-35)
                // Solution: Scale up by INPUT_SCALE (1,000,000), then adjust compensation later
                prod_r *= INPUT_SCALE;
                prod_i *= INPUT_SCALE;
                
                // CRITICAL: Flush denormal numbers to zero (HLS FFT requirement)
                // Denormal numbers (< 1e-38) cause HLS FFT to produce NaN/Inf warnings
                // This is standard practice for numerical stability in FFT implementations
                const float denormal_thresh = 1e-30f;  // Use 1e-30f threshold (safe margin)
                float abs_r = std::abs(prod_r);
                float abs_i = std::abs(prod_i);
                if (abs_r < denormal_thresh) prod_r = 0.0f;
                if (abs_i < denormal_thresh) prod_i = 0.0f;
                
                // DEBUG: Check for NaN/Inf after scaling (C Simulation only)
                #ifndef __SYNTHESIS__
                if (std::isnan(prod_r) || std::isnan(prod_i) || std::isinf(prod_r) || std::isinf(prod_i)) {
                    std::cout << "[DEBUG] NaN/Inf in prod AFTER scaling: ky=" << ky << " kx=" << kx 
                              << ", prod=(" << prod_r << "," << prod_i << ")" << std::endl;
                }
                
                // Log denormal flush events
                if (abs_r < denormal_thresh || abs_i < denormal_thresh) {
                }
                #endif
                
                // Write to padded array (BOTTOM-RIGHT embedding, matching litho.cpp)
                int pad_y = difY + ky;  // Matches litho.cpp: by = difY + ky
                int pad_x = difX + kx;  // Matches litho.cpp: bx = difX + kx
                
                padded[pad_y][pad_x] = cmpxData_t(prod_r, prod_i);
            }
        }
    }
}

// ============================================================================
// Extract Valid 17×17 Region from 32×32 (after fftshift)
// ============================================================================
// IMPORTANT: This must be done AFTER fftshift on the full 32×32 array
// litho.cpp: myShift(tmpImg, tmpImgp, fftConvX, fftConvY, true, true)
// then extract center region
// ============================================================================

void extract_center_17x17_from_32(
    float in_32[fftConvY][fftConvX],
    float out_17[convY][convX]
) {
    // Calculate extraction offset (center crop from 32×32)
    // 32 - 17 = 15, 15 / 2 = 7
    // Extract indices (7,7) to (23,23) from 32×32
    int offX = (fftConvX - convX) / 2;  // = 7
    int offY = (fftConvY - convY) / 2;  // = 7
    
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE
            out_17[y][x] = in_32[offY + y][offX + x];
        }
    }
}

// ============================================================================
// Intensity Accumulation on 32×32 Array (before fftshift)
// ============================================================================
// IMPORTANT: Accumulate intensity on full 32×32 array before any extraction
// litho.cpp: for each kernel, compute intensity on full bwdDataOut array
// ============================================================================

void accumulate_intensity_32x32(
    cmpxData_t ifft_out[fftConvY][fftConvX],
    float accum[fftConvY][fftConvX],
    float scale
) {
    // CRITICAL: Apply FFT scaling compensation
    // HLS FFT scaled mode divides 2D IFFT output by 1024
    // Intensity = |IFFT|^2, so compensation factor is 1024^2 = 1048576
    // This matches numpy/FFTW unscaled IFFT behavior
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE
            
            float re = ifft_out[y][x].real();
            float im = ifft_out[y][x].imag();
            
            // Intensity with FFT scaling compensation
            float intensity = scale * (re * re + im * im) * FFT_SCALING_COMPENSATION;
            
            accum[y][x] += intensity;
        }
    }
}

// ============================================================================
// FFTShift for 32×32 Array (matching litho.cpp myShift)
// ============================================================================
// IMPORTANT: litho.cpp uses shiftTypeX=true, shiftTypeY=true
// xh = sizeX / 2 = 16, yh = sizeY / 2 = 16
// Swap quadrants: out[sy * sizeX + sx] = in[y * sizeX + x]
// where sx = (x + xh) % sizeX, sy = (y + yh) % sizeY
// ============================================================================

void fftshift_32x32(
    float in[fftConvY][fftConvX],
    float out[fftConvY][fftConvX]
) {
    // For 32×32 (power-of-two), fftshift swaps quadrants
    // halfX = 16, halfY = 16
    
    const int halfX = fftConvX / 2;  // = 16
    const int halfY = fftConvY / 2;  // = 16
    
    // Quadrant swap pattern (standard fftshift):
    // Each pixel moves to opposite quadrant
    // (x, y) -> ((x + 16) % 32, (y + 16) % 32)
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE
            
            // Calculate destination coordinates (swap)
            int dst_y = (y + halfY) % fftConvY;
            int dst_x = (x + halfX) % fftConvX;
            
            // Write to swapped position
            // Note: litho.cpp writes out[sy * sizeX + sx] = in[y * sizeX + x]
            // This means destination indices are (sy, sx), source is (y, x)
            out[dst_y][dst_x] = in[y][x];
        }
    }
}