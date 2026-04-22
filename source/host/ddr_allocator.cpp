/**
 * DDR Allocator Implementation for SOCS 2048 Architecture
 * 
 * Implementation notes:
 *   - In actual FPGA deployment, DDR allocation requires OS memory management
 *   - For simulation/testing, we use host memory as DDR proxy
 *   - Physical DDR address management depends on FPGA platform
 */

#include "ddr_allocator.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

using namespace DDRConsts;

// ============================================================================
// DDRAllocator Implementation
// ============================================================================

DDRAllocator::DDRAllocator()
    : initialized_(false)
    , ddr_base_(0)
{
}

DDRAllocator::~DDRAllocator() {
    release();
}

bool DDRAllocator::initialize(uint64_t fpga_ddr_base) {
    if (initialized_) {
        std::cerr << "[DDRAllocator] Already initialized" << std::endl;
        return true;
    }
    
    ddr_base_ = fpga_ddr_base;
    initialized_ = true;
    
    std::cout << "[DDRAllocator] Initialized with DDR base: 0x" 
              << std::hex << ddr_base_ << std::dec << std::endl;
    
    return true;
}

bool DDRAllocator::allocate_fft_buffer(uint64_t& fft_buf_base_out) {
    if (!initialized_) {
        std::cerr << "[DDRAllocator] Not initialized. Call initialize() first." << std::endl;
        return false;
    }
    
    if (fft_buffer_.is_allocated) {
        std::cerr << "[DDRAllocator] FFT buffer already allocated" << std::endl;
        fft_buf_base_out = fft_buffer_.base_addr;
        return true;
    }
    
    // Calculate FFT buffer base address
    fft_buffer_.base_addr = ddr_base_;
    fft_buffer_.size = FFT_BUF_SIZE;
    
    // In real deployment, this would call platform-specific DDR allocation API
    // For simulation, we allocate host memory as proxy
    fft_buffer_.mapped_ptr = malloc(FFT_BUF_SIZE);
    if (!fft_buffer_.mapped_ptr) {
        std::cerr << "[DDRAllocator] Failed to allocate " << FFT_BUF_SIZE 
                  << " bytes for FFT buffer" << std::endl;
        return false;
    }
    
    // Zero initialize buffer
    memset(fft_buffer_.mapped_ptr, 0, FFT_BUF_SIZE);
    fft_buffer_.is_allocated = true;
    
    fft_buf_base_out = fft_buffer_.base_addr;
    
    std::cout << "[DDRAllocator] FFT buffer allocated:" << std::endl;
    std::cout << "  Base address: 0x" << std::hex << fft_buffer_.base_addr << std::dec << std::endl;
    std::cout << "  Size: " << fft_buffer_.size << " bytes (512KB)" << std::endl;
    std::cout << "  temp1 (FFT input): 0x" << std::hex << (fft_buffer_.base_addr + TEMP1_OFFSET) << std::dec << std::endl;
    std::cout << "  temp2 (Row FFT):   0x" << std::hex << (fft_buffer_.base_addr + TEMP2_OFFSET) << std::dec << std::endl;
    std::cout << "  temp3 (Col FFT):   0x" << std::hex << (fft_buffer_.base_addr + TEMP3_OFFSET) << std::dec << std::endl;
    
    allocated_regions_.push_back(fft_buffer_);
    
    return true;
}

uint64_t DDRAllocator::get_temp1_addr() const {
    if (!fft_buffer_.is_allocated) return 0;
    return fft_buffer_.base_addr + TEMP1_OFFSET;
}

uint64_t DDRAllocator::get_temp2_addr() const {
    if (!fft_buffer_.is_allocated) return 0;
    return fft_buffer_.base_addr + TEMP2_OFFSET;
}

uint64_t DDRAllocator::get_temp3_addr() const {
    if (!fft_buffer_.is_allocated) return 0;
    return fft_buffer_.base_addr + TEMP3_OFFSET;
}

void DDRAllocator::release() {
    if (fft_buffer_.mapped_ptr) {
        free(fft_buffer_.mapped_ptr);
        fft_buffer_.mapped_ptr = nullptr;
    }
    fft_buffer_.is_allocated = false;
    allocated_regions_.clear();
    initialized_ = false;
    
    std::cout << "[DDRAllocator] Released all buffers" << std::endl;
}

// ============================================================================
// DDRTransfer Implementation
// ============================================================================

namespace DDRTransfer {

bool write_fft_input_to_ddr(
    const std::vector<std::complex<float>>& fft_input,
    void* ddr_ptr
) {
    const size_t expected_size = MAX_FFT_X * MAX_FFT_Y;
    
    if (fft_input.size() != expected_size) {
        std::cerr << "[DDRTransfer] Invalid FFT input size: " << fft_input.size()
                  << ", expected: " << expected_size << std::endl;
        return false;
    }
    
    // Write to DDR temp1 region
    size_t copy_size = expected_size * sizeof(std::complex<float>);
    memcpy(ddr_ptr, fft_input.data(), copy_size);
    
    std::cout << "[DDRTransfer] Written FFT input to DDR: " << copy_size << " bytes" << std::endl;
    return true;
}

bool read_fft_output_from_ddr(
    std::vector<std::complex<float>>& fft_output,
    const void* ddr_ptr
) {
    const size_t expected_size = MAX_FFT_X * MAX_FFT_Y;
    
    fft_output.resize(expected_size);
    
    // Read from DDR temp3 region
    size_t copy_size = expected_size * sizeof(std::complex<float>);
    memcpy(fft_output.data(), ddr_ptr, copy_size);
    
    std::cout << "[DDRTransfer] Read FFT output from DDR: " << copy_size << " bytes" << std::endl;
    return true;
}

void zero_pad_fft_input(
    const std::vector<std::complex<float>>& input,
    std::vector<std::complex<float>>& output,
    int nx_actual,
    int ny_actual
) {
    // Input size: (2*nx+1) × (2*ny+1)
    int input_w = 2 * nx_actual + 1;
    int input_h = 2 * ny_actual + 1;
    
    // Output size: 128×128 (zero-padded)
    output.resize(MAX_FFT_X * MAX_FFT_Y, std::complex<float>(0.0f, 0.0f));
    
    // Copy input data to center of output
    int offset_x = (MAX_FFT_X - input_w) / 2;
    int offset_y = (MAX_FFT_Y - input_h) / 2;
    
    for (int y = 0; y < input_h; y++) {
        for (int x = 0; x < input_w; x++) {
            int out_x = x + offset_x;
            int out_y = y + offset_y;
            
            if (out_x >= 0 && out_x < MAX_FFT_X && 
                out_y >= 0 && out_y < MAX_FFT_Y) {
                
                int in_idx = y * input_w + x;
                int out_idx = out_y * MAX_FFT_X + out_x;
                
                output[out_idx] = input[in_idx];
            }
        }
    }
    
    std::cout << "[DDRTransfer] Zero-padded " << input_w << "×" << input_h 
              << " input to " << MAX_FFT_X << "×" << MAX_FFT_Y << std::endl;
}

void extract_valid_region(
    const std::vector<std::complex<float>>& input,
    std::vector<std::complex<float>>& output,
    int nx_actual,
    int ny_actual
) {
    // Input size: 128×128
    // Output size: (2*nx+1) × (2*ny+1)
    int output_w = 2 * nx_actual + 1;
    int output_h = 2 * ny_actual + 1;
    
    output.resize(output_w * output_h);
    
    // Extract center region from input
    int offset_x = (MAX_FFT_X - output_w) / 2;
    int offset_y = (MAX_FFT_Y - output_h) / 2;
    
    for (int y = 0; y < output_h; y++) {
        for (int x = 0; x < output_w; x++) {
            int in_x = x + offset_x;
            int in_y = y + offset_y;
            
            if (in_x >= 0 && in_x < MAX_FFT_X && 
                in_y >= 0 && in_y < MAX_FFT_Y) {
                
                int in_idx = in_y * MAX_FFT_X + in_x;
                int out_idx = y * output_w + x;
                
                output[out_idx] = input[in_idx];
            } else {
                // Outside bounds: zero
                output[y * output_w + x] = std::complex<float>(0.0f, 0.0f);
            }
        }
    }
    
    std::cout << "[DDRTransfer] Extracted " << output_w << "×" << output_h 
              << " valid region from " << MAX_FFT_X << "×" << MAX_FFT_Y << std::endl;
}

} // namespace DDRTransfer