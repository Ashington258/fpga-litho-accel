/**
 * SOCS HLS Simple Implementation Header
 * FPGA-Litho Project
 */

#ifndef SOCS_SIMPLE_H
#define SOCS_SIMPLE_H

#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <cmath>
#include <complex>

// Configuration parameters (Nx=4, based on current config)
#define Lx 512
#define Ly 512
#define Nx 4
#define Ny 4
#define convX (4*Nx + 1)  // = 17
#define convY (4*Ny + 1)  // = 17
#define fftConvX 32       // nextPowerOfTwo(17)
#define fftConvY 32
#define kerX (2*Nx + 1)   // = 9
#define kerY (2*Ny + 1)   // = 9
#define nk 10

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