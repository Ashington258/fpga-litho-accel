CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17

# Eigen library path (adjust based on platform)
# Linux: /usr/include/eigen3 or /usr/local/include/eigen3
# Windows (MSYS2 UCRT64): C:/msys64/ucrt64/include/eigen3
# Windows (vcpkg): installed/x64-windows/include/eigen3
# Windows (conda): <conda_env>/Library/include/eigen3

# Auto-detect platform
ifeq ($(OS),Windows_NT)
    # Windows MSYS2 UCRT64 paths
    EIGEN_PATH ?= C:/msys64/ucrt64/include/eigen3
    FFTW_INCLUDE ?= C:/msys64/ucrt64/include
    FFTW_LIB ?= C:/msys64/ucrt64/lib
    CXXFLAGS += -I$(EIGEN_PATH) -I$(FFTW_INCLUDE)
    LDFLAGS = -L$(FFTW_LIB)
else
    # Linux paths
    EIGEN_PATH ?= /usr/include/eigen3
    CXXFLAGS += -I$(EIGEN_PATH)
    LDFLAGS =
endif

# FFTW3 libraries (single and double precision)
LIBS = -lfftw3 -lfftw3f

TARGET = socs_host
SRCS = socs_host.cpp json_parser.cpp mask_processor.cpp kernel_loader.cpp \
       file_io.cpp mask.cpp source_processor.cpp tcc_processor.cpp kernel_extractor.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
