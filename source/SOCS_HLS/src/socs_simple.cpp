/**
 * SOCS HLS Implementation - Complete Algorithm
 * FPGA-Litho Project
 * 
 * Implements the SOCS (Sum of Coherent Systems) aerial image computation
 * Current configuration: Nx=4, IFFT 32×32, Output 17×17
 * 
 * Algorithm:
 *   I(x,y) = sum_k lambda_k * |IFFT(K_k * M)|^2
 *   where lambda_k are eigenvalues, K_k are SOCS kernels, M is mask spectrum
 */

#include <hls_stream.h>
#include <ap_fixed.h>
#include <hls_fft.h>
#include <cmath>
#include <complex>

// ============================================================================
// Configuration Parameters (Nx=4, based on current config)
// ============================================================================
#define Lx 512
#define Ly 512
#define Nx 4
#define Ny 4
#define convX (4*Nx + 1)   // = 17 (physical convolution output)
#define convY (4*Ny + 1)   // = 17
#define fftConvX 32        // nextPowerOfTwo(17) - FFT grid size
#define fftConvY 32
#define kerX (2*Nx + 1)    // = 9 (kernel support)
#define kerY (2*Ny + 1)    // = 9
#define nk 10              // number of kernels

// ============================================================================
// FFT Configuration for HLS (following Vitis HLS 2025.2 API pattern)
// ============================================================================
// FFT parameters must be global const (not struct static members)
// Reference: reference/vitis_hls_ftt的实现/interface_stream/fft_top.h

const int  FFT_LENGTH                          = 32;   // 32-point transform (matching fftConvX)
const char FFT_NFFT_MAX                        = 5;    // log2(32) = 5
const bool FFT_HAS_NFFT                        = 0;    // Fixed length (no runtime config)
const bool FFT_USE_FLT_PT                      = 1;    // Floating-point mode
const char FFT_INPUT_WIDTH                     = 32;   // Float = 32 bits
const char FFT_OUTPUT_WIDTH                    = 32;   // Float = 32 bits
const char FFT_TWIDDLE_WIDTH                   = 24;   // Twiddle factor width
const char FFT_CHANNELS                        = 1;    // Single channel
const hls::ip_fft::arch FFT_ARCH               = hls::ip_fft::pipelined_streaming_io;
const hls::ip_fft::ordering FFT_OUTPUT_ORDER   = hls::ip_fft::natural_order;
const hls::ip_fft::scaling FFT_SCALING         = hls::ip_fft::scaled;
const hls::ip_fft::rounding FFT_ROUNDING       = hls::ip_fft::truncation;
const bool FFT_OVFLO                           = 1;
const bool FFT_CYCLIC_PREFIX_INSERTION         = 0;
const bool FFT_XK_INDEX                        = 0;

// Not configurable (use defaults)
const hls::ip_fft::mem FFT_MEM_DATA            = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS   = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_REORDER         = hls::ip_fft::block_ram;
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE   = hls::ip_fft::use_luts;
const hls::ip_fft::opt FFT_BUTTERLY_TYPE       = hls::ip_fft::use_luts;

// Data types - using float for accuracy
typedef float data_t;
typedef std::complex<float> cmpx_t;

// FFT config struct - minimal configuration matching library API
// Only output_ordering needs to be specified in struct (Vitis HLS 2025.2 style)
struct config1 : hls::ip_fft::params_t {
    static const unsigned output_ordering = hls::ip_fft::natural_order;
};

typedef hls::ip_fft::config_t<config1> config_t;
typedef hls::ip_fft::status_t<config1> status_t;

// ============================================================================
// FFT Helper Functions
// ============================================================================

/**
 * 1D FFT/IFFT on a stream
 * 
 * Vitis HLS 2025.2 simplified streaming API (from hls_fft.h line 2723):
 *   hls::fft<CONFIG_T>(in_stream, out_stream, fwd_inv, scale_sch, cp_len, ovflo, blk_exp)
 * 
 * fwd_inv:    bool - false=FFT (forward), true=IFFT (backward)
 * scale_sch:  int - scaling factor (-1 for default/no scaling)
 * cp_len:     int - cyclic prefix length (-1 for none)
 * ovflo:      bool* - overflow status output
 * blk_exp:    unsigned* - block exponent output
 */
void fft_1d_stream(
    hls::stream<cmpx_t>& in_stream,
    hls::stream<cmpx_t>& out_stream,
    bool is_inverse       // false=forward (FFT), true=backward (IFFT)
) {
    #pragma HLS INLINE off
    
    // Overflow status output (bool* for simplified streaming API)
    bool ovflo_status = false;
    
    // Call FFT using simplified streaming API (hls_fft.h line 2723):
    // fwd_inv = is_inverse (bool)
    // scale_sch = -1 (no scaling, use default)
    // cp_len = -1 (no cyclic prefix)
    // ovflo = overflow status pointer
    // blk_exp = nullptr (no block exponent needed)
    hls::fft<config1>(in_stream, out_stream, is_inverse, -1, -1, &ovflo_status);
}

/**
 * Row-wise FFT on 2D array (streaming)
 * Process each row through 1D FFT
 */
void fft_2d_rows(
    cmpx_t input[fftConvY][fftConvX],
    cmpx_t output[fftConvY][fftConvX],
    ap_uint<1> dir,         // 0=FFT, 1=IFFT
    ap_uint<15> scaling     // scaling schedule
) {
    // For HLS C Simulation, use a simple DFT implementation
    // This allows functional verification without complex stream architecture
    // For synthesis, this will be replaced with streaming FFT IP
    
    for (int row = 0; row < fftConvY; row++) {
        // Compute DFT/IDFT for each row using direct formula
        // DFT: X[k] = sum_n x[n] * exp(-2*pi*i*n*k/N)
        // IDFT (FFTW BACKWARD): x[n] = sum_k X[k] * exp(+2*pi*i*n*k/N)
        // NOTE: FFTW BACKWARD does NOT apply 1/N scaling (matching litho.cpp)
        
        float sign = (dir == 1) ? +1.0f : -1.0f;  // IDFT: +, FFT: -
        // NO extra scaling for IDFT - FFTW BACKWARD convention
        
        for (int k = 0; k < fftConvX; k++) {
            #pragma HLS PIPELINE II=1
            
            float sum_r = 0.0f;
            float sum_i = 0.0f;
            
            for (int n = 0; n < fftConvX; n++) {
                // Complex multiplication: input[n] * exp(sign * 2*pi*i*n*k/N)
                float angle = sign * 2.0f * M_PI * n * k / FFT_LENGTH;
                float cos_val = cos(angle);
                float sin_val = sin(angle);
                
                // input[n] * (cos + i*sin)
                float in_r = input[row][n].real();
                float in_i = input[row][n].imag();
                
                sum_r += in_r * cos_val - in_i * sin_val;
                sum_i += in_r * sin_val + in_i * cos_val;
            }
            
            // No extra normalization (matching FFTW BACKWARD behavior)
            output[row][k] = cmpx_t(sum_r, sum_i);
        }
    }
}

/**
 * Transpose 2D array (required for row-column FFT decomposition)
 */
void transpose_2d(
    cmpx_t input[fftConvY][fftConvX],
    cmpx_t output[fftConvX][fftConvY]
) {
    #pragma HLS PIPELINE II=1
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            output[x][y] = input[y][x];
        }
    }
}

/**
 * Full 2D FFT/IFFT using row-column decomposition
 * Order: Row FFT → Transpose → Column FFT → Transpose
 */
void fft_2d(
    cmpx_t input[fftConvY][fftConvX],
    cmpx_t output[fftConvY][fftConvX],
    ap_uint<1> dir,          // 0=FFT (forward), 1=IFFT (backward)
    ap_uint<15> scaling      // scaling schedule
) {
    #pragma HLS DATAFLOW
    
    // Intermediate buffers
    cmpx_t temp1[fftConvY][fftConvX];
    cmpx_t temp2[fftConvX][fftConvY];
    cmpx_t temp3[fftConvX][fftConvY];
    
    #pragma HLS ARRAY_PARTITION variable=temp1 cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=2 dim=1
    #pragma HLS ARRAY_PARTITION variable=temp3 cyclic factor=2 dim=1
    #pragma HLS RESOURCE variable=temp1 core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=temp2 core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=temp3 core=RAM_2P_BRAM
    
    // Step 1: Row FFT
    fft_2d_rows(input, temp1, dir, scaling);
    
    // Step 2: Transpose (rows → columns)
    transpose_2d(temp1, temp2);
    
    // Step 3: Column FFT (now treating columns as "rows" in temp2)
    fft_2d_rows(temp2, temp3, dir, scaling);
    
    // Step 4: Transpose back
    transpose_2d(temp3, output);
}

// ============================================================================
// SOCS Kernel Functions
// ============================================================================

/**
 * Embed kernel * mask product into padded FFT array
 * 
 * Layout follows litho.cpp "bottom-right" embedding:
 *   difX = fftConvX - kerX = 32 - 9 = 23
 *   difY = fftConvY - kerY = 32 - 9 = 23
 *   Kernel is placed at [difY:difY+kerY-1, difX:difX+kerX-1]
 * 
 * Mask spectrum indices: centered at (Lx/2, Ly/2) = (256, 256)
 *   For offset (x,y) where x,y ∈ [-Nx:Nx], index is (256+x, 256+y)
 */
void embed_kernel_mask_product(
    float* mskf_r,          // Mask spectrum real (Lx×Ly)
    float* mskf_i,          // Mask spectrum imaginary
    float* krn_r,           // Kernel real for current k (kerX×kerY)
    float* krn_i,           // Kernel imaginary
    cmpx_t fft_input[fftConvY][fftConvX],  // Output: padded FFT input
    int kernel_idx,         // Current kernel index (for AXI offset)
    int Lxh,                // Lx/2 = 256
    int Lyh                 // Ly/2 = 256
) {
    #pragma HLS INLINE off
    
    // Offsets for bottom-right embedding (matching litho.cpp)
    int difX = fftConvX - kerX;  // = 23
    int difY = fftConvY - kerY;  // = 23
    
    // Clear FFT input array (zero-padding)
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            fft_input[y][x] = cmpx_t(0.0f, 0.0f);
        }
    }
    
    // Compute kernel * mask product and embed
    // Kernel indices: ky ∈ [0:kerY-1], kx ∈ [0:kerX-1]
    // Corresponding to y ∈ [-Ny:Ny], x ∈ [-Nx:Nx]
    // Mask indices: my = Lyh + y, mx = Lxh + x
    
    int kernel_offset = kernel_idx * kerX * kerY;
    
    for (int ky = 0; ky < kerY; ky++) {
        for (int kx = 0; kx < kerX; kx++) {
            #pragma HLS PIPELINE II=1
            
            // Convert kernel index to offset
            int y_offset = ky - Ny;  // y ∈ [-Ny:Ny]
            int x_offset = kx - Nx;  // x ∈ [-Nx:Nx]
            
            // FFT array position (bottom-right embedding)
            int fft_y = difY + ky;
            int fft_x = difX + kx;
            
            // Mask spectrum position (centered at Lxh/Lyh)
            int mask_y = Lyh + y_offset;
            int mask_x = Lxh + x_offset;
            
            // Bounds check
            if (fft_y >= 0 && fft_y < fftConvY &&
                fft_x >= 0 && fft_x < fftConvX &&
                mask_y >= 0 && mask_y < Ly &&
                mask_x >= 0 && mask_x < Lx) {
                
                // Read kernel value
                float kr_r = krn_r[kernel_offset + ky * kerX + kx];
                float kr_i = krn_i[kernel_offset + ky * kerX + kx];
                cmpx_t kernel_val(kr_r, kr_i);
                
                // Read mask spectrum value
                float ms_r = mskf_r[mask_y * Lx + mask_x];
                float ms_i = mskf_i[mask_y * Lx + mask_x];
                cmpx_t mask_val(ms_r, ms_i);
                
                // Compute product: K_k * M
                cmpx_t product = kernel_val * mask_val;
                
                // Embed into FFT input
                fft_input[fft_y][fft_x] = product;
            }
        }
    }
}

/**
 * Accumulate intensity from IFFT output
 * Formula: tmpImg[i] += scales[k] * |IFFT_out[i]|^2
 * 
 * FFT Scaling Convention (CRITICAL):
 *   - calcSOCS uses FFTW Complex-to-Complex BACKWARD (fftw_plan_dft_2d)
 *   - FFTW BACKWARD does NOT multiply by N (unscaled mode)
 *   - HLS FFT output matches FFTW BACKWARD behavior
 *   - NO compensation needed - direct intensity calculation
 */
void accumulate_intensity(
    cmpx_t ifft_output[fftConvY][fftConvX],
    float tmpImg[fftConvY][fftConvX],    // Accumulator
    float scale                            // Eigenvalue for current kernel
) {
    #pragma HLS INLINE off
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            
            // Compute intensity: |re + im*i|^2 = re^2 + im^2
            float re = ifft_output[y][x].real();
            float im = ifft_output[y][x].imag();
            float intensity = re * re + im * im;
            
            // Accumulate with eigenvalue scaling
            // NO FFT compensation - FFTW BACKWARD produces raw unscaled output
            tmpImg[y][x] += scale * intensity;
        }
    }
}

/**
 * FFT shift: move zero-frequency component to center
 * Equivalent to swapping quadrants
 */
void fftshift_2d(
    float input[fftConvY][fftConvX],
    float output[fftConvY][fftConvX]
) {
    #pragma HLS INLINE off
    
    int halfX = fftConvX / 2;  // = 16
    int halfY = fftConvY / 2;  // = 16
    
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            
            int src_y = y;
            int src_x = x;
            int dst_y = (y + halfY) % fftConvY;
            int dst_x = (x + halfX) % fftConvX;
            
            output[dst_y][dst_x] = input[src_y][src_x];
        }
    }
}

/**
 * Extract center region from shifted intensity map
 * Input: fftConvX×fftConvY (32×32)
 * Output: convX×convY (17×17)
 */
void extract_center_region(
    float tmpImgp[fftConvY][fftConvX],
    float output[convY][convX]
) {
    #pragma HLS INLINE off
    
    int offsetX = (fftConvX - convX) / 2;  // = (32-17)/2 = 7
    int offsetY = (fftConvY - convY) / 2;  // = 7
    
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE II=1
            
            int src_y = offsetY + y;
            int src_x = offsetX + x;
            
            output[y][x] = tmpImgp[src_y][src_x];
        }
    }
}

// ============================================================================
// Main SOCS Function
// ============================================================================

/**
 * SOCS Aerial Image Computation
 * 
 * Inputs (via AXI-MM):
 *   - mskf_r, mskf_i: Mask spectrum (Lx×Ly = 512×512)
 *   - scales: Eigenvalues (nk = 10)
 *   - krn_r, krn_i: SOCS kernels (nk × kerX × kerY = 10×9×9)
 * 
 * Output (via AXI-MM):
 *   - output: tmpImgp (convX×convY = 17×17)
 */
void calc_socs_simple_hls(
    float *mskf_r,      // AXI-MM: Mask spectrum real (512×512)
    float *mskf_i,      // AXI-MM: Mask spectrum imaginary
    float *scales,      // AXI-MM: Eigenvalues (10)
    float *krn_r,       // AXI-MM: Kernels real (10×9×9)
    float *krn_i,       // AXI-MM: Kernels imaginary
    float *output       // AXI-MM: Output tmpImgp (17×17)
) {
    // ==================== AXI-MM Interface Configuration ====================
    // Each interface has independent bundle for separate AXI Master ports
    
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 \
        depth=262144 latency=32 num_read_outstanding=8 max_read_burst_length=64
    
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 \
        depth=10 latency=16 num_read_outstanding=2 max_read_burst_length=4
    
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 \
        depth=810 latency=16 num_read_outstanding=4 max_read_burst_length=16
    
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 \
        depth=810 latency=16 num_read_outstanding=4 max_read_burst_length=16
    
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 \
        depth=289 latency=8 num_write_outstanding=2 max_write_burst_length=8
    
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // ==================== Local Arrays ====================
    // FFT input/output buffers
    cmpx_t fft_input[fftConvY][fftConvX];
    cmpx_t fft_output[fftConvY][fftConvX];
    
    // Intensity accumulator
    float tmpImg[fftConvY][fftConvX];
    float tmpImgp[fftConvY][fftConvX];
    
    // Output buffer
    float output_local[convY][convX];
    
    // Partition arrays for parallel access
    #pragma HLS ARRAY_PARTITION variable=fft_input cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=fft_output cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImg cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=tmpImgp cyclic factor=2 dim=2
    #pragma HLS ARRAY_PARTITION variable=output_local cyclic factor=2 dim=2
    
    #pragma HLS RESOURCE variable=fft_input core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImg core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=tmpImgp core=RAM_2P_BRAM
    #pragma HLS RESOURCE variable=output_local core=RAM_2P_BRAM
    
    // ==================== Algorithm Implementation ====================
    
    // Initialize accumulator to zero
    for (int y = 0; y < fftConvY; y++) {
        for (int x = 0; x < fftConvX; x++) {
            #pragma HLS PIPELINE II=1
            tmpImg[y][x] = 0.0f;
        }
    }
    
    // Mask spectrum center indices
    int Lxh = Lx / 2;  // = 256
    int Lyh = Ly / 2;  // = 256
    
    // Process each kernel
    for (int k = 0; k < nk; k++) {
        #pragma HLS LOOP_FLATTEN
        
        // Step 1: Embed kernel * mask product into FFT input
        embed_kernel_mask_product(
            mskf_r, mskf_i, krn_r, krn_i,
            fft_input, k, Lxh, Lyh
        );
        
        // Step 2: 2D IFFT (frequency → spatial domain)
        // dir=1 for backward transform (IFFT)
        // scaling = 0 (no scaling for IFFT)
        fft_2d(fft_input, fft_output, ap_uint<1>(1), ap_uint<15>(0));
        
        // Step 3: Accumulate intensity
        // tmpImg += scales[k] * |fft_output|^2
        float eigenvalue = scales[k];
        accumulate_intensity(fft_output, tmpImg, eigenvalue);
    }
    
    // Step 4: FFT shift (move zero-frequency to center)
    fftshift_2d(tmpImg, tmpImgp);
    
    // Step 5: Extract center 17×17 region
    extract_center_region(tmpImgp, output_local);
    
    // Step 6: Write output to AXI-MM
    for (int y = 0; y < convY; y++) {
        for (int x = 0; x < convX; x++) {
            #pragma HLS PIPELINE II=1
            int idx = y * convX + x;
            output[idx] = output_local[y][x];
        }
    }
}