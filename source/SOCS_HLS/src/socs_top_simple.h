/*
 * SOCS HLS 函数声明（简化版本）
 * FPGA-Litho Project
 */

#ifndef SOCS_TOP_SIMPLE_H
#define SOCS_TOP_SIMPLE_H

#include "data_types.h"

// ============================================================================
// 函数声明
// ============================================================================

// fftshift函数
void my_shift_2d(
    data_t* in,
    data_t* out,
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
);

// calcSOCS简化版本
void calc_socs_simple(
    cmpxData_t* mskf,
    cmpxData_t* krns,
    data_t* scales,
    data_t* image,
    int Lx,
    int Ly,
    int Nx,
    int Ny,
    int nk
);

#endif