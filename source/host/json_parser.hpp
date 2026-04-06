#ifndef HOST_JSON_PARSER_HPP
#define HOST_JSON_PARSER_HPP

#include <string>
#include <vector>
#include "mask.hpp"

struct SOCSConfig {
    int Lx = 0;
    int Ly = 0;
    int maskSizeX = 0;
    int maskSizeY = 0;
    std::string maskType;
    LineSpace lineSpace;
    std::string maskInputFile;
    float dose = 1.0f;
    int nk = 0;
    float NA = 0.0f;
    float defocus = 0.0f;
};

bool loadConfig(const std::string& configPath, SOCSConfig& config);

#endif // HOST_JSON_PARSER_HPP
