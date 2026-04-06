#ifndef HOST_MASK_HPP
#define HOST_MASK_HPP

#include <string>
#include <vector>
#include <sstream>

struct LineSpace {
    bool isHorizontal;
    int lineWidth;
    int spaceWidth;
};

void generateLineSpace(std::vector<float>& msk, int sizeX, int sizeY, LineSpace& lineSpace);
void createMask(std::vector<float>& msk, int Lx, int Ly, int maskSizeX, int maskSizeY,
                std::string maskType, LineSpace& lineSpace,
                std::string maskInFile, std::stringstream& ss_mskParam, float dose);

#endif // HOST_MASK_HPP
