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
TARGET_2048 = socs_2048_host

# Original SOCS Host sources
SRCS = socs_host.cpp json_parser.cpp mask_processor.cpp kernel_loader.cpp \
       file_io.cpp mask.cpp source_processor.cpp tcc_processor.cpp kernel_extractor.cpp
OBJS = $(SRCS:.cpp=.o)

# 2048 Host sources (DDR allocator)
SRCS_2048 = socs_2048_host.cpp ddr_allocator.cpp json_parser.cpp file_io.cpp mask.cpp source_processor.cpp tcc_processor.cpp
OBJS_2048 = $(SRCS_2048:.cpp=.o)

.PHONY: all clean 2048

all: $(TARGET) $(TARGET_2048)

2048: $(TARGET_2048)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(TARGET_2048): $(OBJS_2048)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS_2048)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OBJS_2048) $(TARGET) $(TARGET_2048)
