#include "file_io.hpp"
#include "json_parser.hpp"
#include "kernel_loader.hpp"
#include "mask.hpp"
#include "mask_processor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string resolvePath(const std::string& baseDir, const std::string& relativePath) {
    fs::path path(relativePath);
    if (path.is_absolute()) {
        return path.string();
    }
    return (fs::path(baseDir) / path).lexically_normal().string();
}

static bool copyFile(const std::string& source, const std::string& destination) {
    std::error_code ec;
    fs::create_directories(fs::path(destination).parent_path(), ec);
    if (ec) {
        std::cerr << "Error: Failed to create destination directory: " << ec.message() << std::endl;
        return false;
    }
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Error: Failed to copy file from " << source << " to " << destination << ": " << ec.message() << std::endl;
        return false;
    }
    return true;
}

static void displayHelp(const std::string& programName) {
    std::cout << "Usage: " << programName << " <config.json> <kernel_dir> [output_dir]" << std::endl;
    std::cout << "  config.json   - Path to input configuration file." << std::endl;
    std::cout << "  kernel_dir    - Directory containing kernel_info.txt and krn_*_r.bin/krn_*_i.bin." << std::endl;
    std::cout << "  output_dir    - Optional output directory (default: output)." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        displayHelp(argv[0]);
        return 1;
    }

    const std::string configPath = argv[1];
    const std::string kernelDir = argv[2];
    const std::string outputDir = (argc > 3 ? argv[3] : "output");
    const std::string configDir = fs::path(configPath).parent_path().string();
    const std::string projectRoot = fs::path(configDir).parent_path().parent_path().string();

    SOCSConfig config;
    if (!loadConfig(configPath, config)) {
        return 1;
    }

    if (config.Lx <= 0 || config.Ly <= 0) {
        std::cerr << "Error: Invalid output dimensions in config." << std::endl;
        return 1;
    }
    if (config.maskSizeX <= 0 || config.maskSizeY <= 0) {
        std::cerr << "Error: Invalid mask dimensions in config." << std::endl;
        return 1;
    }
    if (config.nk <= 0) {
        std::cerr << "Error: Invalid kernel count in config." << std::endl;
        return 1;
    }

    std::string maskInputFile = resolvePath(configDir, config.maskInputFile);
    if (!fs::exists(maskInputFile)) {
        maskInputFile = resolvePath(projectRoot, config.maskInputFile);
    }
    std::string kernelInfoFile = fs::path(kernelDir) / "kernel_info.txt";

    ensureOutputDirs(outputDir);
    fs::path outputKernelDir = fs::path(outputDir) / "kernels";
    fs::create_directories(outputKernelDir);

    std::vector<float> mask(config.Lx * config.Ly, 0.0f);
    std::stringstream ssMask;
    createMask(mask, config.Lx, config.Ly, config.maskSizeX, config.maskSizeY,
               config.maskType, config.lineSpace, maskInputFile, ssMask, config.dose);

    std::vector<std::complex<float>> maskFreq;
    if (!fftMask(mask, maskFreq, config.Lx, config.Ly)) {
        return 1;
    }

    const std::string maskRealFile = fs::path(outputDir) / "mskf_r.bin";
    const std::string maskImagFile = fs::path(outputDir) / "mskf_i.bin";
    writeComplexArrayToBinary(maskRealFile, maskImagFile, maskFreq);

    int krnSizeX = 0;
    int krnSizeY = 0;
    int nkFromInfo = 0;
    std::vector<float> scales;
    if (!readKernelInfo(kernelInfoFile, krnSizeX, krnSizeY, nkFromInfo, scales)) {
        return 1;
    }

    if (config.nk != nkFromInfo) {
        std::cout << "Warning: kernel.count (" << config.nk << ") does not match Number of Kernels in kernel_info.txt (" << nkFromInfo << ")." << std::endl;
    }

    std::vector<std::vector<std::complex<float>>> kernels;
    if (!loadKernels(kernelDir, nkFromInfo, krnSizeX, krnSizeY, kernels)) {
        return 1;
    }

    const std::string scalesFile = fs::path(outputDir) / "scales.bin";
    writeFloatArrayToBinary(scalesFile, scales);

    const std::string outputKernelInfo = outputKernelDir / "kernel_info.txt";
    if (!copyFile(kernelInfoFile, outputKernelInfo)) {
        return 1;
    }

    for (int i = 0; i < nkFromInfo; ++i) {
        std::string srcReal = fs::path(kernelDir) / ("krn_" + std::to_string(i) + "_r.bin");
        std::string srcImag = fs::path(kernelDir) / ("krn_" + std::to_string(i) + "_i.bin");
        std::string dstReal = outputKernelDir / ("krn_" + std::to_string(i) + "_r.bin");
        std::string dstImag = outputKernelDir / ("krn_" + std::to_string(i) + "_i.bin");
        if (!copyFile(srcReal, dstReal) || !copyFile(srcImag, dstImag)) {
            return 1;
        }
    }

    std::cout << "SOCS host preprocessing completed successfully." << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
    std::cout << "Mask FFT output: " << maskRealFile << " and " << maskImagFile << std::endl;
    std::cout << "Scales output: " << scalesFile << std::endl;
    std::cout << "Kernel files copied to: " << outputKernelDir << std::endl;
    std::cout << std::endl;
    std::cout << "Mask parameters:\n" << ssMask.str() << std::endl;
    std::cout << "Kernel metadata: " << krnSizeX << " x " << krnSizeY << ", nk=" << nkFromInfo << std::endl;

    if (!maskFreq.empty()) {
        int centerY = config.Ly / 2;
        int centerX = config.Lx / 2;
        std::cout << "Mask FFT center samples:" << std::endl;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int x = centerX + dx;
                int y = centerY + dy;
                if (x < 0 || x >= config.Lx || y < 0 || y >= config.Ly) {
                    continue;
                }
                auto c = maskFreq[y * config.Lx + x];
                std::cout << "  [" << y << "," << x << "] = (" << c.real() << ", " << c.imag() << ")" << std::endl;
            }
        }
    }

    return 0;
}
