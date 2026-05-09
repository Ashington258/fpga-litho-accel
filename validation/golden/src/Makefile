# Litho Verification Build
# ========================
# 独立编译验证程序，不依赖 reference 目录

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -I./common -fopenmp
LDFLAGS = -lfftw3 -lfftw3_threads -lfftw3f -lfftw3f_threads -llapack -lblas -lm -fopenmp

# Source files
SRCS = litho.cpp \
       common/file_io.cpp \
       common/mask.cpp \
       common/source.cpp
SRCS_THREAD = litho_thread.cpp \
              common/file_io.cpp \
              common/mask.cpp \
              common/source.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Targets
TARGET = litho
TARGET_THREAD = litho_thread

.PHONY: all clean

all: $(TARGET) $(TARGET_THREAD)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

$(TARGET_THREAD): litho_thread.o common/file_io.o common/mask.o common/source.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful: $(TARGET_THREAD)"

# Pattern rule for object files
%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.cpp.o: common/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	cmd /c del /f /q $(TARGET) *.o common\*.o 2>nul || echo "Clean done"