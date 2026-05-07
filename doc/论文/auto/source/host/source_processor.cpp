#include "source_processor.hpp"
#include "tcc_processor.hpp"  // For computeOuterSigma
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Source Pattern Creation Functions
// ============================================================================

void createAnnular(std::vector<float>& matrix, int matrixSize, const Annular& annular) {
    matrix.resize(matrixSize * matrixSize, 0.0f);
    int center = (matrixSize - 1) / 2;
    float r_inner = annular.innerRadius * center;
    float r_outer = annular.outerRadius * center;
    
    for (int y = 0; y < matrixSize; y++) {
        for (int x = 0; x < matrixSize; x++) {
            float dx = x - center;
            float dy = y - center;
            float r = std::sqrt(dx * dx + dy * dy);
            if (r >= r_inner && r <= r_outer) {
                matrix[y * matrixSize + x] = 1.0f;
            }
        }
    }
}

void createDipole(std::vector<float>& matrix, int matrixSize, const Dipole& dipole) {
    matrix.resize(matrixSize * matrixSize, 0.0f);
    int center = (matrixSize - 1) / 2;
    float radius = dipole.radius * center;
    float offset = dipole.offset * center;
    
    // Two poles on axis (x-axis or y-axis)
    if (dipole.onXAxis) {
        // Poles on x-axis at (center±offset, center)
        for (int y = 0; y < matrixSize; y++) {
            for (int x = 0; x < matrixSize; x++) {
                float dx1 = x - (center + offset);
                float dy = y - center;
                float r1 = std::sqrt(dx1 * dx1 + dy * dy);
                float dx2 = x - (center - offset);
                float r2 = std::sqrt(dx2 * dx2 + dy * dy);
                if (r1 <= radius || r2 <= radius) {
                    matrix[y * matrixSize + x] = 1.0f;
                }
            }
        }
    } else {
        // Poles on y-axis at (center, center±offset)
        for (int y = 0; y < matrixSize; y++) {
            for (int x = 0; x < matrixSize; x++) {
                float dx = x - center;
                float dy1 = y - (center + offset);
                float r1 = std::sqrt(dx * dx + dy1 * dy1);
                float dy2 = y - (center - offset);
                float r2 = std::sqrt(dx * dx + dy2 * dy2);
                if (r1 <= radius || r2 <= radius) {
                    matrix[y * matrixSize + x] = 1.0f;
                }
            }
        }
    }
}

void createCrossQuadrupole(std::vector<float>& matrix, int matrixSize, const CrossQuadrupole& crossQuadrupole) {
    matrix.resize(matrixSize * matrixSize, 0.0f);
    int center = (matrixSize - 1) / 2;
    float radius = crossQuadrupole.radius * center;
    float offset = crossQuadrupole.offset * center;
    
    // Four poles in cross pattern: (±offset, center) and (center, ±offset)
    for (int y = 0; y < matrixSize; y++) {
        for (int x = 0; x < matrixSize; x++) {
            // Check all four poles
            float dx1 = x - (center + offset);
            float dy1 = y - center;
            float r1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            
            float dx2 = x - (center - offset);
            float r2 = std::sqrt(dx2 * dx2 + dy1 * dy1);
            
            float dx3 = x - center;
            float dy3 = y - (center + offset);
            float r3 = std::sqrt(dx3 * dx3 + dy3 * dy3);
            
            float dy4 = y - (center - offset);
            float r4 = std::sqrt(dx3 * dx3 + dy4 * dy4);
            
            if (r1 <= radius || r2 <= radius || r3 <= radius || r4 <= radius) {
                matrix[y * matrixSize + x] = 1.0f;
            }
        }
    }
}

void createPoint(std::vector<float>& matrix, int matrixSize, const Point& point) {
    matrix.resize(matrixSize * matrixSize, 0.0f);
    int center = (matrixSize - 1) / 2;
    int px = center + static_cast<int>(point.x * center);
    int py = center + static_cast<int>(point.y * center);
    
    // Single point (usually at center for conventional source)
    if (px >= 0 && px < matrixSize && py >= 0 && py < matrixSize) {
        matrix[py * matrixSize + px] = 1.0f;
    }
}

void normalizeSource(std::vector<float>& matrix, int matrixSize) {
    double sum = 0.0;
    for (const auto& val : matrix) {
        sum += static_cast<double>(val);
    }
    if (sum > 0.0) {
        float normFactor = static_cast<float>(1.0 / sum);
        for (auto& val : matrix) {
            val *= normFactor;
        }
    }
}

// ============================================================================
// Source Processing Functions
// ============================================================================

bool createSource(std::vector<float>& src, SOCSConfig& config, std::stringstream& ss_log) {
    src.resize(config.srcSize * config.srcSize, 0.0f);
    
    ss_log << "[Source] Creating source pattern: type=" << config.srcType 
           << ", size=" << config.srcSize << std::endl;
    
    if (config.srcType == "Annular" || config.srcType == "annular") {
        createAnnular(src, config.srcSize, config.annular);
        ss_log << "[Source] Annular: inner=" << config.annular.innerRadius 
               << ", outer=" << config.annular.outerRadius << std::endl;
    }
    else if (config.srcType == "Dipole" || config.srcType == "dipole") {
        createDipole(src, config.srcSize, config.dipole);
        ss_log << "[Source] Dipole: radius=" << config.dipole.radius 
               << ", offset=" << config.dipole.offset 
               << ", onXAxis=" << config.dipole.onXAxis << std::endl;
    }
    else if (config.srcType == "CrossQuadrupole" || config.srcType == "crossQuadrupole") {
        createCrossQuadrupole(src, config.srcSize, config.crossQuadrupole);
        ss_log << "[Source] CrossQuadrupole: radius=" << config.crossQuadrupole.radius 
               << ", offset=" << config.crossQuadrupole.offset << std::endl;
    }
    else if (config.srcType == "Point" || config.srcType == "point") {
        createPoint(src, config.srcSize, config.point);
        ss_log << "[Source] Point: x=" << config.point.x 
               << ", y=" << config.point.y << std::endl;
    }
    else if (config.srcType == "Import" || config.srcType == "import") {
        // Load from file
        if (config.sourceInputFile.empty()) {
            ss_log << "[Source ERROR] Import type requires inputFile" << std::endl;
            return false;
        }
        
        std::ifstream ifs(config.sourceInputFile, std::ios::binary);
        if (!ifs.is_open()) {
            ss_log << "[Source ERROR] Cannot open source file: " << config.sourceInputFile << std::endl;
            return false;
        }
        
        // Read binary float array
        ifs.read(reinterpret_cast<char*>(src.data()), 
                 config.srcSize * config.srcSize * sizeof(float));
        if (!ifs) {
            ss_log << "[Source ERROR] Failed to read source data" << std::endl;
            return false;
        }
        ifs.close();
        ss_log << "[Source] Imported from: " << config.sourceInputFile << std::endl;
    }
    else {
        ss_log << "[Source ERROR] Unknown source type: " << config.srcType << std::endl;
        return false;
    }
    
    // Normalize to unit energy
    normalizeSource(src, config.srcSize);
    
    // Compute outer sigma
    config.outerSigma = computeOuterSigma(src, config.srcSize);
    ss_log << "[Source] OuterSigma computed: " << config.outerSigma << std::endl;
    
    // Print statistics
    printSourceStats(src, config.srcSize, ss_log);
    
    return true;
}

void computeNxNy(SOCSConfig& config) {
    // Formula from litho.cpp line 1296:
    // Nx = floor(NA * physLx * (1 + outerSigma) / wavelength)
    float physLx = static_cast<float>(config.Lx);
    float physLy = static_cast<float>(config.Ly);
    
    config.Nx = static_cast<int>(std::floor(
        config.NA * physLx * (1.0f + config.outerSigma) / config.wavelength));
    config.Ny = static_cast<int>(std::floor(
        config.NA * physLy * (1.0f + config.outerSigma) / config.wavelength));
    
    // Minimum value is 4 for meaningful FFT
    if (config.Nx < 4) config.Nx = 4;
    if (config.Ny < 4) config.Ny = 4;
}

void printSourceStats(const std::vector<float>& src, int srcSize, std::stringstream& ss_log) {
    double sum = 0.0;
    int nonzeroCount = 0;
    float maxVal = 0.0f;
    float minVal = 0.0f;
    
    for (const auto& val : src) {
        sum += static_cast<double>(val);
        if (val != 0.0f) nonzeroCount++;
        if (val > maxVal) maxVal = val;
        if (val < minVal) minVal = val;
    }
    
    ss_log << "[Source Statistics]" << std::endl;
    ss_log << "  Total energy (sum): " << sum << std::endl;
    ss_log << "  Non-zero pixels: " << nonzeroCount << "/" << (srcSize * srcSize) << std::endl;
    ss_log << "  Max value: " << maxVal << std::endl;
    ss_log << "  Min value: " << minVal << std::endl;
}