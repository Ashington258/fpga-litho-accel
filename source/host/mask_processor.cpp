#include "mask_processor.hpp"
#include <complex>
#include <vector>
#include <iostream>
#include <fftw3.h>

// Float version of myShift (same as Golden litho.cpp)
static void myShift(const std::vector<float>& in, std::vector<float>& out, int sizeX, int sizeY, bool shiftTypeX, bool shiftTypeY) {
    int xh = shiftTypeX ? sizeX / 2 : (sizeX + 1) / 2;
    int yh = shiftTypeY ? sizeY / 2 : (sizeY + 1) / 2;
    out.resize(sizeX * sizeY);
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            int sx = (x + xh) % sizeX;
            int sy = (y + yh) % sizeY;
            out[sy * sizeX + sx] = in[y * sizeX + x];
        }
    }
}

// Complex version of myShift for FFT output (fftshift)
static void myShiftComplex(const std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int sizeX, int sizeY, bool shiftTypeX, bool shiftTypeY) {
    int xh = shiftTypeX ? sizeX / 2 : (sizeX + 1) / 2;
    int yh = shiftTypeY ? sizeY / 2 : (sizeY + 1) / 2;
    out.resize(sizeX * sizeY);
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            int sx = (x + xh) % sizeX;
            int sy = (y + yh) % sizeY;
            out[sy * sizeX + sx] = in[y * sizeX + x];
        }
    }
}

bool fftMask(const std::vector<float>& mask, std::vector<std::complex<float>>& maskFreq, int sizeX, int sizeY) {
    if (static_cast<int>(mask.size()) != sizeX * sizeY) {
        std::cerr << "Error: fftMask expected mask size " << (sizeX * sizeY) << " but got " << mask.size() << std::endl;
        return false;
    }

    std::vector<float> shifted;
    myShift(mask, shifted, sizeX, sizeY, false, false);

    std::vector<fftwf_complex> data(sizeX * sizeY);
    for (int i = 0; i < sizeX * sizeY; ++i) {
        data[i][0] = shifted[i];
        data[i][1] = 0.0f;
    }

    fftwf_plan plan = fftwf_plan_dft_2d(sizeY, sizeX, data.data(), data.data(), FFTW_FORWARD, FFTW_ESTIMATE);
    if (!plan) {
        std::cerr << "Error: fftwf_plan_dft_2d failed." << std::endl;
        return false;
    }
    fftwf_execute(plan);
    fftwf_destroy_plan(plan);

    // Apply FFT
    float scale = 1.0f / static_cast<float>(sizeX * sizeY);
    std::vector<std::complex<float>> fftResult(sizeX * sizeY);
    for (int i = 0; i < sizeX * sizeY; ++i) {
        fftResult[i] = std::complex<float>(data[i][0] * scale, data[i][1] * scale);
    }
    
    // Apply fftshift to move DC component to center (match Golden litho.cpp behavior)
    myShiftComplex(fftResult, maskFreq, sizeX, sizeY, false, false);
    
    return true;
}
