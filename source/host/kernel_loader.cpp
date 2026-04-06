#include "kernel_loader.hpp"
#include "file_io.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

bool readKernelInfo(const std::string& filename, int& krnSizeX, int& krnSizeY, int& nk, std::vector<float>& scales) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Error: Failed to open kernel info file: " << filename << std::endl;
        return false;
    }

    std::string line;
    krnSizeX = krnSizeY = 0;
    nk = 0;
    scales.clear();

    while (std::getline(ifs, line)) {
        if (line.find("Kernel Size:") != std::string::npos) {
            int x = 0, y = 0;
            if (std::sscanf(line.c_str(), "%*[- ]Kernel Size: %dx%d", &x, &y) == 2 ||
                std::sscanf(line.c_str(), "Kernel Size: %dx%d", &x, &y) == 2) {
                krnSizeX = x;
                krnSizeY = y;
            }
        } else if (line.find("Number of Kernels:") != std::string::npos) {
            int count = 0;
            if (std::sscanf(line.c_str(), "%*[- ]Number of Kernels: %d", &count) == 1 ||
                std::sscanf(line.c_str(), "Number of Kernels: %d", &count) == 1) {
                nk = count;
            }
        } else if (line.find("Eigenvalue") != std::string::npos) {
            float value = 0.0f;
            if (std::sscanf(line.c_str(), "%*[- ]Eigenvalue %*d: %f", &value) == 1 ||
                std::sscanf(line.c_str(), "Eigenvalue %*d: %f", &value) == 1) {
                scales.push_back(value);
            }
        }
    }

    ifs.close();
    if (krnSizeX <= 0 || krnSizeY <= 0 || nk <= 0) {
        std::cerr << "Error: Invalid kernel metadata in " << filename << std::endl;
        return false;
    }
    if (static_cast<int>(scales.size()) != nk) {
        std::cerr << "Warning: Number of scales parsed (" << scales.size() << ") does not match nk (" << nk << ")." << std::endl;
        if (static_cast<int>(scales.size()) < nk) {
            return false;
        }
    }
    return true;
}

bool loadKernels(const std::string& kernelDir, int nk, int krnSizeX, int krnSizeY,
                 std::vector<std::vector<std::complex<float>>>& krns) {
    if (nk <= 0 || krnSizeX <= 0 || krnSizeY <= 0) {
        std::cerr << "Error: Invalid kernel dimensions or count." << std::endl;
        return false;
    }

    size_t krnSize = static_cast<size_t>(krnSizeX) * static_cast<size_t>(krnSizeY);
    krns.resize(nk);

    for (int i = 0; i < nk; ++i) {
        std::ostringstream realName;
        std::ostringstream imagName;
        realName << kernelDir << "/krn_" << i << "_r.bin";
        imagName << kernelDir << "/krn_" << i << "_i.bin";
        std::vector<std::complex<float>> data;
        readComplexArrayFromBinary(realName.str(), imagName.str(), data, krnSize);
        if (static_cast<int>(data.size()) != static_cast<int>(krnSize)) {
            std::cerr << "Error: Kernel file size mismatch for kernel " << i << std::endl;
            return false;
        }
        krns[i] = std::move(data);
    }

    return true;
}
