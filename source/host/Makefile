CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17
LIBS = -lfftw3f
TARGET = socs_host
SRCS = socs_host.cpp json_parser.cpp mask_processor.cpp kernel_loader.cpp file_io.cpp mask.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
