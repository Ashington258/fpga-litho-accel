/*
 * 2D FFT模块配置（简化版本）
 * SOCS HLS Project
 * 
 * 注意：简化版本不使用真实FFT，仅做点乘和插值
 * 此文件仅包含必要的类型定义，避免CoSim冲突
 */

#ifndef FFT_2D_H
#define FFT_2D_H

#include <complex>

typedef std::complex<float> fft_complex_t;

// FFT尺寸常量（用于综合时的数组尺寸计算）
const int FFT_SIZE_17_PADDED = 32;
const int FFT_SIZE_17_ACTUAL = 17;
const int FFT_SIZE_512 = 512;

// ============================================================================
// C仿真时的模拟FFT函数声明
// ============================================================================
#ifndef __SYNTHESIS__

// 模拟FFT函数（仅在C仿真时使用）
void fft_1d_forward_32_sim(std::complex<float>* input, std::complex<float>* output);
void fft_1d_inverse_32_sim(std::complex<float>* input, std::complex<float>* output);
void fft_2d_forward_32_sim(std::complex<float>* input, std::complex<float>* output);
void fft_2d_inverse_32_sim(std::complex<float>* input, std::complex<float>* output);

#endif // __SYNTHESIS__

#endif // FFT_2D_H
    static const bool has_nfft = false;
    static const bool ovflo = false;
    static const bool xk_index = false;
    static const bool cyclic_prefix_insertion = false;
};

// 512点FFT配置
struct fft_config_512 : hls::ip_fft::params_t {
    static const unsigned input_width = 32;
    static const unsigned output_width = 32;
    static const unsigned phase_factor_width = 24;
    static const unsigned status_width = 8;
    static const unsigned config_width = 8;
    
    static const unsigned max_nfft = 9;
    
    static const hls::ip_fft::arch arch_opt = hls::ip_fft::pipelined_streaming_io;
    static const hls::ip_fft::ordering ordering_opt = hls::ip_fft::natural_order;
    static const hls::ip_fft::scaling scaling_opt = hls::ip_fft::scaled;
    static const hls::ip_fft::rounding rounding_opt = hls::ip_fft::truncation;
    
    static const hls::ip_fft::mem mem_data = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_phase_factors = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_reorder = hls::ip_fft::block_ram;
    
    static const hls::ip_fft::opt complex_mult_type = hls::ip_fft::use_luts;
    static const hls::ip_fft::opt butterfly_type = hls::ip_fft::use_luts;
    
    static const bool use_native_float = false;
    static const bool has_nfft = false;
    static const bool ovflo = false;
    static const bool xk_index = false;
    static const bool cyclic_prefix_insertion = false;
};

// ============================================================================
// FFT数据类型
// ============================================================================

typedef float fft_data_t;
typedef std::complex<fft_data_t> fft_complex_t;

// ============================================================================
// 函数声明
// ============================================================================

void fft_1d_forward_32(hls::stream<fft_complex_t>& input, hls::stream<fft_complex_t>& output);
void fft_1d_inverse_32(hls::stream<fft_complex_t>& input, hls::stream<fft_complex_t>& output);

void fft_1d_forward_512(hls::stream<fft_complex_t>& input, hls::stream<fft_complex_t>& output);
void fft_1d_inverse_512(hls::stream<fft_complex_t>& input, hls::stream<fft_complex_t>& output);

void fft_2d_forward_32(fft_complex_t* input, fft_complex_t* output);
void fft_2d_inverse_32(fft_complex_t* input, fft_complex_t* output);

void fft_2d_forward_512(fft_complex_t* input, fft_complex_t* output);
void fft_2d_inverse_512(fft_complex_t* input, fft_complex_t* output);

void transpose_32x32(fft_complex_t* input, fft_complex_t* output);
void transpose_512x512(fft_complex_t* input, fft_complex_t* output);

#endif // FFT_2D_H
