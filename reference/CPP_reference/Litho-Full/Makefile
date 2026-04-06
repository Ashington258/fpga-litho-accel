# Litho-Full: Integrated TCC + SOCS Verification Program
# ===========================================================
# Dependencies: FFTW3, LAPACK, BLAS

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -I../common -I../Litho-TCC
LDFLAGS = -lfftw3 -lfftw3f -llapack -lblas -lm

# Source files
SRCS = klitho_full.cpp \
       ../common/file_io.cpp \
       ../common/mask.cpp \
       ../Litho-TCC/source.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target
TARGET = klitho_full

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# Pattern rule for object files
%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf output/full

run: $(TARGET)
	./$(TARGET) ../../input/config/config.json output/full --debug