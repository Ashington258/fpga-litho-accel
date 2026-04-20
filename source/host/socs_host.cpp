/**
 * SOCS Host - Preprocessing Pipeline for FPGA-Litho (方案 B: Host + FPGA)
 * 
 * Architecture:
 *   MODULE 1: JSON Parser → Load configuration
 *   MODULE 2: Mask Processor → Create mask, compute FFT
 *   MODULE 3: TCC Calculator → Compute transmission cross coefficient
 *   MODULE 4: Kernel Extractor → Eigenvalue decomposition of TCC
 *   MODULE 5: Source Term → Prepare SOCS source weights
 *   MODULE 6: Data Preparation → Format FPGA input (mskf, krn, scales)
 *   [MODULE 7: FPGA calcSOCS → Core convolution (offloaded to FPGA)]
 *   MODULE 8: IFFT Post-processing → Convert frequency to spatial domain
 * 
 * Usage modes:
 *   --load-kernels: Load precomputed kernels from directory (legacy mode)
 *   --compute-kernels: Compute kernels from TCC in real-time (new mode)
 */

#include "file_io.hpp"
#include "json_parser.hpp"
#include "kernel_loader.hpp"
#include "mask.hpp"
#include "mask_processor.hpp"
#include "tcc_processor.hpp"
#include "kernel_extractor.hpp"
#include "source_processor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <complex>
#include <cmath>

namespace fs = std::filesystem;

using ComplexD = std::complex<double>;
using ComplexF = std::complex<float>;

// === Utility Functions ===

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
    std::cout << "SOCS Host - Preprocessing Pipeline for FPGA-Litho" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage modes:" << std::endl;
    std::cout << std::endl;
    std::cout << "Mode 1: Load precomputed kernels (legacy)" << std::endl;
    std::cout << "  " << programName << " --load-kernels <config.json> <kernel_dir> [output_dir]" << std::endl;
    std::cout << "  config.json   - Path to input configuration file." << std::endl;
    std::cout << "  kernel_dir    - Directory containing kernel_info.txt and krn_*_r.bin/krn_*_i.bin." << std::endl;
    std::cout << "  output_dir    - Optional output directory (default: output)." << std::endl;
    std::cout << std::endl;
    std::cout << "Mode 2: Compute kernels from TCC (new)" << std::endl;
    std::cout << "  " << programName << " --compute-kernels <config.json> [output_dir]" << std::endl;
    std::cout << "  config.json   - Path to input configuration file (must include source params)." << std::endl;
    std::cout << "  output_dir    - Optional output directory (default: output)." << std::endl;
    std::cout << std::endl;
    std::cout << "Mode 3: Full pipeline (mask → TCC → kernels → FPGA input)" << std::endl;
    std::cout << "  " << programName << " --full <config.json> [output_dir]" << std::endl;
}

// === Mode 1: Load Precomputed Kernels ===

static int runLoadKernelsMode(int argc, char* argv[]) {
    if (argc < 4) {
        displayHelp(argv[0]);
        return 1;
    }

    const std::string configPath = argv[2];
    const std::string kernelDir = argv[3];
    const std::string outputDir = (argc > 4 ? argv[4] : "output");
    const std::string configDir = fs::path(configPath).parent_path().string();
    const std::string projectRoot = fs::path(configDir).parent_path().parent_path().string();

    std::cout << "[Mode 1] Loading precomputed kernels..." << std::endl;

    SOCSConfig config;
    if (!loadConfig(configPath, config)) {
        return 1;
    }

    std::string maskInputFile = resolvePath(configDir, config.maskInputFile);
    if (!fs::exists(maskInputFile)) {
        maskInputFile = resolvePath(projectRoot, config.maskInputFile);
    }
    std::string kernelInfoFile = (fs::path(kernelDir) / "kernel_info.txt").string();

    ensureOutputDirs(outputDir);
    fs::path outputKernelDir = fs::path(outputDir) / "kernels";
    fs::create_directories(outputKernelDir);

    // MODULE 2: Mask + FFT
    std::vector<float> mask(config.Lx * config.Ly, 0.0f);
    std::stringstream ssMask;
    createMask(mask, config.Lx, config.Ly, config.maskSizeX, config.maskSizeY,
               config.maskType, config.lineSpace, maskInputFile, ssMask, config.dose);

    std::vector<ComplexF> maskFreq;
    if (!fftMask(mask, maskFreq, config.Lx, config.Ly)) {
        return 1;
    }

    const std::string maskRealFile = (fs::path(outputDir) / "mskf_r.bin").string();
    const std::string maskImagFile = (fs::path(outputDir) / "mskf_i.bin").string();
    writeComplexArrayToBinary(maskRealFile, maskImagFile, maskFreq);

    // Load kernels from directory
    int krnSizeX = 0, krnSizeY = 0, nkFromInfo = 0;
    std::vector<float> scales;
    if (!readKernelInfo(kernelInfoFile, krnSizeX, krnSizeY, nkFromInfo, scales)) {
        return 1;
    }

    std::vector<std::vector<ComplexF>> kernels;
    if (!loadKernels(kernelDir, nkFromInfo, krnSizeX, krnSizeY, kernels)) {
        return 1;
    }

    const std::string scalesFile = (fs::path(outputDir) / "scales.bin").string();
    writeFloatArrayToBinary(scalesFile, scales);

    // Copy kernel files
    const std::string outputKernelInfo = (outputKernelDir / "kernel_info.txt").string();
    copyFile(kernelInfoFile, outputKernelInfo);

    for (int i = 0; i < nkFromInfo; ++i) {
        std::string srcReal = (fs::path(kernelDir) / ("krn_" + std::to_string(i) + "_r.bin")).string();
        std::string srcImag = (fs::path(kernelDir) / ("krn_" + std::to_string(i) + "_i.bin")).string();
        std::string dstReal = (outputKernelDir / ("krn_" + std::to_string(i) + "_r.bin")).string();
        std::string dstImag = (outputKernelDir / ("krn_" + std::to_string(i) + "_i.bin")).string();
        copyFile(srcReal, dstReal);
        copyFile(srcImag, dstImag);
    }

    std::cout << "[Mode 1] Completed successfully." << std::endl;
    std::cout << "  Mask FFT: " << maskRealFile << std::endl;
    std::cout << "  Scales: " << scalesFile << std::endl;
    std::cout << "  Kernels: " << outputKernelDir << std::endl;

    return 0;
}

// === Mode 2: Compute Kernels from TCC ===

static int runComputeKernelsMode(int argc, char* argv[]) {
    if (argc < 3) {
        displayHelp(argv[0]);
        return 1;
    }

    const std::string configPath = argv[2];
    const std::string outputDir = (argc > 3 ? argv[3] : "output");
    const std::string configDir = fs::path(configPath).parent_path().string();

    std::cout << "[Mode 2] Computing kernels from TCC..." << std::endl;

    SOCSConfig config;
    if (!loadConfig(configPath, config)) {
        return 1;
    }

    ensureOutputDirs(outputDir);
    fs::path outputKernelDir = fs::path(outputDir) / "kernels";
    fs::create_directories(outputKernelDir);

    // MODULE 5: Source Term (generate source points)
    std::vector<float> source;
    std::stringstream ssSource;
    if (!createSource(source, config, ssSource)) {
        std::cerr << "Error: Failed to generate source points." << std::endl;
        return 1;
    }

    std::cout << ssSource.str();

    // Compute Nx/Ny dynamically from optical parameters
    computeNxNy(config);
    std::cout << "  Nx/Ny computed: " << config.Nx << " / " << config.Ny << std::endl;

    // MODULE 3: TCC Calculator
    std::cout << std::endl;
    std::cout << "=== MODULE 3: TCC Calculation ===" << std::endl;

    float outerSigma = config.outerSigma;
    if (outerSigma <= 0) {
        outerSigma = config.outerSigma;  // fallback to config value
    }

    int tccSize = (2 * config.Nx + 1) * (2 * config.Ny + 1);
    std::vector<ComplexD> tcc(tccSize * tccSize);
    
    std::cout << "  Computing TCC matrix (size: " << tccSize << " x " << tccSize << ")..." << std::endl;
    
    calcTCC(tcc, tccSize, source, static_cast<int>(source.size()), outerSigma, config, 1);

    // MODULE 4: Kernel Extractor
    std::vector<std::vector<ComplexD>> kernelsDouble;
    std::vector<float> scales;
    
    std::cout << "  Extracting " << config.nk << " kernels via eigenvalue decomposition..." << std::endl;
    
    if (extractKernels(kernelsDouble, scales, config.nk, tcc, tccSize, config.Nx, config.Ny, 1) != 0) {
        std::cerr << "Error: Kernel extraction failed." << std::endl;
        return 1;
    }

    // Convert kernels to float format for FPGA
    int krnSizeX = 2 * config.Nx + 1;
    int krnSizeY = 2 * config.Ny + 1;
    std::vector<std::vector<ComplexF>> kernelsFloat(config.nk);
    for (int k = 0; k < config.nk; k++) {
        kernelsFloat[k].resize(krnSizeX * krnSizeY);
        for (int i = 0; i < krnSizeX * krnSizeY; i++) {
            kernelsFloat[k][i] = ComplexF(static_cast<float>(kernelsDouble[k][i].real()),
                                           static_cast<float>(kernelsDouble[k][i].imag()));
        }
    }

    // Write outputs
    const std::string scalesFile = (fs::path(outputDir) / "scales.bin").string();
    writeFloatArrayToBinary(scalesFile, scales);

    // Write kernel_info.txt
    const std::string kernelInfoFile = (outputKernelDir / "kernel_info.txt").string();
    std::ofstream infoStream(kernelInfoFile);
    infoStream << "Kernel Size X: " << krnSizeX << "\n";
    infoStream << "Kernel Size Y: " << krnSizeY << "\n";
    infoStream << "Number of Kernels: " << config.nk << "\n";
    infoStream << "Scales:\n";
    for (int k = 0; k < config.nk; k++) {
        infoStream << "  " << k << ": " << scales[k] << "\n";
    }
    infoStream.close();

    // Write kernel binary files
    for (int k = 0; k < config.nk; k++) {
        std::string krnRealFile = (outputKernelDir / ("krn_" + std::to_string(k) + "_r.bin")).string();
        std::string krnImagFile = (outputKernelDir / ("krn_" + std::to_string(k) + "_i.bin")).string();
        writeComplexArrayToBinary(krnRealFile, krnImagFile, kernelsFloat[k]);
    }

    std::cout << "[Mode 2] Completed successfully." << std::endl;
    std::cout << "  TCC size: " << tccSize << " x " << tccSize << std::endl;
    std::cout << "  Kernels: " << config.nk << " x " << krnSizeX << " x " << krnSizeY << std::endl;
    std::cout << "  Scales: " << scalesFile << std::endl;
    std::cout << "  Output: " << outputKernelDir << std::endl;

    return 0;
}

// === Mode 3: Full Pipeline ===

static int runFullPipelineMode(int argc, char* argv[]) {
    if (argc < 3) {
        displayHelp(argv[0]);
        return 1;
    }

    const std::string configPath = argv[2];
    const std::string outputDir = (argc > 3 ? argv[3] : "output");
    const std::string configDir = fs::path(configPath).parent_path().string();
    const std::string projectRoot = fs::path(configDir).parent_path().parent_path().string();

    std::cout << "[Mode 3] Running full preprocessing pipeline..." << std::endl;

    // MODULE 1: JSON Parser
    SOCSConfig config;
    if (!loadConfig(configPath, config)) {
        return 1;
    }

    ensureOutputDirs(outputDir);
    fs::path outputKernelDir = fs::path(outputDir) / "kernels";
    fs::create_directories(outputKernelDir);

    std::cout << std::endl;
    std::cout << "=== MODULE 1: Configuration ===" << std::endl;
    std::cout << "  Lx=" << config.Lx << ", Ly=" << config.Ly << std::endl;
    std::cout << "  Nx=" << config.Nx << ", Ny=" << config.Ny << std::endl;
    std::cout << "  nk=" << config.nk << std::endl;
    std::cout << "  NA=" << config.NA << ", wavelength=" << config.wavelength << " nm" << std::endl;
    std::cout << "  defocus=" << config.defocus << " nm" << std::endl;

    // MODULE 2: Mask + FFT
    std::cout << std::endl;
    std::cout << "=== MODULE 2: Mask Processing ===" << std::endl;

    std::string maskInputFile = resolvePath(configDir, config.maskInputFile);
    if (!fs::exists(maskInputFile)) {
        maskInputFile = resolvePath(projectRoot, config.maskInputFile);
    }

    std::vector<float> mask(config.Lx * config.Ly, 0.0f);
    std::stringstream ssMask;
    createMask(mask, config.Lx, config.Ly, config.maskSizeX, config.maskSizeY,
               config.maskType, config.lineSpace, maskInputFile, ssMask, config.dose);

    std::cout << ssMask.str();

    std::vector<ComplexF> maskFreq;
    if (!fftMask(mask, maskFreq, config.Lx, config.Ly)) {
        return 1;
    }

    const std::string maskRealFile = (fs::path(outputDir) / "mskf_r.bin").string();
    const std::string maskImagFile = (fs::path(outputDir) / "mskf_i.bin").string();
    writeComplexArrayToBinary(maskRealFile, maskImagFile, maskFreq);

    std::cout << "  Mask FFT output: " << maskFreq.size() << " complex values" << std::endl;

    // MODULE 5: Source Term
    std::cout << std::endl;
    std::cout << "=== MODULE 5: Source Generation ===" << std::endl;

    std::vector<float> source;
    std::stringstream ssSource;
    if (!createSource(source, config, ssSource)) {
        std::cerr << "Error: Failed to generate source points." << std::endl;
        return 1;
    }

    std::cout << ssSource.str();

    // Compute Nx/Ny dynamically from optical parameters
    computeNxNy(config);
    std::cout << "  Nx/Ny computed: " << config.Nx << " / " << config.Ny << std::endl;

    // MODULE 3: TCC Calculator
    std::cout << std::endl;
    std::cout << "=== MODULE 3: TCC Calculation ===" << std::endl;

    float outerSigma = config.outerSigma;

    int tccSize = (2 * config.Nx + 1) * (2 * config.Ny + 1);
    std::vector<ComplexD> tcc(tccSize * tccSize);

    std::cout << "  TCC matrix size: " << tccSize << " x " << tccSize << std::endl;
    std::cout << "  outerSigma: " << outerSigma << std::endl;

    calcTCC(tcc, tccSize, source, static_cast<int>(source.size()), outerSigma, config, 1);

    std::cout << "  TCC computation completed." << std::endl;

    // MODULE 4: Kernel Extractor
    std::cout << std::endl;
    std::cout << "=== MODULE 4: Kernel Extraction ===" << std::endl;

    std::vector<std::vector<ComplexD>> kernelsDouble;
    std::vector<float> scales;

    if (extractKernels(kernelsDouble, scales, config.nk, tcc, tccSize, config.Nx, config.Ny, 1) != 0) {
        std::cerr << "Error: Kernel extraction failed." << std::endl;
        return 1;
    }

    // Convert kernels to float format
    int krnSizeX = 2 * config.Nx + 1;
    int krnSizeY = 2 * config.Ny + 1;
    std::vector<std::vector<ComplexF>> kernelsFloat(config.nk);
    for (int k = 0; k < config.nk; k++) {
        kernelsFloat[k].resize(krnSizeX * krnSizeY);
        for (int i = 0; i < krnSizeX * krnSizeY; i++) {
            kernelsFloat[k][i] = ComplexF(static_cast<float>(kernelsDouble[k][i].real()),
                                           static_cast<float>(kernelsDouble[k][i].imag()));
        }
    }

    // MODULE 6: Data Preparation
    std::cout << std::endl;
    std::cout << "=== MODULE 6: Data Preparation ===" << std::endl;

    const std::string scalesFile = (fs::path(outputDir) / "scales.bin").string();
    writeFloatArrayToBinary(scalesFile, scales);

    // Write kernel_info.txt
    const std::string kernelInfoFile = (outputKernelDir / "kernel_info.txt").string();
    std::ofstream infoStream(kernelInfoFile);
    infoStream << "Kernel Size X: " << krnSizeX << "\n";
    infoStream << "Kernel Size Y: " << krnSizeY << "\n";
    infoStream << "Number of Kernels: " << config.nk << "\n";
    infoStream << "Scales:\n";
    for (int k = 0; k < config.nk; k++) {
        infoStream << "  " << k << ": " << scales[k] << "\n";
    }
    infoStream.close();

    // Write kernel binary files
    for (int k = 0; k < config.nk; k++) {
        std::string krnRealFile = (outputKernelDir / ("krn_" + std::to_string(k) + "_r.bin")).string();
        std::string krnImagFile = (outputKernelDir / ("krn_" + std::to_string(k) + "_i.bin")).string();
        writeComplexArrayToBinary(krnRealFile, krnImagFile, kernelsFloat[k]);
    }

    std::cout << "  Scales written: " << scalesFile << std::endl;
    std::cout << "  Kernels written: " << config.nk << " files to " << outputKernelDir << std::endl;

    // Summary
    std::cout << std::endl;
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "[Mode 3] Full pipeline completed successfully." << std::endl;
    std::cout << "  Output directory: " << outputDir << std::endl;
    std::cout << "  Mask FFT: " << maskRealFile << ", " << maskImagFile << std::endl;
    std::cout << "  Scales: " << scalesFile << std::endl;
    std::cout << "  Kernels: " << outputKernelDir << std::endl;
    std::cout << std::endl;
    std::cout << "Next step: Transfer data to FPGA for calcSOCS computation (MODULE 7)." << std::endl;

    return 0;
}

// === Main Entry ===

int main(int argc, char* argv[]) {
    if (argc < 2) {
        displayHelp(argv[0]);
        return 1;
    }

    const std::string mode = argv[1];

    if (mode == "--load-kernels") {
        return runLoadKernelsMode(argc, argv);
    } else if (mode == "--compute-kernels") {
        return runComputeKernelsMode(argc, argv);
    } else if (mode == "--full") {
        return runFullPipelineMode(argc, argv);
    } else if (mode == "--help" || mode == "-h") {
        displayHelp(argv[0]);
        return 0;
    } else {
        // Legacy mode: treat as --load-kernels with old arguments
        std::cout << "Note: Using legacy argument format. Consider using --load-kernels explicitly." << std::endl;
        // Shift arguments for legacy mode
        std::vector<char*> newArgs;
        newArgs.push_back(argv[0]);
        newArgs.push_back(const_cast<char*>("--load-kernels"));
        for (int i = 1; i < argc; i++) {
            newArgs.push_back(argv[i]);
        }
        return runLoadKernelsMode(static_cast<int>(newArgs.size()), newArgs.data());
    }
}
