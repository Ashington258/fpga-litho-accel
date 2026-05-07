#include "mask.hpp"
#include "file_io.hpp"
#include <iostream>

void generateLineSpace(std::vector<float>& msk, int sizeX, int sizeY, LineSpace& lineSpace) {
    int period = lineSpace.lineWidth + lineSpace.spaceWidth;
    if (period <= 0) {
        return;
    }
    if (lineSpace.isHorizontal) {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                if ((y % period) < lineSpace.lineWidth) {
                    msk[y * sizeX + x] = 1.0f;
                } else {
                    msk[y * sizeX + x] = 0.0f;
                }
            }
        }
    } else {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                if ((x % period) < lineSpace.lineWidth) {
                    msk[y * sizeX + x] = 1.0f;
                } else {
                    msk[y * sizeX + x] = 0.0f;
                }
            }
        }
    }
}

void createMask(std::vector<float>& msk, int Lx, int Ly, int maskSizeX, int maskSizeY,
                std::string maskType, LineSpace& lineSpace,
                std::string maskInFile, std::stringstream& ss_mskParam, float dose) {
    std::vector<float> inMsk(maskSizeX * maskSizeY, 0.0f);
    ss_mskParam << "Mask Period: " << Lx << " x " << Ly << "\n";
    ss_mskParam << "Mask Size: " << maskSizeX << " x " << maskSizeY << "\n";
    ss_mskParam << "Mask Type: " << maskType << "\n";

    if (maskType == "LineSpace") {
        ss_mskParam << "Line Width: " << lineSpace.lineWidth << "\n";
        ss_mskParam << "Space Width: " << lineSpace.spaceWidth << "\n";
        generateLineSpace(inMsk, maskSizeX, maskSizeY, lineSpace);
    } else if (maskType == "Import") {
        std::cout << "Read Mask: " << maskInFile << std::endl;
        std::string extension;
        std::size_t pos = maskInFile.find_last_of('.');
        if (pos != std::string::npos) {
            extension = maskInFile.substr(pos + 1);
        }
        if (extension == "bin") {
            readFloatArrayFromBinary(maskInFile, inMsk, static_cast<size_t>(maskSizeX * maskSizeY));
        } else {
            std::cerr << "Error: Unsupported mask input file extension: " << extension << std::endl;
            std::exit(1);
        }
    } else {
        std::cerr << "Error: Unsupported mask type: " << maskType << std::endl;
        std::exit(1);
    }

    int difYh = (Ly - maskSizeY) / 2;
    int difXh = (Lx - maskSizeX) / 2;
    for (int y = 0; y < maskSizeY; y++) {
        for (int x = 0; x < maskSizeX; x++) {
            msk[(y + difYh) * Lx + x + difXh] = inMsk[y * maskSizeX + x] * dose;
        }
    }
}
