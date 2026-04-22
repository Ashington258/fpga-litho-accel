/**
 * SOCS 2048 Host - DDR Allocation and Data Transfer Interface
 * 
 * This module demonstrates the Host-side DDR buffer management for
 * the SOCS 2048 architecture with Ping-Pong FFT caching.
 * 
 * Architecture:
 *   1. DDR Allocator → Allocate 512KB FFT buffer region
 *   2. Data Preparation → Zero-pad input to 128×128
 *   3. DDR Transfer → Write FFT input to DDR temp1
 *   4. HLS IP Invocation → calc_socs_2048_hls with DDR parameters
 *   5. Result Extraction → Read output from DDR temp3
 * 
 * Usage:
 *   ./socs_2048_host <config.json> --ddr-mode
 */

#include "ddr_allocator.hpp"
#include "file_io.hpp"
#include "json_parser.hpp"
#include "source_processor.hpp"

#include <iostream>
#include <vector>
#include <complex>
#include <string>
#include <fstream>

using namespace DDRConsts;
using ComplexF = std::complex<float>;

// ============================================================================
// DDR Address Map Structure (for HLS AXI-Lite parameters)
// ============================================================================
struct DDRAddressMap {
    uint64_t fft_buf_base;      // FFT buffer base address
    int nx_actual;              // Runtime Nx (2~24)
    int ny_actual;              // Runtime Ny
    int nk;                     // Number of kernels
    int Lx;                     // Mask width
    int Ly;                     // Mask height
};

// ============================================================================
// Host DDR Preparation Pipeline
// ============================================================================

/**
 * Step 1: Initialize DDR allocator
 */
bool init_ddr_allocator(DDRAllocator& allocator, DDRAddressMap& addr_map) {
    if (!allocator.initialize()) {
        std::cerr << "[Host] DDR allocator initialization failed" << std::endl;
        return false;
    }
    
    uint64_t fft_buf_base;
    if (!allocator.allocate_fft_buffer(fft_buf_base)) {
        std::cerr << "[Host] FFT buffer allocation failed" << std::endl;
        return false;
    }
    
    addr_map.fft_buf_base = fft_buf_base;
    
    std::cout << "[Host] DDR allocation completed" << std::endl;
    return true;
}

/**
 * Step 2: Prepare FFT input data (zero-padding)
 */
bool prepare_fft_input(
    const std::string& mskf_r_file,
    const std::string& mskf_i_file,
    const std::string& krn_r_file,
    const std::string& krn_i_file,
    int kernel_idx,
    int nx_actual,
    int ny_actual,
    std::vector<ComplexF>& fft_input_padded
) {
    // Load mask spectrum (512×512)
    std::vector<float> mskf_r, mskf_i;
    size_t mask_size = 512 * 512;  // Expected mask spectrum size
    readFloatArrayFromBinary(mskf_r_file, mskf_r, mask_size);
    readFloatArrayFromBinary(mskf_i_file, mskf_i, mask_size);
    
    // Load kernel (kernel_idx)
    // Kernel size: (2*nx+1) × (2*ny+1)
    int kernel_size = 2 * nx_actual + 1;
    int kernel_h = 2 * ny_actual + 1;
    size_t krn_size = kernel_size * kernel_h;
    
    std::vector<float> krn_r, krn_i;
    readFloatArrayFromBinary(krn_r_file, krn_r, krn_size);
    readFloatArrayFromBinary(krn_i_file, krn_i, krn_size);
    
    std::cout << "[Host] Loaded kernel " << kernel_idx 
              << " (size: " << kernel_size << "×" << kernel_h << ")" << std::endl;
    
    // Create embedded data: mask_spectrum × kernel
    // For 2048 architecture, we create a 128×128 zero-padded FFT input
    int embedded_w = kernel_size;
    int embedded_h = kernel_h;
    
    std::vector<ComplexF> fft_input(embedded_w * embedded_h);
    
    // Embed kernel with mask (simplified - actual implementation in HLS)
    for (int y = 0; y < embedded_h; y++) {
        for (int x = 0; x < embedded_w; x++) {
            int krn_idx = y * kernel_size + x;
            
            // Use kernel data directly (mask embedding done in HLS)
            fft_input[y * embedded_w + x] = ComplexF(krn_r[krn_idx], krn_i[krn_idx]);
        }
    }
    
    // Zero-pad to 128×128
    DDRTransfer::zero_pad_fft_input(fft_input, fft_input_padded, nx_actual, ny_actual);
    
    return true;
}

/**
 * Step 3: Write FFT input to DDR temp1 buffer
 */
bool write_fft_input_to_ddr(
    DDRAllocator& allocator,
    const std::vector<ComplexF>& fft_input_padded
) {
    void* ddr_ptr = allocator.get_fft_buffer().mapped_ptr;
    if (!ddr_ptr) {
        std::cerr << "[Host] DDR buffer not mapped" << std::endl;
        return false;
    }
    
    // Write to temp1 region (offset = 0)
    return DDRTransfer::write_fft_input_to_ddr(fft_input_padded, ddr_ptr);
}

/**
 * Step 4: Prepare HLS invocation parameters
 */
void prepare_hls_parameters(
    const DDRAddressMap& addr_map,
    uint64_t& fft_buf_base_out,
    int& nx_out,
    int& ny_out,
    int& nk_out,
    int& Lx_out,
    int& Ly_out
) {
    fft_buf_base_out = addr_map.fft_buf_base;
    nx_out = addr_map.nx_actual;
    ny_out = addr_map.ny_actual;
    nk_out = addr_map.nk;
    Lx_out = addr_map.Lx;
    Ly_out = addr_map.Ly;
    
    std::cout << "[Host] HLS parameters prepared:" << std::endl;
    std::cout << "  fft_buf_base: 0x" << std::hex << fft_buf_base_out << std::dec << std::endl;
    std::cout << "  nx_actual: " << nx_out << std::endl;
    std::cout << "  ny_actual: " << ny_out << std::endl;
    std::cout << "  nk: " << nk_out << std::endl;
}

/**
 * Step 5: Read FFT output from DDR temp3 buffer (after HLS IP execution)
 */
bool read_fft_output_from_ddr(
    DDRAllocator& allocator,
    std::vector<ComplexF>& fft_output
) {
    // temp3 is at offset 0x00040000
    void* ddr_ptr = allocator.get_fft_buffer().mapped_ptr;
    if (!ddr_ptr) {
        std::cerr << "[Host] DDR buffer not mapped" << std::endl;
        return false;
    }
    
    // Read from temp3 region
    char* temp3_ptr = static_cast<char*>(ddr_ptr) + TEMP3_OFFSET;
    
    return DDRTransfer::read_fft_output_from_ddr(fft_output, temp3_ptr);
}

// ============================================================================
// Main Entry (Demonstration)
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "SOCS 2048 Host - DDR Allocation Demo" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: " << argv[0] << " <config.json> [--ddr-mode]" << std::endl;
        std::cout << std::endl;
        std::cout << "Modes:" << std::endl;
        std::cout << "  --ddr-mode: Enable DDR caching mode (default: off)" << std::endl;
        std::cout << std::endl;
        std::cout << "DDR Layout:" << std::endl;
        std::cout << "  temp1 (0x00000000): FFT input buffer  (128KB)" << std::endl;
        std::cout << "  temp2 (0x00020000): Row FFT output   (128KB)" << std::endl;
        std::cout << "  temp3 (0x00040000): Col FFT output   (128KB)" << std::endl;
        std::cout << "  Reserved (0x00060000): Future use   (128KB)" << std::endl;
        return 1;
    }
    
    std::string configPath = argv[1];
    bool ddr_mode = (argc > 2 && std::string(argv[2]) == "--ddr-mode");
    
    std::cout << "=== SOCS 2048 Host - DDR Preparation Pipeline ===" << std::endl;
    std::cout << "Config: " << configPath << std::endl;
    std::cout << "DDR Mode: " << (ddr_mode ? "enabled" : "disabled") << std::endl;
    std::cout << std::endl;
    
    // Load configuration
    SOCSConfig config;
    if (!loadConfig(configPath, config)) {
        std::cerr << "[Host] Failed to load config" << std::endl;
        return 1;
    }
    
    // Compute Nx/Ny dynamically
    computeNxNy(config);
    
    std::cout << "=== Configuration ===" << std::endl;
    std::cout << "  Lx = " << config.Lx << ", Ly = " << config.Ly << std::endl;
    std::cout << "  Nx = " << config.Nx << ", Ny = " << config.Ny << std::endl;
    std::cout << "  nk = " << config.nk << std::endl;
    std::cout << "  NA = " << config.NA << ", λ = " << config.wavelength << " nm" << std::endl;
    std::cout << std::endl;
    
    // Initialize DDR allocator
    DDRAllocator allocator;
    DDRAddressMap addr_map;
    
    addr_map.nx_actual = config.Nx;
    addr_map.ny_actual = config.Ny;
    addr_map.nk = config.nk;
    addr_map.Lx = config.Lx;
    addr_map.Ly = config.Ly;
    
    if (!init_ddr_allocator(allocator, addr_map)) {
        return 1;
    }
    
    std::cout << std::endl;
    
    // Prepare FFT input (demonstration with kernel 0)
    // Use project root relative path
    std::string projectRoot = "../../";  // Relative to source/host directory
    std::string outputDir = projectRoot + "output/verification";
    std::string mskf_r_file = outputDir + "/mskf_r.bin";
    std::string mskf_i_file = outputDir + "/mskf_i.bin";
    std::string krn_dir = outputDir + "/kernels";
    std::string krn_r_file = krn_dir + "/krn_0_r.bin";
    std::string krn_i_file = krn_dir + "/krn_0_i.bin";
    
    std::vector<ComplexF> fft_input_padded;
    if (!prepare_fft_input(mskf_r_file, mskf_i_file, krn_r_file, krn_i_file,
                           0, config.Nx, config.Ny, fft_input_padded)) {
        std::cerr << "[Host] FFT input preparation failed" << std::endl;
        allocator.release();
        return 1;
    }
    
    std::cout << std::endl;
    
    // Write FFT input to DDR
    if (!write_fft_input_to_ddr(allocator, fft_input_padded)) {
        std::cerr << "[Host] DDR write failed" << std::endl;
        allocator.release();
        return 1;
    }
    
    std::cout << std::endl;
    
    // Prepare HLS parameters
    uint64_t fft_buf_base;
    int nx, ny, nk, Lx, Ly;
    prepare_hls_parameters(addr_map, fft_buf_base, nx, ny, nk, Lx, Ly);
    
    std::cout << std::endl;
    std::cout << "=== HLS Invocation Parameters ===" << std::endl;
    std::cout << "  (In real deployment, these would be passed to HLS IP via AXI-Lite)" << std::endl;
    std::cout << std::endl;
    std::cout << "  mskf_r:      DDR address (from mask spectrum)" << std::endl;
    std::cout << "  mskf_i:      DDR address" << std::endl;
    std::cout << "  scales:      DDR address (eigenvalues)" << std::endl;
    std::cout << "  krn_r:       DDR address (kernel real)" << std::endl;
    std::cout << "  krn_i:       DDR address (kernel imag)" << std::endl;
    std::cout << "  output:      DDR address (aerial image output)" << std::endl;
    std::cout << "  fft_buf_ddr: DDR address (FFT buffer)" << std::endl;
    std::cout << std::endl;
    std::cout << "  AXI-Lite parameters:" << std::endl;
    std::cout << "    nx_actual:   " << nx << std::endl;
    std::cout << "    ny_actual:   " << ny << std::endl;
    std::cout << "    nk:          " << nk << std::endl;
    std::cout << "    Lx:          " << Lx << std::endl;
    std::cout << "    Ly:          " << Ly << std::endl;
    std::cout << "    fft_buf_base: 0x" << std::hex << fft_buf_base << std::dec << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== DDR Buffer Status ===" << std::endl;
    std::cout << "  temp1 (FFT input):  Ready (written)" << std::endl;
    std::cout << "  temp2 (Row FFT):    Empty (will be filled by HLS)" << std::endl;
    std::cout << "  temp3 (Col FFT):    Empty (will be filled by HLS)" << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== Next Steps ===" << std::endl;
    std::cout << "  1. Invoke HLS IP: calc_socs_2048_hls(...)" << std::endl;
    std::cout << "  2. Wait for ap_done signal" << std::endl;
    std::cout << "  3. Read output from DDR temp3 buffer" << std::endl;
    std::cout << "  4. Extract valid region (2*nx+1 × 2*ny+1)" << std::endl;
    
    // Cleanup
    allocator.release();
    
    std::cout << std::endl;
    std::cout << "[Host] DDR preparation pipeline completed successfully" << std::endl;
    
    return 0;
}