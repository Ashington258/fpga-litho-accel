#ifndef HOST_FILE_IO_HPP
#define HOST_FILE_IO_HPP

#include <vector>
#include <complex>
#include <string>

void createOutputDir(const std::string& path);
void ensureOutputDirs(const std::string& outputDir);
void writeFloatArrayToBinary(const std::string& filename, const std::vector<float>& data);
void writeComplexArrayToBinary(const std::string& realFile, const std::string& imagFile, const std::vector<std::complex<float>>& data);
void readFloatArrayFromBinary(const std::string& filename, std::vector<float>& data, size_t size);
void readComplexArrayFromBinary(const std::string& realFile, const std::string& imagFile, std::vector<std::complex<float>>& data, size_t size);

#endif // HOST_FILE_IO_HPP
