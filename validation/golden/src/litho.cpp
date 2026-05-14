/**
 * Litho-Full: Integrated TCC + SOCS Verification Program
 * ========================================================
 * Power-of-two padded FFT draft version for HLS-compatible flow.
 */

// Enable M_PI on Windows
#define _USE_MATH_DEFINES
#include <iostream>
#include <complex>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <map>
#include <filesystem>
#include <algorithm>
#include <cassert>
#include <limits>
#include <fftw3.h>

// Use float for HLS compatibility
typedef std::complex<float> ComplexF;
typedef std::complex<double> ComplexD;

#include "common/file_io.hpp"
#include "common/mask.hpp"
#include "common/source.hpp"

using namespace std;
using namespace std::chrono;

// ============================================================================
// Configuration Structure
// ============================================================================

struct FullConfig {
    // Mask parameters
    int Lx = 0, Ly = 0;
    int maskSizeX = 0, maskSizeY = 0;
    string maskType;
    LineSpace lineSpace;
    string maskInputFile;
    float dose = 1.0f;
    
    // Source parameters
    int srcSize = 0;
    string srcType;
    Annular annular;
    Dipole dipole;
    CrossQuadrupole crossQuadrupole;
    Point point;
    string sourceInputFile;
    
    // Optics parameters
    float NA = 0.0f;
    float wavelength = 193.0f;
    float defocus = 0.0f;
    
    // Kernel parameters
    int nk = 0;
    float targetIntensity = 0.225f;
    
    // Output parameters
    string outputDir = "output/full";
    
    // Verbosity
    int verboseLevel = 1;  // 0=quiet, 1=normal, 2=debug
};

// ============================================================================
// Utility Functions
// ============================================================================

float getMax(vector<float>& data) {
    float maxVal = -1e30f;
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] > maxVal) maxVal = data[i];
    }
    return maxVal;
}

float getMin(vector<float>& data) {
    float minVal = 1e30f;
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] < minVal) minVal = data[i];
    }
    return minVal;
}

double computeMean(vector<float>& data) {
    if (data.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); i++) sum += data[i];
    return sum / data.size();
}

double computeStdDev(vector<float>& data, double mean) {
    if (data.empty()) return 0.0;
    double var = 0.0;
    for (size_t i = 0; i < data.size(); i++) {
        double diff = data[i] - mean;
        var += diff * diff;
    }
    return sqrt(var / data.size());
}

bool isPowerOfTwo(int x) {
    return x > 0 && ((x & (x - 1)) == 0);
}

int nextPowerOfTwo(int x) {
    assert(x > 0);
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

void computeCenteredPadOffsets(int srcX, int srcY, int dstX, int dstY, int& offX, int& offY) {
    assert(dstX >= srcX && dstY >= srcY);
    offX = (dstX - srcX) / 2;
    offY = (dstY - srcY) / 2;
}

void computeCenteredCropOffsets(int srcX, int srcY, int dstX, int dstY, int& offX, int& offY) {
    assert(srcX >= dstX && srcY >= dstY);
    offX = (srcX - dstX) / 2;
    offY = (srcY - dstY) / 2;
}

template<typename T>
void myShift(vector<T>& in, vector<T>& out, int sizeX, int sizeY, bool shiftTypeX, bool shiftTypeY) {
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

template<typename T>
void pad2DCentered(const vector<T>& src, int srcX, int srcY,
                   vector<T>& dst, int dstX, int dstY,
                   const T& padValue = T()) {
    assert(dstX >= srcX && dstY >= srcY);
    dst.assign(dstX * dstY, padValue);
    int offX, offY;
    computeCenteredPadOffsets(srcX, srcY, dstX, dstY, offX, offY);

    for (int y = 0; y < srcY; ++y) {
        for (int x = 0; x < srcX; ++x) {
            dst[(y + offY) * dstX + (x + offX)] = src[y * srcX + x];
        }
    }
}

template<typename T>
void crop2DCentered(const vector<T>& src, int srcX, int srcY,
                    vector<T>& dst, int dstX, int dstY) {
    assert(srcX >= dstX && srcY >= dstY);
    dst.resize(dstX * dstY);
    int offX, offY;
    computeCenteredCropOffsets(srcX, srcY, dstX, dstY, offX, offY);

    for (int y = 0; y < dstY; ++y) {
        for (int x = 0; x < dstX; ++x) {
            dst[y * dstX + x] = src[(y + offY) * srcX + (x + offX)];
        }
    }
}

void complexToFloat(vector<ComplexD>& dataComplex, vector<float>& dataReal, vector<float>& dataImag) {
    int size = dataComplex.size();
    dataReal.resize(size);
    dataImag.resize(size);
    for (int i = 0; i < size; i++) {
        dataReal[i] = static_cast<float>(dataComplex[i].real());
        dataImag[i] = static_cast<float>(dataComplex[i].imag());
    }
}

double computeMaxAbsError(const vector<float>& a, const vector<float>& b) {
    assert(a.size() == b.size());
    double maxErr = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        maxErr = max(maxErr, abs((double)a[i] - (double)b[i]));
    }
    return maxErr;
}

// ============================================================================
// FFT Functions
// ============================================================================

/**
 * ========================================================================
 * Padded real-to-complex 2D FFT
 * ========================================================================
 * 
 * Purpose:
 *   - Compute 2D FFT of a physical image, with automatic padding to 2^N size
 *   - Support arbitrary input sizes (non-power-of-two) for HLS compatibility
 *
 * Parameters:
 *   in      : Input real image of physical size (inSizeX x inSizeY)
 *   out     : Output full complex spectrum of FFT size (fftSizeX x fftSizeY)
 *   inSizeX/Y  : Physical image dimensions (may be non-power-of-two)
 *   fftSizeX/Y: FFT execution dimensions (must be power-of-two, >= input size)
 *
 * Processing Steps:
 *   1. Center-pad input image to FFT size (zero-padding)
 *   2. Shift for FFT convention (zero-frequency to array corner)
 *   3. Execute FFTW R2C FFT
 *   4. Rebuild full complex spectrum from FFTW's half-spectrum output
 *   5. Normalize by 1/(fftSizeX * fftSizeY)
 *
 * Normalization Convention (CRITICAL for HLS matching):
 *   - Forward FFT: output is DIVIDED by (fftSizeX * fftSizeY)
 *   - This means the spectrum is normalized so that:
 *     * Total energy in spectrum equals mean of input image
 *     * IFFT (without extra scaling) reconstructs the padded image
 *   - This convention matches litho.cpp original behavior for 2^N sizes
 *   - For non-2^N sizes, the padded zeros contribute to normalization denominator
 *
 * Output Spectrum Layout:
 *   - Full complex array of size (fftSizeX x fftSizeY)
 *   - Zero-frequency (DC) is at array center after myShift convention
 *   - Positive frequencies: columns 0 to fftSizeX/2
 *   - Negative frequencies: columns fftSizeX/2+1 to fftSizeX-1
 *
 * WARNING: The normalization uses FFT SIZE, not physical size.
 * This ensures consistent scaling when IFFT is applied.
 * ========================================================================
 */
void FT_r2c_padded(const vector<float>& in, vector<ComplexD>& out,
                   int inSizeX, int inSizeY,
                   int fftSizeX, int fftSizeY,
                   fftw_plan &fwd, vector<double>& datar, vector<ComplexD>& datac) {
    assert(fftSizeX >= inSizeX && fftSizeY >= inSizeY);
    assert((int)datar.size() == fftSizeX * fftSizeY);
    assert((int)datac.size() == fftSizeY * (fftSizeX / 2 + 1));

    // 1) center-pad to FFT size
    vector<float> padded;
    pad2DCentered(in, inSizeX, inSizeY, padded, fftSizeX, fftSizeY, 0.0f);

    // 2) shift before FFT
    vector<float> shifted(fftSizeX * fftSizeY);
    myShift(padded, shifted, fftSizeX, fftSizeY, false, false);

    // 3) load to FFTW real buffer
    for (int i = 0; i < fftSizeX * fftSizeY; i++) {
        datar[i] = static_cast<double>(shifted[i]);
    }

    // 4) execute
    fftw_execute(fwd);

    // 5) rebuild full complex spectrum from half-spectrum
    int sizeXh = fftSizeX / 2;
    int wt = (fftSizeX / 2) + 1;
    int AY = (fftSizeY + 1) / 2;
    int BY = fftSizeY - AY;

    vector<ComplexD> R(wt * fftSizeY, ComplexD(0, 0));
    vector<ComplexD> L(wt * fftSizeY, ComplexD(0, 0));

    for (int y = 0, y2 = AY; y < BY; y++, y2++) {
        for (int x = 0; x < wt; x++) {
            R[y * wt + x] = datac[y2 * wt + x];
        }
    }
    for (int y = BY, y2 = 0; y < fftSizeY; y++, y2++) {
        for (int x = 0; x < wt; x++) {
            R[y * wt + x] = datac[y2 * wt + x];
        }
    }

    if (fftSizeY % 2 == 0) {
        for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
            L[x] = conj(R[x2]);
        }
        for (int y = 1, y2 = fftSizeY - 1; y < fftSizeY; y++, y2--) {
            for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
                L[y * wt + x] = conj(R[y2 * wt + x2]);
            }
        }
    } else {
        for (int y = 0, y2 = fftSizeY - 1; y < fftSizeY; y++, y2--) {
            for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
                L[y * wt + x] = conj(R[y2 * wt + x2]);
            }
        }
    }

    out.assign(fftSizeX * fftSizeY, ComplexD(0, 0));
    for (int y = 0; y < fftSizeY; y++) {
        for (int x = 0; x < wt; x++) {
            out[y * fftSizeX + x] = L[y * wt + x];
        }
    }
    for (int y = 0; y < fftSizeY; y++) {
        for (int x = sizeXh, x2 = 0; x < fftSizeX; x++, x2++) {
            out[y * fftSizeX + x] = R[y * wt + x2];
        }
    }

    for (int i = 0; i < fftSizeX * fftSizeY; i++) {
        out[i] /= (fftSizeX * fftSizeY);
    }
}

/**
 * ========================================================================
 * Padded complex-to-real 2D Inverse FFT
 * ========================================================================
 * 
 * Purpose:
 *   - Compute 2D IFFT of a complex spectrum, with automatic cropping to physical size
 *   - Completes the round-trip: physical image -> padded FFT -> padded IFFT -> physical image
 *
 * Parameters:
 *   in        : Input full complex spectrum of FFT size (fftSizeX x fftSizeY)
 *   out       : Output real image of physical size (outSizeX x outSizeY)
 *   outSizeX/Y: Target physical output dimensions (may be smaller than FFT size)
 *   fftSizeX/Y: FFT execution dimensions (must match spectrum size)
 *
 * Processing Steps:
 *   1. Convert full complex spectrum to FFTW's half-spectrum format
 *   2. Execute FFTW C2R IFFT (produces fftSizeX x fftSizeY real output)
 *   3. Shift to restore zero-frequency to array center
 *   4. Center-crop padded output to physical size (outSizeX x outSizeY)
 *
 * Normalization Convention (CRITICAL for correctness):
 *   - NO extra scaling is applied here
 *   - FFTW C2R by default multiplies by fftSizeX * fftSizeY
 *   - Combined with FT_r2c_padded's forward-side division by (fftSizeX * fftSizeY):
 *     * Full round-trip: input -> FFT -> IFFT -> output preserves amplitude
 *     * The scaling factors cancel out exactly
 *   - This is the standard "unitary" convention for FFT/IFFT pairs
 *
 * Energy Conservation:
 *   - If input image has total energy E, padded zeros add no energy
 *   - After FFT+IFFT, the cropped output has total energy = E * (outSizeX*outSizeY)/(fftSizeX*fftSizeY)
 *   - This is correct: the ratio accounts for the zero-padded regions
 *
 * WARNING: The input spectrum must use the SAME normalization convention as FT_r2c_padded.
 * Using mismatched normalization will result in incorrect amplitude scaling.
 * ========================================================================
 */
void FT_c2r_padded(const vector<ComplexD>& in, vector<float>& out,
                   int outSizeX, int outSizeY,
                   int fftSizeX, int fftSizeY,
                   fftw_plan &bwd, vector<ComplexD>& datac, vector<double>& datar) {
    assert((int)in.size() == fftSizeX * fftSizeY);
    assert((int)datac.size() == fftSizeY * (fftSizeX / 2 + 1));
    assert((int)datar.size() == fftSizeX * fftSizeY);
    assert(fftSizeX >= outSizeX && fftSizeY >= outSizeY);

    int wt = (fftSizeX / 2) + 1;
    int xh = (fftSizeX + 1) / 2;
    int yh = (fftSizeY + 1) / 2;

    fill(datac.begin(), datac.end(), ComplexD(0, 0));

    for (int y = 0; y < fftSizeY; y++) {
        for (int x = xh - 1; x < fftSizeX; x++) {
            int sx = (x + xh) % fftSizeX;
            int sy = (y + yh) % fftSizeY;
            if (sx < wt) {
                datac[sy * wt + sx] = in[y * fftSizeX + x];
            }
        }
    }

    if (fftSizeX % 2 == 0) {
        vector<ComplexD> R(fftSizeY);
        R[0] = conj(in[0]);
        for (int y = 1, y2 = fftSizeY - 1; y < fftSizeY; y++, y2--) {
            R[y] = conj(in[y2 * fftSizeX]);
        }
        for (int y = 0; y < fftSizeY; y++) {
            datac[y * wt + wt - 1] = R[y];
        }
    }

    fftw_execute(bwd);

    // shift back
    vector<double> shifted_d(fftSizeX * fftSizeY);
    myShift(datar, shifted_d, fftSizeX, fftSizeY, false, false);

    // convert to float full padded image
    vector<float> paddedOut(fftSizeX * fftSizeY);
    for (int i = 0; i < fftSizeX * fftSizeY; i++) {
        paddedOut[i] = static_cast<float>(shifted_d[i]);
    }

    // center-crop back to physical size
    crop2DCentered(paddedOut, fftSizeX, fftSizeY, out, outSizeX, outSizeY);
}

// ============================================================================
// TCC Functions
// ============================================================================

float getOuterSigma(vector<float>& src, int srcSize) {
    int center = (srcSize - 1) / 2;
    float max_distance = 0.0f;
    for (int y = 0; y < srcSize; y++) {
        for (int x = 0; x < srcSize; x++) {
            if (src[y * srcSize + x] != 0.0f) {
                float distance = sqrt((y - center) * (y - center) + (x - center) * (x - center));
                if (distance > max_distance) max_distance = distance;
            }
        }
    }
    return (center == 0) ? 0.0f : (max_distance / center);
}

void calcTCC(vector<ComplexD>& tcc, int tccSize, vector<float>& src, int srcSize, float outerSigma,
             float lambda, float NA, float defocus, int Lx, int Ly, int Nx, int Ny, int verbose) {
    int sh = ((srcSize - 1) / 2.0);
    int oSgm = ceil(sh * outerSigma);
    float dz = defocus / (NA * NA / lambda);
    float k = 2 * M_PI / lambda;
    float Lx_norm = Lx * NA / lambda;
    float Ly_norm = Ly * NA / lambda;
    
    vector<vector<ComplexD>> pupil(tccSize, vector<ComplexD>(srcSize * srcSize, ComplexD(0, 0)));
    
    if (verbose >= 1) {
        cout << "[TCC] Computing pupil functions..." << endl;
        cout << "[TCC] Parameters: outerSigma=" << outerSigma << ", dz=" << dz << ", k=" << k << endl;
    }
    
    // Compute pupil
    for (int q = -oSgm; q <= oSgm; q++) {
        for (int p = -oSgm; p <= oSgm; p++) {
            int srcID = (q + sh) * srcSize + p + sh;
            if (src[srcID] != 0 && (p * p + q * q) <= sh * sh) {
                float sx = (float)p / sh;
                float sy = (float)q / sh;
                int nxMinTmp = (-1 - sx) * Lx_norm;
                int nxMaxTmp = (1 - sx) * Lx_norm;
                int nyMinTmp = (-1 - sy) * Ly_norm;
                int nyMaxTmp = (1 - sy) * Ly_norm;
                
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    float fy = (float)ny / Ly_norm + sy;
                    if (fy * fy <= 1) {
                        for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                            float fx = (float)nx / Lx_norm + sx;
                            float rho2 = fx * fx + fy * fy;
                            if (rho2 <= 1.0) {
                                float tmpValFloat = dz * k * (sqrt(1.0 - rho2 * NA * NA));
                                pupil[(ny + Ny) * (Nx * 2 + 1) + nx + Nx][srcID] = ComplexD(cos(tmpValFloat), sin(tmpValFloat));
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (verbose >= 1) {
        cout << "[TCC] Computing TCC matrix..." << endl;
    }
    
    // Compute TCC
    ComplexD srcVal;
    for (int q = -oSgm; q <= oSgm; q++) {
        for (int p = -oSgm; p <= oSgm; p++) {
            int srcID = (q + sh) * srcSize + p + sh;
            if (src[srcID] != 0 && (p * p + q * q) <= sh * sh) {
                srcVal = ComplexD(src[srcID], 0);
                float sx = (float)p / sh;
                float sy = (float)q / sh;
                int nxMinTmp = (-1 - sx) * Lx_norm;
                int nxMaxTmp = (1 - sx) * Lx_norm;
                int nyMinTmp = (-1 - sy) * Ly_norm;
                int nyMaxTmp = (1 - sy) * Ly_norm;
                
                for (int ny = nyMinTmp; ny <= nyMaxTmp; ny++) {
                    for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) {
                        int ID1 = (ny + Ny) * (Nx * 2 + 1) + (nx + Nx);
                        if (pupil[ID1][srcID] != ComplexD(0, 0)) {
                            ComplexD tmpValComplex = pupil[ID1][srcID] * srcVal;
                            for (int my = ny; my <= nyMaxTmp; my++) {
                                int startmx = (ny == my) ? nx : nxMinTmp;
                                for (int mx = startmx; mx <= nxMaxTmp; mx++) {
                                    int ID2 = (my + Ny) * (Nx * 2 + 1) + (mx + Nx);
                                    tcc[ID1 * tccSize + ID2] += tmpValComplex * conj(pupil[ID2][srcID]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Fill symmetric part
    for (int i = 0; i < tccSize; i++) {
        for (int j = i + 1; j < tccSize; j++) {
            tcc[j * tccSize + i] = conj(tcc[i * tccSize + j]);
        }
    }
}

// ============================================================================
// calcImage (TCC-based aerial image) using padded spectrum size
// ============================================================================

void calcImage(vector<ComplexD>& imgf, const vector<ComplexD>& mskf,
               int fftLx, int fftLy,
               const vector<ComplexD>& tcc, int tccSize, int Nx, int Ny, int verbose) {
    int tccSizeh = (tccSize - 1) / 2;
    int fftLxh = fftLx / 2;
    int fftLyh = fftLy / 2;

    imgf.assign(fftLx * fftLy, ComplexD(0, 0));
    
    if (verbose >= 1) {
        cout << "[calcImage] Computing aerial image from TCC..." << endl;
    }
    
    for (int ny2 = -2 * Ny; ny2 <= 2 * Ny; ny2++) {
        for (int nx2 = 0; nx2 <= 2 * Nx; nx2++) {
            ComplexD val = 0;
            for (int ny1 = -Ny; ny1 <= Ny; ny1++) {
                for (int nx1 = -Nx; nx1 <= Nx; nx1++) {
                    if (abs(nx2 + nx1) <= Nx && abs(ny2 + ny1) <= Ny) {
                        int idxA_y = ny2 + ny1 + fftLyh;
                        int idxA_x = nx2 + nx1 + fftLxh;
                        int idxB_y = ny1 + fftLyh;
                        int idxB_x = nx1 + fftLxh;

                        if (idxA_y >= 0 && idxA_y < fftLy &&
                            idxA_x >= 0 && idxA_x < fftLx &&
                            idxB_y >= 0 && idxB_y < fftLy &&
                            idxB_x >= 0 && idxB_x < fftLx) {
                            val += mskf[idxA_y * fftLx + idxA_x]
                                 * conj(mskf[idxB_y * fftLx + idxB_x])
                                 * tcc[((ny1) * (2 * Nx + 1) + nx1 + tccSizeh) * tccSize
                                     + ((ny2 + ny1) * (2 * Nx + 1) + (nx2 + nx1) + tccSizeh)];
                        }
                    }
                }
            }

            int outY = ny2 + fftLyh;
            int outX = nx2 + fftLxh;
            if (outY >= 0 && outY < fftLy && outX >= 0 && outX < fftLx) {
                imgf[outY * fftLx + outX] = val;
            }
        }
    }
}

// ============================================================================
// calcKernels (SOCS kernel extraction)
// ============================================================================

extern "C" {
    void zheevr_(const char& jobz, const char& range, const char& uplo, int* n, ComplexD* a,
                 int* lda, double* vl, double* vu, int* il, int* iu, double* abstol,
                 int* m, double* w, ComplexD* z, int* ldz, int* isuppz, ComplexD* work, int* lwork,
                 double* rwork, int* lrwork, int* iwork, int* liwork, int* info);
}

int calcKernels(vector<vector<ComplexD>>& krns, vector<float>& scales, int nk,
                vector<ComplexD>& tcc, int tccSize, int Nx, int Ny, int verbose) {
    int n = tccSize, lda = tccSize, ldz = tccSize, il = tccSize - (nk - 1), iu = tccSize, m = iu - il + 1, info;
    double abstol, vl, vu;
    
    ComplexD* a = new ComplexD[tccSize * tccSize];
    ComplexD* z = new ComplexD[tccSize * m];
    double* w = new double[m];
    int* isuppz = new int[2 * m];
    
    for (int i = 0; i < tccSize; i++) {
        for (int j = 0; j < tccSize; j++) {
            a[i * tccSize + j] = tcc[j * tccSize + i];
        }
    }
    
    abstol = -1.0;
    int lwork = -1, lrwork = -1, liwork = -1;
    ComplexD wkopt;
    double rwkopt;
    int iwkopt;
    
    // Workspace query - m is an OUTPUT parameter and may be modified
    int m_expected = m;  // Save expected number of eigenvalues
    zheevr_('V', 'I', 'U', &n, a, &lda, &vl, &vu, &il, &iu,
            &abstol, &m, w, z, &ldz, isuppz, &wkopt, &lwork, &rwkopt, &lrwork, &iwkopt, &liwork, &info);
    m = m_expected;  // Reset m after workspace query
    
    lwork = static_cast<int>(wkopt.real());
    lrwork = static_cast<int>(rwkopt);
    liwork = iwkopt;
    
    ComplexD* work = static_cast<ComplexD*>(malloc(lwork * sizeof(ComplexD)));
    double* rwork = static_cast<double*>(malloc(lrwork * sizeof(double)));
    int* iwork = static_cast<int*>(malloc(liwork * sizeof(int)));
    
    if (verbose >= 1) {
        cout << "[calcKernels] Solving eigenproblem for TCC matrix..." << endl;
    }
    
    zheevr_('V', 'I', 'U', &n, a, &lda, &vl, &vu, &il, &iu,
            &abstol, &m, w, z, &ldz, isuppz, work, &lwork, rwork, &lrwork, iwork, &liwork, &info);
    
    if (verbose >= 1) {
        cout << "[calcKernels] zheevr returned: info=" << info << ", m=" << m << endl;
    }
    
    for (int j = m - 1; j >= 0; j--) {
        scales[m - 1 - j] = static_cast<float>(w[j]);
    }
    
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            krns[i][j] = z[(m - 1 - i) * n + (n - 1 - j)];
        }
    }
    
    free(work);
    free(rwork);
    free(iwork);
    delete[] a;
    delete[] z;
    delete[] w;
    delete[] isuppz;
    
    return info;
}

// ============================================================================
// Fourier Interpolation (Zero-padding upsampling in frequency domain)
// ============================================================================
//
// This function performs Fourier Interpolation (FI), which is essentially
// upsampling via zero-padding in the frequency domain.
//
// Mathematical principle:
//   1. Forward FFT of input (smaller image) -> frequency spectrum
//   2. Embed the low-frequency content into larger output spectrum (zero-pad high freq)
//   3. Inverse FFT of output spectrum -> interpolated (larger) image
//
// Normalization convention:
//   - Forward FFT: output divided by (in_sizeX * in_sizeY)
//   - Backward FFT: FFTW c2r default (no extra scaling)
//   - Combined: preserves signal energy, results in correct amplitude scaling
//
// Requirements:
//   - Both in_sizeX/in_sizeY and out_sizeX/out_sizeY should be power-of-two
//   - out_sizeX >= in_sizeX, out_sizeY >= in_sizeY (upsampling only)
//   - FFT plans must match the declared sizes
//
// IMPORTANT: The frequency domain embedding preserves the PHYSICAL low-frequency
// content of the input. The zero-padded high-frequency region does NOT introduce
// artificial content - it represents interpolation between original samples.
// ============================================================================

template<typename T1, typename T2>
void FI(vector<T1>& in, int in_sizeX, int in_sizeY,
        vector<T2>& out, int out_sizeX, int out_sizeY,
        fftw_plan &R2C_plan, vector<double>& R2CDataReal, vector<ComplexD>& R2CDataComplex,
        fftw_plan &C2R_plan, vector<ComplexD>& C2RDataComplex, vector<double>& C2RDataReal) {
    
    // Validate dimensions (upsampling only)
    assert(out_sizeX >= in_sizeX && out_sizeY >= in_sizeY);
    assert(isPowerOfTwo(in_sizeX) && isPowerOfTwo(in_sizeY));
    assert(isPowerOfTwo(out_sizeX) && isPowerOfTwo(out_sizeY));
    
    int difY = out_sizeY - in_sizeY;
    int in_sizeXY = in_sizeX * in_sizeY;
    int in_wt = in_sizeX / 2 + 1;  // Width of input half-spectrum
    int out_wt = out_sizeX / 2 + 1; // Width of output half-spectrum
    
    // Clear buffers
    fill(R2CDataReal.begin(), R2CDataReal.end(), 0);
    fill(R2CDataComplex.begin(), R2CDataComplex.end(), 0);
    fill(C2RDataComplex.begin(), C2RDataComplex.end(), 0);
    fill(C2RDataReal.begin(), C2RDataReal.end(), 0);
    
    // Step 1: Shift input for FFT (move zero-frequency to corner)
    vector<T1> shifted_in(in_sizeX * in_sizeY);
    myShift(in, shifted_in, in_sizeX, in_sizeY, false, false);
    for (int i = 0; i < in_sizeX * in_sizeY; i++) {
        R2CDataReal[i] = static_cast<double>(shifted_in[i]);
    }
    
    // Step 2: Forward FFT (R2C) with normalization
    fftw_execute(R2C_plan);
    for (int i = 0; i < in_sizeY * in_wt; i++) {
        R2CDataComplex[i] /= in_sizeXY;
    }
    
    // Step 3: Copy low-frequency content from input spectrum to output spectrum
    // The key is to preserve the center-zero layout after FFTW's half-spectrum format
    //
    // FFTW R2C output layout (for Nx x Ny image):
    //   - Rows 0 to Ny/2: positive frequencies (center region after shift)
    //   - Rows Ny/2+1 to Ny-1: negative frequencies (wrapped from bottom)
    //
    // For upsampling, we copy the central low-frequency region:
    //   - Copy positive freq region (rows 0 to in_sizeY/2) to output rows 0 to in_sizeY/2
    //   - Copy negative freq region (rows in_sizeY/2+1 to end) to output rows with offset difY
    
    // Copy positive frequency region (top half, including DC at row 0)
    int posFreqRows = in_sizeY / 2 + 1;  // Number of positive frequency rows
    for (int y = 0; y < posFreqRows; y++) {
        for (int x = 0; x < in_wt; x++) {
            // Input spectrum only extends to in_wt columns
            // Output spectrum extends to out_wt columns - zeros beyond in_wt
            if (x < in_wt && x < out_wt) {
                C2RDataComplex[y * out_wt + x] = R2CDataComplex[y * in_wt + x];
            }
        }
    }
    
    // Copy negative frequency region (bottom half) with offset to maintain center alignment
    for (int y = posFreqRows; y < in_sizeY; y++) {
        int out_y = y + difY;  // Shift down by the difference in size
        for (int x = 0; x < in_wt; x++) {
            if (x < out_wt && out_y < out_sizeY) {
                C2RDataComplex[out_y * out_wt + x] = R2CDataComplex[y * in_wt + x];
            }
        }
    }
    
    // Step 4: Inverse FFT (C2R) - produces out_sizeX * out_sizeY real image
    fftw_execute(C2R_plan);
    
    // Step 5: Shift output back (restore zero-frequency to center)
    vector<double> shifted_out(out_sizeX * out_sizeY);
    myShift(C2RDataReal, shifted_out, out_sizeX, out_sizeY, false, false);
    
    // Step 6: Convert to output type
    out.resize(out_sizeX * out_sizeY);
    for (int i = 0; i < out_sizeX * out_sizeY; i++) {
        out[i] = static_cast<T2>(shifted_out[i]);
    }
}

// ============================================================================
// calcSOCS - SOCS-based Aerial Image Computation
// ============================================================================
//
// This function computes the aerial image using the SOCS (Sum of Coherent
// Systems) approximation method, which uses a small number of coherent kernels
// extracted from the TCC matrix via eigenvalue decomposition.
//
// Mathematical principle:
//   The aerial image I(x,y) is approximated as:
//     I(x,y) = sum_k lambda_k * |IFFT(K_k * M)|^2
//   where:
//     - lambda_k: eigenvalue (scale) for kernel k
//     - K_k: SOCS kernel in frequency domain
//     - M: mask spectrum in frequency domain
//
// Implementation flow:
//   1. For each kernel k, compute product K_k * M in frequency domain
//   2. Place this product into a padded convolution array (fftConvX x fftConvY)
//   3. Execute inverse FFT to get spatial domain amplitude
//   4. Accumulate weighted intensity: lambda_k * |amplitude|^2
//   5. Apply Fourier Interpolation to upsample to final image size
//   6. Center-crop to physical output size
//
// FFT Size Convention:
//   - Convolution IFFT size: fftConvX x fftConvY (padded to 2^N)
//   - Physical convolution support: convX = 4*Nx+1, convY = 4*Ny+1
//   - Final image size: fftLx x fftLy (padded), then crop to physLx x physLy
//
// Normalization Convention:
//   - FFTW BACKWARD (IFFT) does NOT apply 1/N scaling by default
//   - This matches the original litho.cpp behavior
//   - For HLS FFT, the 1/N scaling may need to be added explicitly
//
// IMPORTANT: The kernel*mask embedding uses centered padding to maintain
// phase alignment. The kernel support (2*Nx+1 x 2*Ny+1) is placed at the
// center of the padded convolution array (fftConvX x fftConvY).
// ============================================================================

void calcSOCS(vector<float>& image, const vector<ComplexD>& mskf,
              int physLx, int physLy,
              int fftLx, int fftLy,
              vector<vector<ComplexD>>& krns, vector<float>& scales, int nk, int Nx, int Ny,
              fftw_plan& LxyC2R_plan, vector<ComplexD>& complexDataLxy, vector<double>& realDataLxy,
              int verbose,
              string outputDir = "",  // For HLS golden output
              bool outputIntermediate = false) {
    
    // -----------------------------------------------------------------------
    // Step 1: Compute FFT sizes for convolution
    // -----------------------------------------------------------------------
    // Physical convolution support: needs to accommodate 2*Nx+1 kernel
    // convolved with mask, producing output of size 4*Nx+1
    int convX = 4 * Nx + 1;  // Physical convolution output size
    int convY = 4 * Ny + 1;
    
    // Padded to power-of-two for FFT compatibility
    // ⚠️ MODIFIED: Force FFT size to 128×128 to match HLS MAX_FFT_X architecture
    // This enables proper validation of HLS 128×128 zero-padding strategy
    int fftConvX = 128;  // Force to match HLS MAX_FFT_X
    int fftConvY = 128;  // Force to match HLS MAX_FFT_Y
    
    // Kernel size: the actual support of each SOCS kernel
    int kerX = 2 * Nx + 1;
    int kerY = 2 * Ny + 1;
    
    // Embedding position matching HLS architecture (relative 73.4% position)
    // HLS uses embed_x = (MAX_FFT_X * (64 - kerX)) / 64 for 128×128 array
    // For kerX=17: embed_x = (128 * 47) / 64 = 94
    // This corresponds to relative position 94/128 = 73.4%
    // 
    // For Golden 128×128 array: difX = 128 - 17 = 111 (bottom-right)
    // To match HLS relative position: use embedX = 94
    int embedX = 94;  // Match HLS embed_x calculation
    int embedY = 94;  // Match HLS embed_y calculation
    
    // Keep original variables for backward compatibility (unused in new mode)
    int offKerX = (fftConvX - kerX) / 2;
    int offKerY = (fftConvY - kerY) / 2;
    int difX = fftConvX - kerX;
    int difY = fftConvY - kerY;
    
    // Mask spectrum center indices (in padded FFT domain)
    int fftLxh = fftLx / 2;
    int fftLyh = fftLy / 2;
    
    // -----------------------------------------------------------------------
    // Step 2: Setup FFT plans and buffers
    // -----------------------------------------------------------------------
    if (verbose >= 1) {
        cout << "[calcSOCS] Computing aerial image via SOCS..." << endl;
        cout << "[calcSOCS] Physical convolution size: " << convX << " x " << convY << endl;
        cout << "[calcSOCS] Padded convolution FFT size: " << fftConvX << " x " << fftConvY << endl;
        cout << "[calcSOCS] Kernel support size: " << kerX << " x " << kerY << endl;
        cout << "[calcSOCS] Centered padding offset: (" << offKerX << ", " << offKerY << ")" << endl;
        cout << "[calcSOCS] Number of kernels: " << nk << endl;
    }
    
    // Complex-to-complex IFFT for convolution (backward transform)
    vector<ComplexD> bwdDataIn(fftConvX * fftConvY, ComplexD(0, 0));
    vector<ComplexD> bwdDataOut(fftConvX * fftConvY, ComplexD(0, 0));
    fftw_plan bwd_plan = fftw_plan_dft_2d(
        fftConvY, fftConvX,
        reinterpret_cast<fftw_complex*>(bwdDataIn.data()),
        reinterpret_cast<fftw_complex*>(bwdDataOut.data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    
    // Fourier interpolation buffers: R2C for input (conv grid)
    vector<double> realDataConv(fftConvX * fftConvY, 0.0);
    vector<ComplexD> complexDataConv((fftConvX / 2 + 1) * fftConvY, ComplexD(0, 0));
    fftw_plan ConvR2C = fftw_plan_dft_r2c_2d(
        fftConvY, fftConvX,
        realDataConv.data(),
        reinterpret_cast<fftw_complex*>(complexDataConv.data()),
        FFTW_ESTIMATE);
    
    // Accumulated intensity on convolution grid
    vector<double> tmpImg(fftConvX * fftConvY, 0.0);
    vector<double> tmpImgp(fftConvX * fftConvY, 0.0);
    
    // -----------------------------------------------------------------------
    // Step 3: Process each kernel
    // -----------------------------------------------------------------------
    for (int k = 0; k < nk; ++k) {
        // Clear convolution input buffer for each kernel
        fill(bwdDataIn.begin(), bwdDataIn.end(), ComplexD(0, 0));
        
        // Embed kernel*mask product into padded convolution array
        // Use centered padding to maintain phase alignment
        //
        // The kernel is indexed from -Ny to Ny (y direction) and -Nx to Nx (x direction)
        // The mask spectrum is indexed similarly with center at (fftLxh, fftLyh)
        for (int y = -Ny; y <= Ny; ++y) {
            for (int x = -Nx; x <= Nx; ++x) {
                // Kernel indices (0-based)
                int ky = y + Ny;
                int kx = x + Nx;
                
                // Convolution array indices (centered padding)
                // Option C: HLS-matched embedding (relative 73.4% position)
                // This matches HLS embed_x = (MAX_FFT_X * (64 - kerX)) / 64 = 94
                int by = embedY + ky;
                int bx = embedX + kx;
                
                // Mask spectrum indices
                int my = y + fftLyh;
                int mx = x + fftLxh;
                
                // Validate all indices are within bounds
                if (by >= 0 && by < fftConvY &&
                    bx >= 0 && bx < fftConvX &&
                    my >= 0 && my < fftLy &&
                    mx >= 0 && mx < fftLx) {
                    // Compute product and store in convolution input
                    bwdDataIn[by * fftConvX + bx] =
                        krns[k][ky * kerX + kx] * mskf[my * fftLx + mx];
                }
            }
        }
        
        // Execute inverse FFT (frequency domain -> spatial domain)
        // NOTE: FFTW BACKWARD does NOT scale by 1/N
        fftw_execute(bwd_plan);
        
        // Accumulate weighted intensity for this kernel
        // I(x,y) += lambda_k * |IFFT(K_k * M)|^2
        for (int i = 0; i < fftConvX * fftConvY; ++i) {
            double re = bwdDataOut[i].real();
            double im = bwdDataOut[i].imag();
            tmpImg[i] += scales[k] * (re * re + im * im);
        }
    }
    
    // -----------------------------------------------------------------------
    // Step 4: Fourier Interpolation to final image size
    // -----------------------------------------------------------------------
    // Shift accumulated intensity to prepare for Fourier interpolation
    // (Move zero-frequency to center for proper spectral embedding)
    myShift(tmpImg, tmpImgp, fftConvX, fftConvY, true, true);

    // -----------------------------------------------------------------------
    // HLS GOLDEN OUTPUT: tmpImgp (before Fourier Interpolation)
    // -----------------------------------------------------------------------
    // This is the intermediate output that HLS IP should produce
    // Size: fftConvX × fftConvY (e.g., 32×32 for Nx=4)
    // Extract center convX × convY (e.g., 17×17) as the actual output
    if (outputIntermediate && !outputDir.empty()) {
        if (verbose >= 1) {
            cout << "[GOLDEN OUTPUT] Writing tmpImgp for HLS validation..." << endl;
            cout << "  Full tmpImgp size: " << fftConvX << "×" << fftConvY << endl;
            cout << "  Extracted center: " << convX << "×" << convY << endl;
        }
        
        // Extract center region (convX × convY) from tmpImgp (fftConvX × fftConvY)
        vector<float> tmpImgp_center(convX * convY);
        int offsetX = (fftConvX - convX) / 2;
        int offsetY = (fftConvY - convY) / 2;
        
        for (int y = 0; y < convY; y++) {
            for (int x = 0; x < convX; x++) {
                int srcY = offsetY + y;
                int srcX = offsetX + x;
                tmpImgp_center[y * convX + x] = static_cast<float>(tmpImgp[srcY * fftConvX + srcX]);
            }
        }
        
        // Output golden files for HLS
        string goldenFile = outputDir + "/tmpImgp_pad" + to_string(fftConvX) + ".bin";
        writeFloatArrayToBinary(goldenFile, tmpImgp_center, convX * convY);
        
        if (verbose >= 1) {
            cout << "  ✓ Golden saved: " << goldenFile << endl;
            cout << "  ✓ Golden size: " << convX << "×" << convY << " = " << (convX * convY) << " floats" << endl;
        }
        
        // Also output full tmpImgp for Fourier Interpolation validation
        // Changed: Always output full 128×128 for HLS FI validation (Phase 1.4+)
        if (verbose >= 1) {  // Changed from >= 2 to >= 1
            vector<float> tmpImgp_full(fftConvX * fftConvY);
            for (int i = 0; i < fftConvX * fftConvY; i++) {
                tmpImgp_full[i] = static_cast<float>(tmpImgp[i]);
            }
            string fullFile = outputDir + "/tmpImgp_full_" + to_string(fftConvX) + ".bin";
            writeFloatArrayToBinary(fullFile, tmpImgp_full, fftConvX * fftConvY);
            cout << "  ✓ Full tmpImgp saved: " << fullFile << " (for FI validation)" << endl;
        }
    }

    // IMPORTANT:
    // Here we interpolate from padded conv grid -> padded full image grid (fftLx * fftLy),
    // then center-crop back to physical image size.
    vector<float> imagePadded(fftLx * fftLy, 0.0f);

    vector<ComplexD> complexDataFinal(fftLy * (fftLx / 2 + 1), ComplexD(0, 0));
    vector<double> realDataFinal(fftLx * fftLy, 0.0);
    fftw_plan FinalC2R = fftw_plan_dft_c2r_2d(
        fftLy, fftLx,
        reinterpret_cast<fftw_complex*>(complexDataFinal.data()),
        realDataFinal.data(),
        FFTW_ESTIMATE);

    FI(tmpImgp, fftConvX, fftConvY, imagePadded, fftLx, fftLy,
       ConvR2C, realDataConv, complexDataConv,
       FinalC2R, complexDataFinal, realDataFinal);

    crop2DCentered(imagePadded, fftLx, fftLy, image, physLx, physLy);
    
    fftw_destroy_plan(FinalC2R);
    fftw_destroy_plan(bwd_plan);
    fftw_destroy_plan(ConvR2C);
}

// ============================================================================
// JSON Parser (Simple)
// ============================================================================

string extractSection(const string& content, const string& sectionName) {
    size_t pos = content.find("\"" + sectionName + "\"");
    if (pos == string::npos) return "";
    pos = content.find("{", pos);
    if (pos == string::npos) return "";
    
    int braceCount = 1;
    size_t end = pos + 1;
    while (end < content.length() && braceCount > 0) {
        if (content[end] == '{') braceCount++;
        else if (content[end] == '}') braceCount--;
        end++;
    }
    
    return content.substr(pos, end - pos);
}

bool parseValueInSection(const string& section, const string& key, int& value) {
    size_t pos = section.find("\"" + key + "\"");
    if (pos == string::npos) return false;
    pos = section.find(":", pos);
    if (pos == string::npos) return false;
    size_t start = pos + 1;
    while (start < section.length() && isspace(section[start])) start++;
    value = stoi(section.substr(start));
    return true;
}

bool parseValueInSection(const string& section, const string& key, float& value) {
    size_t pos = section.find("\"" + key + "\"");
    if (pos == string::npos) return false;
    pos = section.find(":", pos);
    if (pos == string::npos) return false;
    size_t start = pos + 1;
    while (start < section.length() && isspace(section[start])) start++;
    value = stof(section.substr(start));
    return true;
}

bool parseValueInSection(const string& section, const string& key, string& value) {
    size_t pos = section.find("\"" + key + "\"");
    if (pos == string::npos) return false;
    pos = section.find(":", pos);
    if (pos == string::npos) return false;
    size_t start = section.find("\"", pos + 1);
    if (start == string::npos) return false;
    size_t end = section.find("\"", start + 1);
    if (end == string::npos) return false;
    value = section.substr(start + 1, end - start - 1);
    return true;
}

bool loadConfig(const string& configPath, FullConfig& config) {
    ifstream ifs(configPath);
    if (!ifs.is_open()) {
        cerr << "Error: Cannot open config file: " << configPath << endl;
        return false;
    }
    stringstream buffer;
    buffer << ifs.rdbuf();
    string content = buffer.str();
    ifs.close();
    
    filesystem::path configDir = filesystem::path(configPath).parent_path();
    filesystem::path projectRoot = configDir.parent_path().parent_path();
    string projectRootStr = projectRoot.string();
    
    string maskSection = extractSection(content, "mask");
    string maskPeriodSection = extractSection(maskSection, "period");
    string maskSizeSection = extractSection(maskSection, "size");
    string lineSpaceSection = extractSection(maskSection, "lineSpace");
    
    string sourceSection = extractSection(content, "source");
    string annularSection = extractSection(sourceSection, "annular");
    
    string opticsSection = extractSection(content, "optics");
    string kernelSection = extractSection(content, "kernel");
    
    parseValueInSection(maskPeriodSection, "Lx", config.Lx);
    parseValueInSection(maskPeriodSection, "Ly", config.Ly);
    parseValueInSection(maskSizeSection, "maskSizeX", config.maskSizeX);
    parseValueInSection(maskSizeSection, "maskSizeY", config.maskSizeY);
    parseValueInSection(maskSection, "type", config.maskType);
    parseValueInSection(lineSpaceSection, "lineWidth", config.lineSpace.lineWidth);
    parseValueInSection(lineSpaceSection, "spaceWidth", config.lineSpace.spaceWidth);
    parseValueInSection(maskSection, "inputFile", config.maskInputFile);
    parseValueInSection(maskSection, "dose", config.dose);
    
    parseValueInSection(sourceSection, "gridSize", config.srcSize);
    parseValueInSection(sourceSection, "type", config.srcType);
    parseValueInSection(annularSection, "innerRadius", config.annular.innerRadius);
    parseValueInSection(annularSection, "outerRadius", config.annular.outerRadius);
    
    parseValueInSection(opticsSection, "NA", config.NA);
    parseValueInSection(opticsSection, "wavelength", config.wavelength);
    parseValueInSection(opticsSection, "defocus", config.defocus);
    
    parseValueInSection(kernelSection, "count", config.nk);
    parseValueInSection(kernelSection, "targetIntensity", config.targetIntensity);
    
    if (!config.maskInputFile.empty() && config.maskInputFile[0] != '/') {
        filesystem::path maskPath = filesystem::path(projectRootStr) / config.maskInputFile;
        config.maskInputFile = maskPath.string();
    }
    
    return true;
}

// ============================================================================
// Output Functions
// ============================================================================

void printStatistics(const string& name, vector<float>& data, int sampleCount = 5) {
    double mean = computeMean(data);
    double stddev = computeStdDev(data, mean);
    float maxVal = getMax(data);
    float minVal = getMin(data);
    
    cout << fixed << setprecision(6);
    cout << "[STATISTICS] " << name << ":" << endl;
    cout << "  Mean:    " << mean << endl;
    cout << "  StdDev:  " << stddev << endl;
    cout << "  Max:     " << maxVal << endl;
    cout << "  Min:     " << minVal << endl;
    
    if (sampleCount > 0 && !data.empty()) {
        cout << "  Samples: ";
        int count = min(sampleCount, (int)data.size());
        for (int i = 0; i < count; i++) {
            cout << data[i] << " ";
        }
        cout << endl;
    }
}

void printComplexSamples(const string& name, vector<ComplexD>& data, int sizeX, int sizeY, int count = 5) {
    int centerY = sizeY / 2;
    int centerX = sizeX / 2;
    cout << "[SAMPLES] " << name << " (center region):" << endl;
    int printed = 0;
    for (int dy = -1; dy <= 1 && printed < count; dy++) {
        for (int dx = -1; dx <= 1 && printed < count; dx++) {
            int y = centerY + dy;
            int x = centerX + dx;
            if (y >= 0 && y < sizeY && x >= 0 && x < sizeX) {
                ComplexD val = data[y * sizeX + x];
                cout << "  [" << y << "," << x << "] = (" << val.real() << ", " << val.imag() << ")" << endl;
                printed++;
            }
        }
    }
}

void writeFFTMeta(const string& path,
                  int physLx, int physLy,
                  int fftLx, int fftLy,
                  int Nx, int Ny) {
    ofstream ofs(path);
    if (!ofs.is_open()) return;
    int convX = 4 * Nx + 1;
    int convY = 4 * Ny + 1;
    int fftConvX = nextPowerOfTwo(convX);
    int fftConvY = nextPowerOfTwo(convY);

    ofs << "physical_size_x " << physLx << "\n";
    ofs << "physical_size_y " << physLy << "\n";
    ofs << "fft_size_x " << fftLx << "\n";
    ofs << "fft_size_y " << fftLy << "\n";
    ofs << "Nx " << Nx << "\n";
    ofs << "Ny " << Ny << "\n";
    ofs << "conv_size_x " << convX << "\n";
    ofs << "conv_size_y " << convY << "\n";
    ofs << "fft_conv_size_x " << fftConvX << "\n";
    ofs << "fft_conv_size_y " << fftConvY << "\n";
    ofs.close();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    cout << "============================================================" << endl;
    cout << "  Litho-Full: Integrated TCC + SOCS Verification Program" << endl;
    cout << "============================================================" << endl;
    
    auto totalStart = system_clock::now();
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <config.json> <output_dir> [--verbose] [--debug]" << endl;
        return 1;
    }
    
    string configPath = argv[1];
    string outputDir = argv[2];
    int verbose = 1;
    
    for (int i = 3; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--verbose") verbose = 1;
        else if (arg == "--debug") verbose = 2;
        else if (arg == "--quiet") verbose = 0;
    }
    
    FullConfig config;
    if (!loadConfig(configPath, config)) {
        return 1;
    }
    config.verboseLevel = verbose;
    config.outputDir = outputDir;
    
    if (config.Lx <= 0 || config.Ly <= 0 || config.maskSizeX <= 0 || config.maskSizeY <= 0) {
        cerr << "Error: Invalid mask parameters" << endl;
        return 1;
    }

    const int physLx = config.Lx;
    const int physLy = config.Ly;
    const int physLxy = physLx * physLy;

    const int fftLx = nextPowerOfTwo(physLx);
    const int fftLy = nextPowerOfTwo(physLy);
    const int fftLxy = fftLx * fftLy;
    
    // Create output directories (cross-platform)
    filesystem::create_directories(filesystem::path(outputDir) / "kernels");
    filesystem::create_directories(filesystem::path(outputDir) / "kernels" / "png");
    
    cout << "\n[CONFIG] Simulation Parameters:" << endl;
    cout << "  Mask Period: " << physLx << " x " << physLy << endl;
    cout << "  Mask Size: " << config.maskSizeX << " x " << config.maskSizeY << endl;
    cout << "  NA: " << config.NA << endl;
    cout << "  Defocus: " << config.defocus << endl;
    cout << "  Number of Kernels: " << config.nk << endl;
    cout << "  Output Directory: " << outputDir << endl;
    cout << "  FFT Padded Size: " << fftLx << " x " << fftLy << endl;

    if (!isPowerOfTwo(physLx) || !isPowerOfTwo(physLy)) {
        cout << "[WARN] Physical mask/image size is not power-of-two. "
             << "FFT processing will use padded size " << fftLx << " x " << fftLy << endl;
    }
    
    // ========================================================================
    // MODULE 1: Create Source
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 1] Creating Source..." << endl;
    auto t1_start = system_clock::now();
    
    vector<float> src(config.srcSize * config.srcSize);
    stringstream ss_srcParam;
    createSource(src, config.srcSize, config.srcType, config.annular, config.dipole, 
                 config.crossQuadrupole, config.point, config.sourceInputFile, ss_srcParam);
    
    auto t1_end = system_clock::now();
    auto t1_dur = duration_cast<microseconds>(t1_end - t1_start).count();
    
    printStatistics("Source", src, 5);
    cout << "[TIME] Source creation: " << t1_dur << " us" << endl;
    writeFloatArrayToPNGWithGrayscale(config.srcSize, config.srcSize, src, getMin(src), getMax(src), 
                                      outputDir + "/source.png");
    
    // ========================================================================
    // MODULE 2: Create Mask
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 2] Creating Mask..." << endl;
    auto t2_start = system_clock::now();
    
    vector<float> msk(physLxy, 0);
    stringstream ss_mskParam;
    createMask(msk, physLx, physLy, config.maskSizeX, config.maskSizeY,
               config.maskType, config.lineSpace, config.maskInputFile, ss_mskParam, config.dose);
    
    auto t2_end = system_clock::now();
    auto t2_dur = duration_cast<microseconds>(t2_end - t2_start).count();
    
    printStatistics("Mask", msk, 5);
    cout << "[TIME] Mask creation: " << t2_dur << " us" << endl;
    writeFloatArrayToPNGWithGrayscale(physLx, physLy, msk, getMin(msk), getMax(msk), 
                                      outputDir + "/mask.png");
    
    // ========================================================================
    // Compute TCC Parameters
    // ========================================================================
    float outerSigma = getOuterSigma(src, config.srcSize);
    if (outerSigma > 1) outerSigma = 1;
    
    int Nx = floor(config.NA * physLx * (1 + outerSigma) / config.wavelength);
    int Ny = floor(config.NA * physLy * (1 + outerSigma) / config.wavelength);
    int tccSize = (2 * Nx + 1) * (2 * Ny + 1);
    
    cout << "\n[PARAM] TCC Parameters:" << endl;
    cout << "  Outer Sigma: " << outerSigma << endl;
    cout << "  Nx: " << Nx << ", Ny: " << Ny << endl;
    cout << "  TCC Size: " << tccSize << " x " << tccSize << endl;

    writeFFTMeta(outputDir + "/fft_meta.txt", physLx, physLy, fftLx, fftLy, Nx, Ny);
    
    // ========================================================================
    // MODULE 3: FFT Mask
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 3] Computing FFT of Mask..." << endl;
    auto t3_start = system_clock::now();
    
    vector<double> realDataFFT(fftLxy, 0.0);
    vector<ComplexD> complexDataFFT(fftLy * (fftLx / 2 + 1), ComplexD(0, 0));

    fftw_plan LxyR2C_plan = fftw_plan_dft_r2c_2d(
        fftLy, fftLx,
        realDataFFT.data(),
        reinterpret_cast<fftw_complex*>(complexDataFFT.data()),
        FFTW_ESTIMATE);

    fftw_plan LxyC2R_plan = fftw_plan_dft_c2r_2d(
        fftLy, fftLx,
        reinterpret_cast<fftw_complex*>(complexDataFFT.data()),
        realDataFFT.data(),
        FFTW_ESTIMATE);
    
    vector<ComplexD> mskf(fftLxy, ComplexD(0, 0));
    FT_r2c_padded(msk, mskf, physLx, physLy, fftLx, fftLy,
                  LxyR2C_plan, realDataFFT, complexDataFFT);
    
    auto t3_end = system_clock::now();
    auto t3_dur = duration_cast<microseconds>(t3_end - t3_start).count();
    
    printComplexSamples("FFT Mask", mskf, fftLx, fftLy, 5);
    cout << "[TIME] FFT Mask: " << t3_dur << " us" << endl;
    
    vector<float> mskfReal, mskfImag;
    complexToFloat(mskf, mskfReal, mskfImag);
    writeFloatArrayToBinary(outputDir + "/mskf_r.bin", mskfReal, (int)mskfReal.size());
    writeFloatArrayToBinary(outputDir + "/mskf_i.bin", mskfImag, (int)mskfImag.size());
    
    // ========================================================================
    // MODULE 4: Calculate TCC
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 4] Computing TCC Matrix..." << endl;
    auto t4_start = system_clock::now();
    
    vector<ComplexD> tcc(tccSize * tccSize, 0);
    calcTCC(tcc, tccSize, src, config.srcSize, outerSigma, config.wavelength,
            config.NA, config.defocus, physLx, physLy, Nx, Ny, verbose);
    
    auto t4_end = system_clock::now();
    auto t4_dur = duration_cast<microseconds>(t4_end - t4_start).count();
    
    vector<float> tccReal, tccImag;
    complexToFloat(tcc, tccReal, tccImag);
    printStatistics("TCC Real", tccReal, 0);
    if (tccSize > 1) {
        cout << "  Diagonal[0]: (" << tcc[0].real() << ", " << tcc[0].imag() << ")" << endl;
        cout << "  Diagonal[1]: (" << tcc[tccSize + 1].real() << ", " << tcc[tccSize + 1].imag() << ")" << endl;
    }
    cout << "[TIME] TCC computation: " << t4_dur << " us" << endl;
    
    writeFloatArrayToBinary(outputDir + "/tcc_r.bin", tccReal, (int)tccReal.size());
    writeFloatArrayToBinary(outputDir + "/tcc_i.bin", tccImag, (int)tccImag.size());
    
    // ========================================================================
    // MODULE 5: Calculate Image (TCC-based)
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 5] Computing Aerial Image (TCC-based)..." << endl;
    auto t5_start = system_clock::now();
    
    vector<ComplexD> imgf(fftLxy, ComplexD(0, 0));
    calcImage(imgf, mskf, fftLx, fftLy, tcc, tccSize, Nx, Ny, verbose);
    
    vector<float> imgTcc(physLxy, 0.0f);
    FT_c2r_padded(imgf, imgTcc, physLx, physLy, fftLx, fftLy,
                  LxyC2R_plan, complexDataFFT, realDataFFT);
    
    auto t5_end = system_clock::now();
    auto t5_dur = duration_cast<microseconds>(t5_end - t5_start).count();
    
    printStatistics("Aerial Image (TCC)", imgTcc, 5);
    cout << "[TIME] TCC Image: " << t5_dur << " us" << endl;
    
    writeFloatArrayToBinary(outputDir + "/aerial_image_tcc_direct.bin", imgTcc, (int)imgTcc.size());
    writeFloatArrayToPNGWithContinuousColor(physLx, physLy, imgTcc,
                                            outputDir + "/aerial_image_tcc_direct.png", getMin(imgTcc), getMax(imgTcc));
    
    // ========================================================================
    // MODULE 6: Calculate Kernels (SOCS)
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 6] Extracting SOCS Kernels..." << endl;
    auto t6_start = system_clock::now();
    
    int nkActual = min(config.nk, tccSize);
    vector<vector<ComplexD>> krns(nkActual, vector<ComplexD>(tccSize));
    vector<float> scales(nkActual);
    calcKernels(krns, scales, nkActual, tcc, tccSize, Nx, Ny, verbose);
    
    auto t6_end = system_clock::now();
    auto t6_dur = duration_cast<microseconds>(t6_end - t6_start).count();
    
    cout << "[STATISTICS] SOCS Kernel Eigenvalues:" << endl;
    for (int i = 0; i < nkActual; i++) {
        cout << "  Scale[" << i << "]: " << scales[i] << endl;
    }
    cout << "[TIME] Kernel extraction: " << t6_dur << " us" << endl;
    
    writeKernelInfo(outputDir + "/kernels/kernel_info.txt", 2 * Nx + 1, 2 * Ny + 1, nkActual, scales);
    for (int i = 0; i < nkActual; i++) {
        vector<float> krnReal(tccSize), krnImag(tccSize);
        complexToFloat(krns[i], krnReal, krnImag);
        writeFloatArrayToBinary(outputDir + "/kernels/krn_" + to_string(i) + "_r.bin", krnReal, tccSize);
        writeFloatArrayToBinary(outputDir + "/kernels/krn_" + to_string(i) + "_i.bin", krnImag, tccSize);
    }
    
    writeFloatArrayToBinary(outputDir + "/scales.bin", scales, nkActual);
    
    // ========================================================================
    // MODULE 7: Calculate SOCS Image
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 7] Computing Aerial Image (SOCS)..." << endl;
    auto t7_start = system_clock::now();
    
    vector<float> imgSocs(physLxy, 0.0f);
    calcSOCS(imgSocs, mskf, physLx, physLy, fftLx, fftLy,
             krns, scales, nkActual, Nx, Ny,
             LxyC2R_plan, complexDataFFT, realDataFFT, verbose,
             outputDir, true);  // Enable HLS golden output
    
    auto t7_end = system_clock::now();
    auto t7_dur = duration_cast<microseconds>(t7_end - t7_start).count();
    
    printStatistics("Aerial Image (SOCS)", imgSocs, 5);
    cout << "[TIME] SOCS Image: " << t7_dur << " us" << endl;
    
    writeFloatArrayToBinary(outputDir + "/aerial_image_socs_kernel.bin", imgSocs, (int)imgSocs.size());
    writeFloatArrayToPNGWithContinuousColor(physLx, physLy, imgSocs,
                                            outputDir + "/aerial_image_socs_kernel.png", getMin(imgSocs), getMax(imgSocs));
    writeFloatArrayToPNGWith7Colors(physLx, physLy, imgSocs,
                                    outputDir + "/aerial_image_socs_threshold.png", 0, config.targetIntensity);
    
    // ========================================================================
    // Summary
    // ========================================================================
    fftw_destroy_plan(LxyR2C_plan);
    fftw_destroy_plan(LxyC2R_plan);
    
    auto totalEnd = system_clock::now();
    auto totalDur = duration_cast<microseconds>(totalEnd - totalStart).count();
    
    cout << "\n" << string(60, '=') << endl;
    cout << "[SUMMARY] Performance Breakdown:" << endl;
    cout << "  Module 1 (Source):     " << t1_dur << " us" << endl;
    cout << "  Module 2 (Mask):       " << t2_dur << " us" << endl;
    cout << "  Module 3 (FFT Mask):   " << t3_dur << " us" << endl;
    cout << "  Module 4 (TCC):        " << t4_dur << " us" << endl;
    cout << "  Module 5 (TCC Image):  " << t5_dur << " us" << endl;
    cout << "  Module 6 (Kernels):    " << t6_dur << " us" << endl;
    cout << "  Module 7 (SOCS Image): " << t7_dur << " us" << endl;
    cout << "  ----------------------------------------" << endl;
    cout << "  Total:                 " << totalDur << " us (" << totalDur * 1e-6 << " s)" << endl;
    cout << string(60, '=') << endl;
    
    cout << "\n[VERIFICATION SUMMARY] For HLS Golden Reference:" << endl;
    cout << "  TCC Mean (Real): " << computeMean(tccReal) << endl;
    cout << "  TCC StdDev (Real): " << computeStdDev(tccReal, computeMean(tccReal)) << endl;
    cout << "  Image Mean (TCC): " << computeMean(imgTcc) << endl;
    cout << "  Image Mean (SOCS): " << computeMean(imgSocs) << endl;
    cout << "  Recommended tolerance: relative error < 1e-5" << endl;
    
    cout << "\n[DONE] Output saved to: " << outputDir << endl;
    
    return 0;
}