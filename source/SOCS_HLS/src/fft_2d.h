/*
 * 2D FFT模块配置
 * SOCS HLS Project
 * 
 * HLS FFT配置说明：
 * - 使用 use_native_float = false 以避免SSR限制
 * - phase_factor_width = 24 (非原生浮点)
 * - scaling_opt = scaled (scale_sch = 0 表示不缩放)
 * - complex_mult_type = use_luts
 * - butterfly_type = use_luts
 */

#ifndef FFT_2D_H
#define FFT_2D_H

// C仿真兼容性
#ifdef __SYNTHESIS__
    #include "hls_fft.h"
    #include "hls_stream.h"
#else
    // C仿真时的模拟定义
    #include <complex>
    #include <queue>
    #include <cmath>
    
    // 模拟hls::stream
    namespace hls {
        template<typename T>
        class stream {
        private:
            std::queue<T> data_queue;
        public:
            void write(const T& val) { data_queue.push(val); }
            T read() { 
                T val = data_queue.front(); 
                data_queue.pop(); 
                return val; 
            }
            bool empty() const { return data_queue.empty(); }
        };
    }
    
    // 模拟FFT配置结构（简化版本）
    namespace hls {
        namespace ip_fft {
            enum arch { pipelined_streaming_io, radix_4, radix_2 };
            enum ordering { natural_order, reverse_order };
            enum scaling { scaled, unscaled };
            enum rounding { truncation };
            enum mem { block_ram };
            enum opt { use_luts, use_dsp };
        }
        
        // 模拟FFT模板
        template<typename config_t>
        void fft(stream<std::complex<float>>& in, stream<std::complex<float>>& out,
                 bool direction, int scaling_sch, int nfft, bool* ovflo) {
            // C仿真时的简化FFT：直接复制数据（不执行真实FFT）
            while (!in.empty()) {
                out.write(in.read());
            }
        }
    }
    
    // 模拟FFT配置结构（简化版本）
    namespace hls {
        namespace ip_fft {
            enum arch { pipelined_streaming_io, radix_4, radix_2 };
            enum ordering { natural_order, reverse_order };
            enum scaling { scaled, unscaled, scale };
            enum rounding { truncation, truncate };
            enum mem { block_ram };
            enum opt { use_luts, use_dsp };
            
            // params_t基类
            struct params_t {};
        }
        
        // 模拟FFT模板
        template<typename config_t>
        void fft(stream<std::complex<float>>& in, stream<std::complex<float>>& out,
                 bool direction, int scaling_sch, int nfft, bool* ovflo) {
            // C仿真时的简化FFT：直接复制数据（不执行真实FFT）
            while (!in.empty()) {
                out.write(in.read());
            }
        }
    }
    
    // C仿真时使用简化的FFT函数定义
    void fft_1d_forward_32_sim(hls::stream<std::complex<float>>& input,
                                hls::stream<std::complex<float>>& output);
    void fft_1d_inverse_32_sim(hls::stream<std::complex<float>>& input,
                                hls::stream<std::complex<float>>& output);
    void fft_2d_forward_32_sim(std::complex<float>* input,
                                std::complex<float>* output);
    void fft_2d_inverse_32_sim(std::complex<float>* input,
                                std::complex<float>* output);
    
#endif

#include <complex>

typedef std::complex<float> fft_complex_t;

const int FFT_SIZE_17_PADDED = 32;
const int FFT_SIZE_17_ACTUAL = 17;
const int FFT_SIZE_512 = 512;

// ============================================================================
// FFT配置结构（非原生浮点模式）
// ============================================================================

// 32点FFT配置
struct fft_config_32 : hls::ip_fft::params_t {
    // 数据宽度
    static const unsigned input_width = 32;
    static const unsigned output_width = 32;
    static const unsigned phase_factor_width = 24;
    static const unsigned status_width = 8;
    static const unsigned config_width = 8;
    
    // FFT长度
    static const unsigned max_nfft = 5;
    
    // 架构
    static const hls::ip_fft::arch arch_opt = hls::ip_fft::pipelined_streaming_io;
    
    // 输出顺序
    static const hls::ip_fft::ordering ordering_opt = hls::ip_fft::natural_order;
    
    // 缩放模式
    static const hls::ip_fft::scaling scaling_opt = hls::ip_fft::scaled;
    
    // 舍入模式
    static const hls::ip_fft::rounding rounding_opt = hls::ip_fft::truncation;
    
    // 存储类型
    static const hls::ip_fft::mem mem_data = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_phase_factors = hls::ip_fft::block_ram;
    static const hls::ip_fft::mem mem_reorder = hls::ip_fft::block_ram;
    
    // 实现选项
    static const hls::ip_fft::opt complex_mult_type = hls::ip_fft::use_luts;
    static const hls::ip_fft::opt butterfly_type = hls::ip_fft::use_luts;
    
    // 非原生浮点模式
    static const bool use_native_float = false;
    
    // 其他选项
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
