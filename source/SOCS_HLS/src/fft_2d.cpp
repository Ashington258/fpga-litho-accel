/*
 * 2D FFT模块实现
 * SOCS HLS Project
 * 
 * 关键：使用32×32 FFT处理17×17数据（零填充方案）
 */

#include "fft_2d.h"

// ============================================================================
// 1D FFT实现（32点）
// ============================================================================

void fft_1d_forward_32(
    hls::stream<fft_complex_t>& input,
    hls::stream<fft_complex_t>& output
) {
#pragma HLS DATAFLOW
#pragma HLS STREAM variable=input depth=64
#pragma HLS STREAM variable=output depth=64
#pragma HLS INTERFACE ap_fifo port=input depth=64
#pragma HLS INTERFACE ap_fifo port=output depth=64
    
    bool ovflo;
    // For scaled mode: scale_sch = 0 means no scaling (divide by 1)
    hls::fft<fft_config_32>(input, output, false, 0, -1, &ovflo);
}

void fft_1d_inverse_32(
    hls::stream<fft_complex_t>& input,
    hls::stream<fft_complex_t>& output
) {
#pragma HLS DATAFLOW
#pragma HLS STREAM variable=input depth=64
#pragma HLS STREAM variable=output depth=64
#pragma HLS INTERFACE ap_fifo port=input depth=64
#pragma HLS INTERFACE ap_fifo port=output depth=64
    
    bool ovflo;
    // For scaled mode IFFT: scale_sch = 0x155 (binary 01 01 01 01 01)
    // Each stage (5 stages for 32-point FFT) divides by 2
    // Total scaling = 1/32 (compensates for IFFT 1/N factor)
    hls::fft<fft_config_32>(input, output, true, 0x155, -1, &ovflo);
}

// ============================================================================
// 1D FFT实现（512点）
// ============================================================================

void fft_1d_forward_512(
    hls::stream<fft_complex_t>& input,
    hls::stream<fft_complex_t>& output
) {
#pragma HLS DATAFLOW
#pragma HLS STREAM variable=input depth=512
#pragma HLS STREAM variable=output depth=512
    
    bool ovflo;
    // For scaled mode: scale_sch = 0 means no scaling (divide by 1)
    hls::fft<fft_config_512>(input, output, false, 0, -1, &ovflo);
}

void fft_1d_inverse_512(
    hls::stream<fft_complex_t>& input,
    hls::stream<fft_complex_t>& output
) {
#pragma HLS DATAFLOW
#pragma HLS STREAM variable=input depth=512
#pragma HLS STREAM variable=output depth=512
    
    bool ovflo;
    // For scaled mode IFFT: scale_sch = 0x1B5 (9 stages for 512-point FFT)
    // Each stage divides by 2, total scaling = 1/512
    hls::fft<fft_config_512>(input, output, true, 0x1B5, -1, &ovflo);
}

// ============================================================================
// 转置函数（32×32）
// ============================================================================

void transpose_32x32(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 32;
    
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
#pragma HLS PIPELINE
            output[j * SIZE + i] = input[i * SIZE + j];
        }
    }
}

// ============================================================================
// 转置函数（512×512）
// ============================================================================

void transpose_512x512(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 512;
    
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
#pragma HLS PIPELINE
            output[j * SIZE + i] = input[i * SIZE + j];
        }
    }
}

// ============================================================================
// 2D FFT实现（32×32）
// ============================================================================

void fft_2d_forward_32(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 32;
    
    // 临时缓冲
    fft_complex_t temp[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp cyclic factor=4 dim=1
    
    // Step 1: Row-wise FFT
    hls::stream<fft_complex_t> row_in;
    hls::stream<fft_complex_t> row_out;
    
    for (int row = 0; row < SIZE; row++) {
        // 读取一行数据
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(input[row * SIZE + col]);
        }
        
        // 执行1D FFT
        fft_1d_forward_32(row_in, row_out);
        
        // 写回结果
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp[row * SIZE + col] = row_out.read();
        }
    }
    
    // Step 2: Transpose
    transpose_32x32(temp, output);
    
    // Step 3: Column-wise FFT (实际是对转置后的矩阵做行FFT)
    fft_complex_t temp2[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=4 dim=1
    
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(output[row * SIZE + col]);
        }
        
        fft_1d_forward_32(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp2[row * SIZE + col] = row_out.read();
        }
    }
    
    // 再次转置恢复原始顺序
    transpose_32x32(temp2, output);
}

void fft_2d_inverse_32(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 32;
    
    fft_complex_t temp[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp cyclic factor=4 dim=1
    
    hls::stream<fft_complex_t> row_in;
    hls::stream<fft_complex_t> row_out;
    
    // Row-wise IFFT
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(input[row * SIZE + col]);
        }
        
        fft_1d_inverse_32(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_32x32(temp, output);
    
    // Column-wise IFFT
    fft_complex_t temp2[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=4 dim=1
    
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(output[row * SIZE + col]);
        }
        
        fft_1d_inverse_32(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp2[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_32x32(temp2, output);
    
    // Manual scaling for C Simulation (RTL synthesis will use scale_sch)
#ifndef __SYNTHESIS__
    // In C Simulation, FFT/IFFT doesn't apply scaling automatically
    // Need to manually scale by 1/(SIZE*SIZE) = 1/1024
    for (int i = 0; i < SIZE * SIZE; i++) {
        output[i] = fft_complex_t(output[i].real() / (SIZE * SIZE), 
                                   output[i].imag() / (SIZE * SIZE));
    }
#endif
}

// ============================================================================
// 2D FFT实现（512×512）
// ============================================================================

void fft_2d_forward_512(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 512;
    
    fft_complex_t temp[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp cyclic factor=8 dim=1
    
    hls::stream<fft_complex_t> row_in;
    hls::stream<fft_complex_t> row_out;
    
    // Row FFT
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(input[row * SIZE + col]);
        }
        
        fft_1d_forward_512(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_512x512(temp, output);
    
    // Column FFT
    fft_complex_t temp2[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=8 dim=1
    
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(output[row * SIZE + col]);
        }
        
        fft_1d_forward_512(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp2[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_512x512(temp2, output);
}

void fft_2d_inverse_512(
    fft_complex_t* input,
    fft_complex_t* output
) {
    const int SIZE = 512;
    
    fft_complex_t temp[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp cyclic factor=8 dim=1
    
    hls::stream<fft_complex_t> row_in;
    hls::stream<fft_complex_t> row_out;
    
    // Row IFFT
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(input[row * SIZE + col]);
        }
        
        fft_1d_inverse_512(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_512x512(temp, output);
    
    // Column IFFT
    fft_complex_t temp2[SIZE * SIZE];
#pragma HLS ARRAY_PARTITION variable=temp2 cyclic factor=8 dim=1
    
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            row_in.write(output[row * SIZE + col]);
        }
        
        fft_1d_inverse_512(row_in, row_out);
        
        for (int col = 0; col < SIZE; col++) {
#pragma HLS PIPELINE
            temp2[row * SIZE + col] = row_out.read();
        }
    }
    
    transpose_512x512(temp2, output);
}
