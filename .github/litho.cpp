/**
 * Litho-Full: Integrated TCC + SOCS Verification Program
 * ========================================================
 * This program combines the full lithography simulation flow:
 *   1. Source generation
 *   2. Mask loading/creation
 *   3. TCC matrix computation
 *   4. SOCS kernel extraction
 *   5. Aerial image computation via SOCS
 * 
 * Output: Detailed debug logs for golden reference verification
 * 
 * Usage: klitho_full <config.json> <output_dir> [--verbose] [--debug]
 */

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
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); i++) sum += data[i];
    return sum / data.size();
}

double computeStdDev(vector<float>& data, double mean) {
    double var = 0.0;
    for (size_t i = 0; i < data.size(); i++) {
        double diff = data[i] - mean;
        var += diff * diff;
    }
    return sqrt(var / data.size());
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

void complexToFloat(vector<ComplexD>& dataComplex, vector<float>& dataReal, vector<float>& dataImag) {
    int size = dataComplex.size();
    dataReal.resize(size);
    dataImag.resize(size);
    for (int i = 0; i < size; i++) {
        dataReal[i] = static_cast<float>(dataComplex[i].real());
        dataImag[i] = static_cast<float>(dataComplex[i].imag());
    }
}

// ============================================================================
// FFT Functions
// ============================================================================

void FT_r2c(vector<float>& in, vector<ComplexD>& out, int sizeX, int sizeY,
            fftw_plan &fwd, vector<double>& datar, vector<ComplexD>& datac) {
    // Convert float to double and shift
    vector<float> shifted(sizeX * sizeY);
    myShift(in, shifted, sizeX, sizeY, false, false);
    for (int i = 0; i < sizeX * sizeY; i++) {
        datar[i] = static_cast<double>(shifted[i]);
    }
    fftw_execute(fwd);
    
    int sizeXh = sizeX / 2;
    int wt = (sizeX / 2) + 1;
    int AY = (sizeY + 1) / 2;
    int BY = sizeY - AY;
    vector<ComplexD> R(wt * sizeY, 0);
    vector<ComplexD> L(wt * sizeY, 0);
    
    for (int y = 0, y2 = AY; y < BY; y++, y2++) {
        for (int x = 0; x < wt; x++) {
            R[y * wt + x] = datac[y2 * wt + x];
        }
    }
    for (int y = BY, y2 = 0; y < sizeY; y++, y2++) {
        for (int x = 0; x < wt; x++) {
            R[y * wt + x] = datac[y2 * wt + x];
        }
    }
    
    if (sizeY % 2 == 0) {
        for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
            L[x] = conj(R[x2]);
        }
        for (int y = 1, y2 = sizeY - 1; y < sizeY; y++, y2--) {
            for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
                L[y * wt + x] = conj(R[y2 * wt + x2]);
            }
        }
    } else {
        for (int y = 0, y2 = sizeY - 1; y < sizeY; y++, y2--) {
            for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
                L[y * wt + x] = conj(R[y2 * wt + x2]);
            }
        }
    }
    
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < wt; x++) {
            out[y * sizeX + x] = L[y * wt + x];
        }
    }
    for (int y = 0; y < sizeY; y++) {
        for (int x = sizeXh, x2 = 0; x < sizeX; x++, x2++) {
            out[y * sizeX + x] = R[y * wt + x2];
        }
    }
    for (int i = 0; i < sizeX * sizeY; i++) {
        out[i] /= (sizeX * sizeY);
    }
}

void FT_c2r(vector<ComplexD>& in, vector<float>& out, int sizeX, int sizeY,
            fftw_plan &bwd, vector<ComplexD>& datac, vector<double>& datar) {
    int wt = (sizeX / 2) + 1;
    int xh = (sizeX + 1) / 2;
    int yh = (sizeY + 1) / 2;
    
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < wt; x++) {
            datac[y * wt + x] = 0;
        }
    }
    for (int y = 0; y < sizeY; y++) {
        for (int x = xh - 1; x < sizeX; x++) {
            int sx = (x + xh) % sizeX;
            int sy = (y + yh) % sizeY;
            datac[sy * wt + sx] = in[y * sizeX + x];
        }
    }
    if (sizeX % 2 == 0) {
        vector<ComplexD> R(sizeY);
        R[0] = conj(in[0]);
        for (int y = 1, y2 = sizeY - 1; y < sizeY; y++, y2--) {
            R[y] = conj(in[y2 * sizeX]);
        }
        for (int y = 0; y < sizeY; y++) {
            datac[y * wt + wt - 1] = R[y];
        }
    }
    
    fftw_execute(bwd);
    // Convert double to float and shift
    vector<double> shifted_d(sizeX * sizeY);
    myShift(datar, shifted_d, sizeX, sizeY, false, false);
    for (int i = 0; i < sizeX * sizeY; i++) {
        out[i] = static_cast<float>(shifted_d[i]);
    }
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
    return max_distance / center;
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
// calcImage (TCC-based aerial image)
// ============================================================================

void calcImage(vector<ComplexD>& imgf, vector<ComplexD>& msk, int Lx, int Ly,
               vector<ComplexD>& tcc, int tccSize, int Nx, int Ny, int verbose) {
    int tccSizeh = (tccSize - 1) / 2;
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    if (verbose >= 1) {
        cout << "[calcImage] Computing aerial image from TCC..." << endl;
    }
    
    for (int ny2 = -2 * Ny; ny2 <= 2 * Ny; ny2++) {
        for (int nx2 = 0; nx2 <= 2 * Nx; nx2++) {
            ComplexD val = 0;
            for (int ny1 = -Ny; ny1 <= Ny; ny1++) {
                for (int nx1 = -Nx; nx1 <= Nx; nx1++) {
                    if (abs(nx2 + nx1) <= Nx && abs(ny2 + ny1) <= Ny) {
                        val += msk[(ny2 + ny1 + Lyh) * Lx + (nx2 + nx1 + Lxh)]
                             * conj(msk[(ny1 + Lyh) * Lx + (nx1 + Lxh)])
                             * tcc[((ny1) * (2 * Nx + 1) + nx1 + tccSizeh) * tccSize 
                                 + ((ny2 + ny1) * (2 * Nx + 1) + (nx2 + nx1) + tccSizeh)];
                    }
                }
            }
            imgf[(ny2 + Lyh) * Lx + (nx2 + Lxh)] = val;
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
    
    zheevr_('V', 'I', 'U', &n, a, &lda, &vl, &vu, &il, &iu,
            &abstol, &m, w, z, &ldz, isuppz, &wkopt, &lwork, &rwkopt, &lrwork, &iwkopt, &liwork, &info);
    
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
        cout << "[calcKernels] Eigenvalues: ";
        for (int j = m - 1; j >= 0; j--) {
            cout << w[j] << " ";
            scales[m - 1 - j] = static_cast<float>(w[j]);
        }
        cout << endl;
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
// calcSOCS
// ============================================================================

template<typename T1, typename T2>
void FI(vector<T1>& in, int in_sizeX, int in_sizeY,
        vector<T2>& out, int out_sizeX, int out_sizeY,
        fftw_plan &R2C_plan, vector<double>& R2CDataReal, vector<ComplexD>& R2CDataComplex,
        fftw_plan &C2R_plan, vector<ComplexD>& C2RDataComplex, vector<double>& C2RDataReal) {
    int difY = out_sizeY - in_sizeY;
    int in_sizeXY = in_sizeX * in_sizeY;
    
    fill(R2CDataReal.begin(), R2CDataReal.end(), 0);
    fill(R2CDataComplex.begin(), R2CDataComplex.end(), 0);
    
    // Handle type conversion for shift
    vector<T1> shifted_in(in_sizeX * in_sizeY);
    myShift(in, shifted_in, in_sizeX, in_sizeY, false, false);
    for (int i = 0; i < in_sizeX * in_sizeY; i++) {
        R2CDataReal[i] = static_cast<double>(shifted_in[i]);
    }
    
    fftw_execute(R2C_plan);
    for (int i = 0; i < in_sizeY * (in_sizeX / 2 + 1); i++) {
        R2CDataComplex[i] /= in_sizeXY;
    }
    
    fill(C2RDataComplex.begin(), C2RDataComplex.end(), 0);
    for (int y = 0; y < in_sizeY / 2 + 1; y++) {
        for (int x = 0; x < in_sizeX / 2 + 1; x++) {
            C2RDataComplex[y * (out_sizeX / 2 + 1) + x] = R2CDataComplex[y * (in_sizeX / 2 + 1) + x];
        }
    }
    for (int y = in_sizeY / 2 + 1; y < in_sizeY; y++) {
        for (int x = 0; x < in_sizeX / 2 + 1; x++) {
            C2RDataComplex[(y + difY) * (out_sizeX / 2 + 1) + x] = R2CDataComplex[y * (in_sizeX / 2 + 1) + x];
        }
    }
    
    fill(C2RDataReal.begin(), C2RDataReal.end(), 0);
    fftw_execute(C2R_plan);
    
    // Handle type conversion for shift
    vector<double> shifted_out(out_sizeX * out_sizeY);
    myShift(C2RDataReal, shifted_out, out_sizeX, out_sizeY, false, false);
    for (int i = 0; i < out_sizeX * out_sizeY; i++) {
        out[i] = static_cast<T2>(shifted_out[i]);
    }
}

void calcSOCS(vector<float>& image, vector<ComplexD>& msk, int Lx, int Ly,
              vector<vector<ComplexD>>& krns, vector<float>& scales, int nk, int Nx, int Ny,
              fftw_plan& LxyC2R_plan, vector<ComplexD>& complexDataLxy, vector<double>& realDataLxy,
              int verbose) {
    // Convolution size: 4*Nx+1 (e.g., 17 for Nx=4, 65 for Nx=16)
    int sizeX = 4 * Nx + 1;
    int sizeY = 4 * Ny + 1;
    
    // Data placement offset in convolution array
    int difX = sizeX - (2 * Nx + 1);
    int difY = sizeY - (2 * Ny + 1);
    
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    if (verbose >= 1) {
        cout << "[calcSOCS] Computing aerial image via SOCS..." << endl;
        cout << "[calcSOCS] Convolution size: " << sizeX << " x " << sizeY << endl;
        cout << "[calcSOCS] Number of kernels: " << nk << endl;
    }
    
    // FFT buffers for IFFT (complex-to-complex)
    vector<ComplexD> bwdDataIn(sizeX * sizeY, ComplexD(0, 0));
    vector<ComplexD> bwdDataOut(sizeX * sizeY, ComplexD(0, 0));
    fftw_plan bwd_plan = fftw_plan_dft_2d(sizeY, sizeX, 
        reinterpret_cast<fftw_complex*>(bwdDataIn.data()), 
        reinterpret_cast<fftw_complex*>(bwdDataOut.data()), 
        FFTW_BACKWARD, FFTW_ESTIMATE);
    
    // FI interpolation buffers (R2C for Fourier Interpolation)
    vector<double> realDataNxy(sizeX * sizeY);
    vector<ComplexD> complexDataNxy((sizeX / 2 + 1) * sizeY);
    fftw_plan NxyR2C = fftw_plan_dft_r2c_2d(sizeY, sizeX, realDataNxy.data(), 
        reinterpret_cast<fftw_complex*>(complexDataNxy.data()), FFTW_ESTIMATE);
    
    // Accumulated intensity image
    vector<double> tmpImg(sizeX * sizeY, 0);
    vector<double> tmpImgp(sizeX * sizeY, 0);
    
    // Compute SOCS aerial image: sum of |IFFT(kernel * mask)|^2 weighted by eigenvalues
    for (int k = 0; k < nk; ++k) {
        // Place kernel*mask product into convolution array
        for (int y = -Ny; y <= Ny; ++y) {
            for (int x = -Nx; x <= Nx; ++x) {
                bwdDataIn[(difY + y + Ny) * sizeX + difX + x + Nx] = 
                    krns[k][(y + Ny) * (2 * Nx + 1) + x + Nx] * msk[(y + Lyh) * Lx + x + Lxh];
            }
        }
        
        // Execute IFFT (frequency domain -> spatial domain)
        fftw_execute(bwd_plan);
        
        // Accumulate intensity: |amplitude|^2 weighted by eigenvalue (scale)
        for (int i = 0; i < sizeX * sizeY; ++i) {
            tmpImg[i] += scales[k] * (bwdDataOut[i].real() * bwdDataOut[i].real() 
                                    + bwdDataOut[i].imag() * bwdDataOut[i].imag());
        }
    }
    
    // Shift array (move zero-frequency to center) and apply Fourier Interpolation
    myShift(tmpImg, tmpImgp, sizeX, sizeY, true, true);
    FI(tmpImgp, sizeX, sizeY, image, Lx, Ly, NxyR2C, realDataNxy, complexDataNxy, 
       LxyC2R_plan, complexDataLxy, realDataLxy);
    
    fftw_destroy_plan(bwd_plan);
    fftw_destroy_plan(NxyR2C);
}

// ============================================================================
// JSON Parser (Simple)
// ============================================================================

// Extract a section from JSON content
string extractSection(const string& content, const string& sectionName) {
    // Find section start: "sectionName": {
    size_t pos = content.find("\"" + sectionName + "\"");
    if (pos == string::npos) return "";
    pos = content.find("{", pos);
    if (pos == string::npos) return "";
    
    // Find matching closing brace
    int braceCount = 1;
    size_t end = pos + 1;
    while (end < content.length() && braceCount > 0) {
        if (content[end] == '{') braceCount++;
        else if (content[end] == '}') braceCount--;
        end++;
    }
    
    return content.substr(pos, end - pos);
}

// Parse value from a section string
bool parseValueInSection(const string& section, const string& key, int& value) {
    size_t pos = section.find("\"" + key + "\"");
    if (pos == string::npos) return false;
    pos = section.find(":", pos);
    if (pos == string::npos) return false;
    // Skip whitespace
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
    
    // Get project root directory from config path
    filesystem::path configDir = filesystem::path(configPath).parent_path();
    filesystem::path projectRoot = configDir.parent_path().parent_path();
    string projectRootStr = projectRoot.string();
    
    // Extract sections
    string maskSection = extractSection(content, "mask");
    string maskPeriodSection = extractSection(maskSection, "period");
    string maskSizeSection = extractSection(maskSection, "size");
    string lineSpaceSection = extractSection(maskSection, "lineSpace");
    
    string sourceSection = extractSection(content, "source");
    string annularSection = extractSection(sourceSection, "annular");
    
    string opticsSection = extractSection(content, "optics");
    string kernelSection = extractSection(content, "kernel");
    
    // Parse mask parameters
    parseValueInSection(maskPeriodSection, "Lx", config.Lx);
    parseValueInSection(maskPeriodSection, "Ly", config.Ly);
    parseValueInSection(maskSizeSection, "maskSizeX", config.maskSizeX);
    parseValueInSection(maskSizeSection, "maskSizeY", config.maskSizeY);
    parseValueInSection(maskSection, "type", config.maskType);
    parseValueInSection(lineSpaceSection, "lineWidth", config.lineSpace.lineWidth);
    parseValueInSection(lineSpaceSection, "spaceWidth", config.lineSpace.spaceWidth);
    parseValueInSection(maskSection, "inputFile", config.maskInputFile);
    parseValueInSection(maskSection, "dose", config.dose);
    
    // Parse source parameters
    parseValueInSection(sourceSection, "gridSize", config.srcSize);
    parseValueInSection(sourceSection, "type", config.srcType);
    parseValueInSection(annularSection, "innerRadius", config.annular.innerRadius);
    parseValueInSection(annularSection, "outerRadius", config.annular.outerRadius);
    
    // Parse optics parameters
    parseValueInSection(opticsSection, "NA", config.NA);
    parseValueInSection(opticsSection, "wavelength", config.wavelength);
    parseValueInSection(opticsSection, "defocus", config.defocus);
    
    // Parse kernel parameters
    parseValueInSection(kernelSection, "count", config.nk);
    parseValueInSection(kernelSection, "targetIntensity", config.targetIntensity);
    
    // Convert relative paths to absolute
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
    
    // Validate parameters
    if (config.Lx <= 0 || config.Ly <= 0 || config.maskSizeX <= 0 || config.maskSizeY <= 0) {
        cerr << "Error: Invalid mask parameters" << endl;
        return 1;
    }
    
    int Lxy = config.Lx * config.Ly;
    
    // Create output directories
    system(("mkdir -p " + outputDir + "/kernels/png").c_str());
    
    // Print configuration
    cout << "\n[CONFIG] Simulation Parameters:" << endl;
    cout << "  Mask Period: " << config.Lx << " x " << config.Ly << endl;
    cout << "  Mask Size: " << config.maskSizeX << " x " << config.maskSizeY << endl;
    cout << "  NA: " << config.NA << endl;
    cout << "  Defocus: " << config.defocus << endl;
    cout << "  Number of Kernels: " << config.nk << endl;
    cout << "  Output Directory: " << outputDir << endl;
    
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
    
    vector<float> msk(Lxy, 0);
    stringstream ss_mskParam;
    createMask(msk, config.Lx, config.Ly, config.maskSizeX, config.maskSizeY,
               config.maskType, config.lineSpace, config.maskInputFile, ss_mskParam, config.dose);
    
    auto t2_end = system_clock::now();
    auto t2_dur = duration_cast<microseconds>(t2_end - t2_start).count();
    
    printStatistics("Mask", msk, 5);
    cout << "[TIME] Mask creation: " << t2_dur << " us" << endl;
    writeFloatArrayToPNGWithGrayscale(config.Lx, config.Ly, msk, getMin(msk), getMax(msk), 
                                       outputDir + "/mask.png");
    
    // ========================================================================
    // Compute TCC Parameters
    // ========================================================================
    float outerSigma = getOuterSigma(src, config.srcSize);
    if (outerSigma > 1) outerSigma = 1;
    
    int Nx = floor(config.NA * config.Lx * (1 + outerSigma) / config.wavelength);
    int Ny = floor(config.NA * config.Ly * (1 + outerSigma) / config.wavelength);
    int tccSize = (2 * Nx + 1) * (2 * Ny + 1);
    
    cout << "\n[PARAM] TCC Parameters:" << endl;
    cout << "  Outer Sigma: " << outerSigma << endl;
    cout << "  Nx: " << Nx << ", Ny: " << Ny << endl;
    cout << "  TCC Size: " << tccSize << " x " << tccSize << endl;
    
    // ========================================================================
    // MODULE 3: FFT Mask
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 3] Computing FFT of Mask..." << endl;
    auto t3_start = system_clock::now();
    
    vector<double> realDataLxy(Lxy, 0);
    vector<ComplexD> complexDataLxy(config.Ly * (config.Lx / 2 + 1), 0);
    fftw_plan LxyR2C_plan = fftw_plan_dft_r2c_2d(config.Ly, config.Lx, realDataLxy.data(), 
        reinterpret_cast<fftw_complex*>(complexDataLxy.data()), FFTW_ESTIMATE);
    fftw_plan LxyC2R_plan = fftw_plan_dft_c2r_2d(config.Ly, config.Lx, 
        reinterpret_cast<fftw_complex*>(complexDataLxy.data()), realDataLxy.data(), FFTW_ESTIMATE);
    
    vector<ComplexD> mskf(Lxy);
    FT_r2c(msk, mskf, config.Lx, config.Ly, LxyR2C_plan, realDataLxy, complexDataLxy);
    
    auto t3_end = system_clock::now();
    auto t3_dur = duration_cast<microseconds>(t3_end - t3_start).count();
    
    printComplexSamples("FFT Mask", mskf, config.Lx, config.Ly, 5);
    cout << "[TIME] FFT Mask: " << t3_dur << " us" << endl;
    
    // Write mskf for SOCS HLS golden input
    vector<float> mskfReal(Lxy), mskfImag(Lxy);
    complexToFloat(mskf, mskfReal, mskfImag);
    writeFloatArrayToBinary(outputDir + "/mskf_r.bin", mskfReal, Lxy);
    writeFloatArrayToBinary(outputDir + "/mskf_i.bin", mskfImag, Lxy);
    
    // ========================================================================
    // MODULE 4: Calculate TCC
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 4] Computing TCC Matrix..." << endl;
    auto t4_start = system_clock::now();
    
    vector<ComplexD> tcc(tccSize * tccSize, 0);
    calcTCC(tcc, tccSize, src, config.srcSize, outerSigma, config.wavelength, 
            config.NA, config.defocus, config.Lx, config.Ly, Nx, Ny, verbose);
    
    auto t4_end = system_clock::now();
    auto t4_dur = duration_cast<microseconds>(t4_end - t4_start).count();
    
    // TCC statistics
    vector<float> tccReal, tccImag;
    complexToFloat(tcc, tccReal, tccImag);
    printStatistics("TCC Real", tccReal, 0);
    cout << "  Diagonal[0]: (" << tcc[0].real() << ", " << tcc[0].imag() << ")" << endl;
    cout << "  Diagonal[1]: (" << tcc[tccSize+1].real() << ", " << tcc[tccSize+1].imag() << ")" << endl;
    cout << "[TIME] TCC computation: " << t4_dur << " us" << endl;
    
    writeFloatArrayToBinary(outputDir + "/tcc_r.bin", tccReal, tccSize * tccSize);
    writeFloatArrayToBinary(outputDir + "/tcc_i.bin", tccImag, tccSize * tccSize);
    
    // ========================================================================
    // MODULE 5: Calculate Image (TCC-based)
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 5] Computing Aerial Image (TCC-based)..." << endl;
    auto t5_start = system_clock::now();
    
    vector<ComplexD> imgf(Lxy);
    calcImage(imgf, mskf, config.Lx, config.Ly, tcc, tccSize, Nx, Ny, verbose);
    
    vector<float> imgTcc(Lxy);
    FT_c2r(imgf, imgTcc, config.Lx, config.Ly, LxyC2R_plan, complexDataLxy, realDataLxy);
    
    auto t5_end = system_clock::now();
    auto t5_dur = duration_cast<microseconds>(t5_end - t5_start).count();
    
    printStatistics("Aerial Image (TCC)", imgTcc, 5);
    cout << "[TIME] TCC Image: " << t5_dur << " us" << endl;
    
    // TCC-based aerial image (direct TCC matrix calculation)
    writeFloatArrayToBinary(outputDir + "/aerial_image_tcc_direct.bin", imgTcc, Lxy);
    writeFloatArrayToPNGWithContinuousColor(config.Lx, config.Ly, imgTcc, 
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
    
    // Write kernels
    writeKernelInfo(outputDir + "/kernels/kernel_info.txt", 2*Nx+1, 2*Ny+1, nkActual, scales);
    for (int i = 0; i < nkActual; i++) {
        vector<float> krnReal(tccSize), krnImag(tccSize);
        complexToFloat(krns[i], krnReal, krnImag);
        writeFloatArrayToBinary(outputDir + "/kernels/krn_" + to_string(i) + "_r.bin", krnReal, tccSize);
        writeFloatArrayToBinary(outputDir + "/kernels/krn_" + to_string(i) + "_i.bin", krnImag, tccSize);
    }
    
    // Write scales for SOCS HLS golden input
    writeFloatArrayToBinary(outputDir + "/scales.bin", scales, nkActual);
    
    // ========================================================================
    // MODULE 7: Calculate SOCS Image
    // ========================================================================
    cout << "\n" << string(60, '=') << endl;
    cout << "[MODULE 7] Computing Aerial Image (SOCS)..." << endl;
    auto t7_start = system_clock::now();
    
    vector<float> imgSocs(Lxy);
    calcSOCS(imgSocs, mskf, config.Lx, config.Ly, krns, scales, nkActual, Nx, Ny,
             LxyC2R_plan, complexDataLxy, realDataLxy, verbose);
    
    auto t7_end = system_clock::now();
    auto t7_dur = duration_cast<microseconds>(t7_end - t7_start).count();
    
    printStatistics("Aerial Image (SOCS)", imgSocs, 5);
    cout << "[TIME] SOCS Image: " << t7_dur << " us" << endl;
    
    // SOCS-based aerial image (SOCS kernel approximation method)
    writeFloatArrayToBinary(outputDir + "/aerial_image_socs_kernel.bin", imgSocs, Lxy);
    writeFloatArrayToPNGWithContinuousColor(config.Lx, config.Ly, imgSocs, 
                                             outputDir + "/aerial_image_socs_kernel.png", getMin(imgSocs), getMax(imgSocs));
    writeFloatArrayToPNGWith7Colors(config.Lx, config.Ly, imgSocs, 
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