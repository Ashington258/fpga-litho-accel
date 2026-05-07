/**
 * SOCS HLS Simple Implementation Header
 * FPGA-Litho Project
 * 
 * Uses unified configuration from socs_config.h
 * Dynamic Nx/Ny calculation based on optical parameters
 */

#ifndef SOCS_SIMPLE_H
#define SOCS_SIMPLE_H

#include "socs_config.h"  // Dynamic configuration
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <cmath>
#include <complex>

// Use types from socs_config.h
typedef std::complex<float> complex_float;

/**
 * Simplified SOCS calculation (without actual FFT for now)
 * This function loads data from AXI-MM and computes a placeholder output
 */
void calc_socs_simple_hls(
    float *mskf_r,      // AXI-MM: Mask spectrum real
    float *mskf_i,      // AXI-MM: Mask spectrum imaginary
    float *scales,      // AXI-MM: Eigenvalues
    float *krn_r,       // AXI-MM: Kernels real (nk×kerX×kerY)
    float *krn_i,       // AXI-MM: Kernels imaginary
    float *output       // AXI-MM: Output (convX×convY)
);

#endif // SOCS_SIMPLE_H