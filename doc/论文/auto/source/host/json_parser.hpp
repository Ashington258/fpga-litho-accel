#ifndef HOST_JSON_PARSER_HPP
#define HOST_JSON_PARSER_HPP

#include <string>
#include <vector>
#include "mask.hpp"

// ============================================================================
// Source Types (matching litho.cpp common/source.hpp)
// ============================================================================
struct Annular {
    float innerRadius = 0.6f;
    float outerRadius = 0.9f;
};

struct Dipole {
    float radius = 0.5f;
    float offset = 0.3f;
    bool onXAxis = true;
};

struct CrossQuadrupole {
    float radius = 0.5f;
    float offset = 0.3f;
};

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

// ============================================================================
// Full Configuration Structure (matching litho.cpp FullConfig)
// ============================================================================
struct SOCSConfig {
    // Mask parameters
    int Lx = 0;
    int Ly = 0;
    int maskSizeX = 0;
    int maskSizeY = 0;
    std::string maskType;
    LineSpace lineSpace;
    std::string maskInputFile;
    float dose = 1.0f;
    
    // Source parameters (NEW)
    int srcSize = 101;          // gridSize for source matrix
    std::string srcType;        // Annular, Dipole, CrossQuadrupole, Point, Import
    Annular annular;
    Dipole dipole;
    CrossQuadrupole crossQuadrupole;
    Point point;
    std::string sourceInputFile;
    
    // Optics parameters (EXTENDED)
    float NA = 0.8f;
    float wavelength = 193.0f;  // NEW: default 193nm ArF
    float defocus = 0.0f;
    
    // Kernel parameters (EXTENDED)
    int nk = 10;
    float targetIntensity = 0.225f;  // NEW
    
    // Output parameters (NEW)
    std::string outputDir = "output/verification";
    
    // Computed parameters (NEW - dynamic calculation)
    int Nx = 0;                 // Will be computed: floor(NA * Lx * (1+outerSigma) / wavelength)
    int Ny = 0;
    float outerSigma = 0.0f;    // Computed from source
    
    // Verbosity
    int verboseLevel = 1;       // 0=quiet, 1=normal, 2=debug
};

bool loadConfig(const std::string& configPath, SOCSConfig& config);

#endif // HOST_JSON_PARSER_HPP
