#ifndef HOST_MASK_PROCESSOR_HPP
#define HOST_MASK_PROCESSOR_HPP

#include <complex>
#include <vector>

bool fftMask(const std::vector<float>& mask, std::vector<std::complex<float>>& maskFreq, int sizeX, int sizeY);

#endif // HOST_MASK_PROCESSOR_HPP
