/**
 * DDR Allocator for SOCS 2048 Architecture
 * 
 * Purpose: Manage DDR memory allocation for FFT caching
 * 
 * DDR Layout (512KB total):
 *   0x00000000 - 0x0001FFFF: temp1 (128KB) - FFT input buffer
 *   0x00020000 - 0x0003FFFF: temp2 (128KB) - Row FFT output buffer  
 *   0x00040000 - 0x0005FFFF: temp3 (128KB) - Column FFT output buffer
 *   0x00060000 - 0x0007FFFF: Reserved (128KB) - Future expansion
 * 
 * Each 128KB block = 128×128 complex float = 32,768 complex × 8 bytes
 */

#ifndef HOST_DDR_ALLOCATOR_HPP
#define HOST_DDR_ALLOCATOR_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <complex>

// ============================================================================
// DDR Constants (matching HLS socs_2048.h)
// ============================================================================
namespace DDRConsts {
    constexpr size_t DDR_BASE_ADDR   = 0x80000000UL;  // FPGA DDR base (typical)
    constexpr size_t FFT_BUF_SIZE    = 512 * 1024;    // 512KB total
    constexpr size_t TEMP_BLOCK_SIZE = 128 * 1024;    // 128KB per temp block
    
    // Offset addresses (relative to fft_buf_base)
    constexpr size_t TEMP1_OFFSET    = 0x00000000UL;  // FFT input
    constexpr size_t TEMP2_OFFSET    = 0x00020000UL;  // Row FFT output
    constexpr size_t TEMP3_OFFSET    = 0x00040000UL;  // Col FFT output
    
    // Block dimensions
    constexpr int BLOCK_SIZE         = 16;            // Ping-Pong block size
    constexpr int MAX_FFT_X          = 128;           // Max FFT width
    constexpr int MAX_FFT_Y          = 128;           // Max FFT height
    constexpr int NUM_BLOCKS_X       = MAX_FFT_X / BLOCK_SIZE;  // 8 blocks
    constexpr int NUM_BLOCKS_Y       = MAX_FFT_Y / BLOCK_SIZE;  // 8 blocks
}

// ============================================================================
// DDR Buffer Structure
// ============================================================================
struct DDRBuffer {
    uint64_t base_addr;        // Physical DDR address
    size_t size;               // Total buffer size
    void* mapped_ptr;          // Host mapped pointer (if applicable)
    bool is_allocated;         // Allocation status
    
    DDRBuffer() : base_addr(0), size(0), mapped_ptr(nullptr), is_allocated(false) {}
};

// ============================================================================
// DDR Allocator Class
// ============================================================================
class DDRAllocator {
public:
    DDRAllocator();
    ~DDRAllocator();
    
    // Initialize allocator with FPGA DDR base address
    bool initialize(uint64_t fpga_ddr_base = DDRConsts::DDR_BASE_ADDR);
    
    // Allocate FFT buffer region (512KB)
    bool allocate_fft_buffer(uint64_t& fft_buf_base_out);
    
    // Get sub-block addresses
    uint64_t get_temp1_addr() const;  // FFT input buffer
    uint64_t get_temp2_addr() const;  // Row FFT output
    uint64_t get_temp3_addr() const;  // Column FFT output
    
    // Free allocated buffers
    void release();
    
    // Status check
    bool is_initialized() const { return initialized_; }
    bool is_fft_buffer_allocated() const { return fft_buffer_.is_allocated; }
    
    // Get buffer info
    const DDRBuffer& get_fft_buffer() const { return fft_buffer_; }
    
private:
    bool initialized_;
    uint64_t ddr_base_;
    DDRBuffer fft_buffer_;
    
    // Track allocated regions
    std::vector<DDRBuffer> allocated_regions_;
};

// ============================================================================
// Host Data Transfer Interface
// ============================================================================
namespace DDRTransfer {
    
    /**
     * Prepare FFT input data in DDR temp1 buffer
     * 
     * @param fft_input: 128×128 complex array (zero-padded)
     * @param ddr_ptr: Pointer to DDR memory region
     * @return: true if successful
     */
    bool write_fft_input_to_ddr(
        const std::vector<std::complex<float>>& fft_input,
        void* ddr_ptr
    );
    
    /**
     * Read FFT output from DDR temp3 buffer
     * 
     * @param fft_output: 128×128 complex array (output)
     * @param ddr_ptr: Pointer to DDR memory region
     * @return: true if successful
     */
    bool read_fft_output_from_ddr(
        std::vector<std::complex<float>>& fft_output,
        const void* ddr_ptr
    );
    
    /**
     * Zero-pad input data to 128×128
     * 
     * @param input: Original Nx×Ny data
     * @param output: 128×128 zero-padded output
     * @param nx_actual: Actual Nx (2~24)
     * @param ny_actual: Actual Ny
     */
    void zero_pad_fft_input(
        const std::vector<std::complex<float>>& input,
        std::vector<std::complex<float>>& output,
        int nx_actual,
        int ny_actual
    );
    
    /**
     * Extract valid region from 128×128 FFT output
     * 
     * @param input: 128×128 FFT output
     * @param output: Valid region (2*nx+1 × 2*ny+1)
     * @param nx_actual: Actual Nx
     * @param ny_actual: Actual Ny
     */
    void extract_valid_region(
        const std::vector<std::complex<float>>& input,
        std::vector<std::complex<float>>& output,
        int nx_actual,
        int ny_actual
    );
}

#endif // HOST_DDR_ALLOCATOR_HPP