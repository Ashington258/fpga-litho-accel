/*
 * FFT Stream Top Level Interface
 * 
 * 完全匹配参考实现的接口配置：
 * - 顶层直接使用stream接口
 * - ap_fifo depth=1024 interface
 * - 直接调用hls::fft，不嵌套
 */

#ifndef FFT_STREAM_TOP_H
#define FFT_STREAM_TOP_H

#include "hls_fft.h"
#include "hls_stream.h"
#include <complex>

// FFT配置（32点）
struct fft_config_32 : hls::ip_fft::params_t {
    static const unsigned FFT_INPUT_WIDTH = 32;
    static const unsigned FFT_OUTPUT_WIDTH = 32;
    static const unsigned FFT_TWIDDLE_WIDTH = 32;
    static const unsigned FFT_NFFT_MAX = 5;  // log2(32)
    static const hls::ip_fft::arch FFT_ARCH = hls::ip_fft::pipelined_streaming_io;
    static const hls::ip_fft::ordering FFT_OUTPUT_ORDER = hls::ip_fft::natural_order;
    static const hls::ip_fft::scaling FFT_SCALING = hls::ip_fft::unscaled;
};

typedef std::complex<float> fft_complex_t;

// FFT顶层函数（Forward）
void fft_forward_top(
    hls::stream<fft_complex_t>& xn,
    hls::stream<fft_complex_t>& xk,
    bool* status
);

// FFT顶层函数（Inverse）
void fft_inverse_top(
    hls::stream<fft_complex_t>& xn,
    hls::stream<fft_complex_t>& xk,
    bool* status
);

#endif // FFT_STREAM_TOP_H