#ifndef HOST_KERNEL_LOADER_HPP
#define HOST_KERNEL_LOADER_HPP

#include <complex>
#include <string>
#include <vector>

bool readKernelInfo(const std::string& filename, int& krnSizeX, int& krnSizeY, int& nk, std::vector<float>& scales);
bool loadKernels(const std::string& kernelDir, int nk, int krnSizeX, int krnSizeY,
                 std::vector<std::vector<std::complex<float>>>& krns);

#endif // HOST_KERNEL_LOADER_HPP
