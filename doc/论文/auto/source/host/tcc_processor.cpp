#include "tcc_processor.hpp"
#include <cmath>
#include <iostream>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using ComplexD = std::complex<double>;

/**
 * Compute outer sigma from source matrix
 * Returns the maximum normalized distance from center to any non-zero source point
 */
float computeOuterSigma(const std::vector<float>& src, int srcSize) {
    int center = (srcSize - 1) / 2;
    float max_distance = 0.0f;
    
    for (int y = 0; y < srcSize; y++) {
        for (int x = 0; x < srcSize; x++) {
            if (src[y * srcSize + x] != 0.0f) {
                float distance = sqrt((y - center) * (y - center) + (x - center) * (x - center));
                if (distance > max_distance) {
                    max_distance = distance;
                }
            }
        }
    }
    
    return (center == 0) ? 0.0f : (max_distance / center);
}

/**
 * Calculate TCC matrix
 * 
 * The TCC (Transmission Cross Coefficient) encodes the partially coherent imaging behavior
 * of an optical lithography system. It is computed by integrating over all source points.
 * 
 * Mathematical definition:
 *   TCC(f1, f2) = integral over source S(s) * P(f1 + s) * conj(P(f2 + s))
 *   where P(f) = pupil function (defocus phase factor for frequencies within NA)
 * 
 * Implementation:
 *   - Iterate over all source points within outerSigma
 *   - For each source point, compute pupil contribution
 *   - Accumulate TCC entries for valid frequency pairs
 */
void calcTCC(std::vector<ComplexD>& tcc, int tccSize,
             const std::vector<float>& src, int srcSize, float outerSigma,
             const SOCSConfig& config, int verbose) {
    
    // Extract parameters
    float lambda = config.wavelength;
    float NA = config.NA;
    float defocus = config.defocus;
    int Lx = config.Lx;
    int Ly = config.Ly;
    int Nx = config.Nx;
    int Ny = config.Ny;
    
    // Compute derived parameters
    int sh = (srcSize - 1) / 2;  // Source half-size (normalized coordinate range)
    int oSgm = static_cast<int>(ceil(sh * outerSigma));  // Source integration range
    
    // Defocus in normalized units (Rayleigh unit)
    float dz = defocus / (NA * NA / lambda);
    
    // Wave number
    float k = 2.0f * static_cast<float>(M_PI) / lambda;
    
    // Normalized lithography coordinates
    float Lx_norm = Lx * NA / lambda;
    float Ly_norm = Ly * NA / lambda;
    
    // Initialize TCC matrix
    tcc.assign(tccSize * tccSize, ComplexD(0, 0));
    
    // Helper function to compute pupil value on-the-fly
    // pupil(ny, nx; sx, sy) = exp(i * k * dz * sqrt(1 - rho^2 * NA^2))
    auto computePupil = [&](int ny, int nx, float sx, float sy) -> ComplexD {
        float fy = static_cast<float>(ny) / Ly_norm + sy;
        float fx = static_cast<float>(nx) / Lx_norm + sx;
        float rho2 = fx * fx + fy * fy;
        if (rho2 > 1.0f) return ComplexD(0, 0);  // Outside NA pupil
        float tmpValFloat = dz * k * sqrt(1.0f - rho2 * NA * NA);
        return ComplexD(cos(tmpValFloat), sin(tmpValFloat));
    };
    
    if (verbose >= 1) {
        std::cout << "[TCC] Computing pupil functions..." << std::endl;
        std::cout << "[TCC] Parameters:" << std::endl;
        std::cout << "  outerSigma = " << outerSigma << std::endl;
        std::cout << "  dz (defocus) = " << dz << std::endl;
        std::cout << "  k = " << k << std::endl;
        std::cout << "  Lx_norm = " << Lx_norm << std::endl;
        std::cout << "  Ly_norm = " << Ly_norm << std::endl;
        std::cout << "  Nx = " << Nx << ", Ny = " << Ny << std::endl;
        std::cout << "  TCC size = " << tccSize << " x " << tccSize << std::endl;
    }
    
    // Compute TCC directly using on-the-fly pupil calculation
    // TCC(ny1, nx1; ny2, nx2) = sum over source S(s) * P(ny1, nx1; s) * conj(P(ny2, nx2; s))
    
    if (verbose >= 1) {
        std::cout << "[TCC] Computing TCC matrix..." << std::endl;
        std::cout << "  Source integration range: oSgm = " << oSgm << " (sh = " << sh << ")" << std::endl;
    }
    
    int srcCount = 0;
    for (int q = -oSgm; q <= oSgm; q++) {
        for (int p = -oSgm; p <= oSgm; p++) {
            int srcID = (q + sh) * srcSize + p + sh;
            
            if (src[srcID] != 0.0f && (p * p + q * q) <= sh * sh) {
                srcCount++;
                if (verbose >= 2 || (srcCount % 100 == 0 && verbose >= 1)) {
                    std::cout << "  Processing source point " << srcCount << ": (p=" << p << ", q=" << q << ")" << std::endl;
                }
                ComplexD srcVal = ComplexD(src[srcID], 0);
                float sx = static_cast<float>(p) / sh;
                float sy = static_cast<float>(q) / sh;
                
                int nxMinTmp = static_cast<int>((-1.0f - sx) * Lx_norm);
                int nxMaxTmp = static_cast<int>((1.0f - sx) * Lx_norm);
                int nyMinTmp = static_cast<int>((-1.0f - sy) * Ly_norm);
                int nyMaxTmp = static_cast<int>((1.0f - sy) * Ly_norm);
                
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                        ComplexD pupil1 = computePupil(ny, nx, sx, sy);
                        if (pupil1 != ComplexD(0, 0)) {
                            int ID1 = (ny + Ny) * (2 * Nx + 1) + nx + Nx;
                            ComplexD tmpValComplex = pupil1 * srcVal;
                            
                            for (int my = ny; my <= nyMaxTmp; my++) {
                                int startmx = (ny == my) ? nx : nxMinTmp;
                                for (int mx = startmx; mx <= nxMaxTmp; mx++) {
                                    ComplexD pupil2 = computePupil(my, mx, sx, sy);
                                    if (pupil2 != ComplexD(0, 0)) {
                                        int ID2 = (my + Ny) * (2 * Nx + 1) + mx + Nx;
                                        tcc[ID1 * tccSize + ID2] += tmpValComplex * std::conj(pupil2);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Step 3: Fill symmetric part (Hermitian symmetry)
    // TCC(ID2, ID1) = conj(TCC(ID1, ID2))
    for (int i = 0; i < tccSize; i++) {
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = std::conj(tcc[i * tccSize + j]);
        }
    }
    
    if (verbose >= 1) {
        // Print TCC statistics
        double sumReal = 0.0, sumImag = 0.0;
        double maxReal = 0.0, maxImag = 0.0;
        for (int i = 0; i < tccSize * tccSize; i++) {
            sumReal += tcc[i].real();
            sumImag += tcc[i].imag();
            maxReal = std::max(maxReal, std::abs(tcc[i].real()));
            maxImag = std::max(maxImag, std::abs(tcc[i].imag()));
        }
        std::cout << "[TCC] Matrix computed." << std::endl;
        std::cout << "  Sum(real) = " << sumReal << ", Sum(imag) = " << sumImag << std::endl;
        std::cout << "  Max(real) = " << maxReal << ", Max(imag) = " << maxImag << std::endl;
    }
}

/**
 * Compute aerial image spectrum from TCC and mask spectrum
 * 
 * Mathematical formula:
 *   I(f) = sum over f1, f2: TCC(f1, f2) * M(f + f1) * conj(M(f2))
 *   where M is the mask spectrum
 * 
 * This is the TCC direct method, used for verification against SOCS
 */
void calcImageFromTCC(std::vector<ComplexD>& imgf,
                      const std::vector<ComplexD>& mskf,
                      int fftLx, int fftLy,
                      const std::vector<ComplexD>& tcc,
                      int tccSize, int Nx, int Ny, int verbose) {
    
    int tccSizeh = (tccSize - 1) / 2;  // = Nx (or Ny)
    int fftLxh = fftLx / 2;
    int fftLyh = fftLy / 2;
    
    imgf.assign(fftLx * fftLy, ComplexD(0, 0));
    
    if (verbose >= 1) {
        std::cout << "[calcImage] Computing aerial image spectrum from TCC..." << std::endl;
        std::cout << "  FFT size: " << fftLx << " x " << fftLy << std::endl;
        std::cout << "  TCC size: " << tccSize << std::endl;
        std::cout << "  Nx = " << Nx << ", Ny = " << Ny << std::endl;
    }
    
    // Compute aerial image spectrum
    // Output frequency range: ny2 = -2*Ny to 2*Ny, nx2 = -2*Nx to 2*Nx
    for (int ny2 = -2 * Ny; ny2 <= 2 * Ny; ny2++) {
        for (int nx2 = 0; nx2 <= 2 * Nx; nx2++) {
            ComplexD val = 0;
            
            // Sum over TCC frequencies
            for (int ny1 = -Ny; ny1 <= Ny; ny1++) {
                for (int nx1 = -Nx; nx1 <= Nx; nx1++) {
                    // Check if (ny2 + ny1, nx2 + nx1) is within valid range
                    if (std::abs(nx2 + nx1) <= Nx && std::abs(ny2 + ny1) <= Ny) {
                        // Mask spectrum indices
                        int idxA_y = ny2 + ny1 + fftLyh;
                        int idxA_x = nx2 + nx1 + fftLxh;
                        int idxB_y = ny1 + fftLyh;
                        int idxB_x = nx1 + fftLxh;
                        
                        // Check bounds
                        if (idxA_y >= 0 && idxA_y < fftLy &&
                            idxA_x >= 0 && idxA_x < fftLx &&
                            idxB_y >= 0 && idxB_y < fftLy &&
                            idxB_x >= 0 && idxB_x < fftLx) {
                            // TCC index
                            int tccIdx1 = ny1 * (2 * Nx + 1) + nx1 + tccSizeh;
                            int tccIdx2 = (ny2 + ny1) * (2 * Nx + 1) + (nx2 + nx1) + tccSizeh;
                            
                            val += mskf[idxA_y * fftLx + idxA_x]
                                 * conj(mskf[idxB_y * fftLx + idxB_x])
                                 * tcc[tccIdx1 * tccSize + tccIdx2];
                        }
                    }
                }
            }
            
            // Store in output spectrum
            int outY = ny2 + fftLyh;
            int outX = nx2 + fftLxh;
            if (outY >= 0 && outY < fftLy && outX >= 0 && outX < fftLx) {
                imgf[outY * fftLx + outX] = val;
            }
        }
    }
    
    if (verbose >= 1) {
        std::cout << "[calcImage] Aerial image spectrum computed." << std::endl;
    }
}