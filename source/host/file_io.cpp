#include "file_io.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

void createOutputDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        std::cerr << "Error: Failed to create directory: " << path << " (" << ec.message() << ")" << std::endl;
        std::exit(1);
    }
}

void ensureOutputDirs(const std::string& outputDir) {
    createOutputDir(outputDir);
    createOutputDir(outputDir + "/kernels");
}

void writeFloatArrayToBinary(const std::string& filename, const std::vector<float>& data) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << filename << std::endl;
        std::exit(1);
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));
    if (!ofs) {
        std::cerr << "Error: Failed to write data to file: " << filename << std::endl;
        std::exit(1);
    }
    ofs.close();
}

void writeComplexArrayToBinary(const std::string& realFile, const std::string& imagFile, const std::vector<std::complex<float>>& data) {
    std::vector<float> real(data.size());
    std::vector<float> imag(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        real[i] = data[i].real();
        imag[i] = data[i].imag();
    }
    writeFloatArrayToBinary(realFile, real);
    writeFloatArrayToBinary(imagFile, imag);
}

void readFloatArrayFromBinary(const std::string& filename, std::vector<float>& data, size_t size) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Error: Failed to open file for reading: " << filename << std::endl;
        std::exit(1);
    }
    data.resize(size);
    ifs.read(reinterpret_cast<char*>(data.data()), size * sizeof(float));
    if (!ifs) {
        std::cerr << "Error: Failed to read data from file: " << filename << std::endl;
        std::exit(1);
    }
    ifs.close();
}

void readComplexArrayFromBinary(const std::string& realFile, const std::string& imagFile, std::vector<std::complex<float>>& data, size_t size) {
    std::vector<float> real;
    std::vector<float> imag;
    readFloatArrayFromBinary(realFile, real, size);
    readFloatArrayFromBinary(imagFile, imag, size);
    data.resize(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = std::complex<float>(real[i], imag[i]);
    }
}
