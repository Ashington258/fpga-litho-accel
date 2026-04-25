/**
 * @file socs_2048_ddr_optimized.h
 * @brief Header file for DDR-optimized SOCS HLS implementation
 */

#ifndef SOCS_2048_DDR_OPTIMIZED_H
#define SOCS_2048_DDR_OPTIMIZED_H

#include "socs_2048.h"

// Main function with DDR access optimization
void calc_socs_2048_hls_ddr_optimized(
    float* mskf_r, float* mskf_i,
    float* krn_r, float* krn_i,
    float* scales,
    float* output,
    int nk,
    int nx_actual,
    int ny_actual,
    int Lx,
    int Ly
);

#endif // SOCS_2048_DDR_OPTIMIZED_H
