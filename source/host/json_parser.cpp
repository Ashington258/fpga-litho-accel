#include "json_parser.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

static bool extractSection(const std::string& content, const std::string& sectionName, std::string& outSection) {
    std::string key = "\"" + sectionName + "\"";
    std::size_t pos = content.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos = content.find('{', pos);
    if (pos == std::string::npos) {
        return false;
    }
    int depth = 1;
    std::size_t end = pos + 1;
    while (end < content.size() && depth > 0) {
        if (content[end] == '{') {
            depth++;
        } else if (content[end] == '}') {
            depth--;
        }
        end++;
    }
    if (depth != 0) {
        return false;
    }
    outSection = content.substr(pos, end - pos);
    return true;
}

static bool extractJsonString(const std::string& text, const std::string& key, std::string& value) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch results;
    if (std::regex_search(text, results, pattern)) {
        value = results[1].str();
        return true;
    }
    return false;
}

static bool extractJsonNumber(const std::string& text, const std::string& key, double& value) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)");
    std::smatch results;
    if (std::regex_search(text, results, pattern)) {
        try {
            value = std::stod(results[1].str());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

static bool extractJsonBool(const std::string& text, const std::string& key, bool& value) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch results;
    if (std::regex_search(text, results, pattern)) {
        value = (results[1].str() == "true");
        return true;
    }
    return false;
}

static bool parseInt(const std::string& text, const std::string& key, int& value) {
    double temp;
    if (!extractJsonNumber(text, key, temp)) {
        return false;
    }
    value = static_cast<int>(temp);
    return true;
}

static bool parseFloat(const std::string& text, const std::string& key, float& value) {
    double temp;
    if (!extractJsonNumber(text, key, temp)) {
        return false;
    }
    value = static_cast<float>(temp);
    return true;
}

bool loadConfig(const std::string& configPath, SOCSConfig& config) {
    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        std::cerr << "Error: Failed to open config file: " << configPath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();
    ifs.close();

    std::string maskSection;
    if (!extractSection(content, "mask", maskSection)) {
        std::cerr << "Error: Failed to parse 'mask' section in config." << std::endl;
        return false;
    }
    std::string periodSection;
    if (!extractSection(maskSection, "period", periodSection)) {
        std::cerr << "Error: Failed to parse 'mask.period' section in config." << std::endl;
        return false;
    }
    std::string sizeSection;
    if (!extractSection(maskSection, "size", sizeSection)) {
        std::cerr << "Error: Failed to parse 'mask.size' section in config." << std::endl;
        return false;
    }
    std::string lineSpaceSection;
    if (!extractSection(maskSection, "lineSpace", lineSpaceSection)) {
        lineSpaceSection = "";
    }

    if (!parseInt(periodSection, "Lx", config.Lx) || !parseInt(periodSection, "Ly", config.Ly)) {
        std::cerr << "Error: Failed to parse mask period dimensions." << std::endl;
        return false;
    }
    if (!parseInt(sizeSection, "maskSizeX", config.maskSizeX) || !parseInt(sizeSection, "maskSizeY", config.maskSizeY)) {
        std::cerr << "Error: Failed to parse mask size dimensions." << std::endl;
        return false;
    }
    if (!extractJsonString(maskSection, "type", config.maskType)) {
        std::cerr << "Error: Failed to parse mask type." << std::endl;
        return false;
    }
    if (!extractJsonString(maskSection, "inputFile", config.maskInputFile)) {
        std::cerr << "Error: Failed to parse mask input file." << std::endl;
        return false;
    }
    if (!parseFloat(maskSection, "dose", config.dose)) {
        config.dose = 1.0f;
    }
    if (!parseInt(lineSpaceSection, "lineWidth", config.lineSpace.lineWidth)) {
        config.lineSpace.lineWidth = 0;
    }
    if (!parseInt(lineSpaceSection, "spaceWidth", config.lineSpace.spaceWidth)) {
        config.lineSpace.spaceWidth = 0;
    }
    if (!extractJsonBool(lineSpaceSection, "isHorizontal", config.lineSpace.isHorizontal)) {
        config.lineSpace.isHorizontal = false;
    }

    std::string opticsSection;
    if (!extractSection(content, "optics", opticsSection)) {
        std::cerr << "Error: Failed to parse 'optics' section in config." << std::endl;
        return false;
    }
    if (!parseFloat(opticsSection, "NA", config.NA)) {
        std::cerr << "Error: Failed to parse optics.NA." << std::endl;
        return false;
    }
    if (!parseFloat(opticsSection, "defocus", config.defocus)) {
        std::cerr << "Error: Failed to parse optics.defocus." << std::endl;
        return false;
    }

    std::string kernelSection;
    if (!extractSection(content, "kernel", kernelSection)) {
        std::cerr << "Error: Failed to parse 'kernel' section in config." << std::endl;
        return false;
    }
    if (!parseInt(kernelSection, "count", config.nk)) {
        std::cerr << "Error: Failed to parse kernel.count." << std::endl;
        return false;
    }

    return true;
}
