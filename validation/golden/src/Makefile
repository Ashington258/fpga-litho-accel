# Litho Verification Build
# ========================
# 独立编译验证程序，不依赖 reference 目录

CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17 -I./common
LDFLAGS = -lfftw3 -lfftw3f -llapack -lblas -lm

# Source files
SRCS = litho.cpp \
       common/file_io.cpp \
       common/mask.cpp \
       common/source.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target
TARGET = litho

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# Pattern rule for object files
%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.cpp.o: common/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	cmd /c del /f /q $(TARGET) *.o common\*.o 2>nul || echo "Clean done"