# ==============================================================
# Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
# Tool Version Limit: 2025.11
# Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
# Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
# 
# ==============================================================
CSIM_DESIGN = 1

__SIM_FPO__ = 1

__SIM_MATHHLS__ = 1

__SIM_FFT__ = 1

__SIM_FIR__ = 1

__SIM_DDS__ = 1

__USE_CLANG__ = 1

__USE_VCXX_CLANG__ = 1

ObjDir = obj

HLS_SOURCES = ../../../../fft_tb.cpp ../../../../fft_top.cpp

override TARGET := csim.exe

AUTOPILOT_ROOT := C:/AMDDesignTools/2025.2/Vitis
AUTOPILOT_MACH := win64
ifdef AP_GCC_M32
  AUTOPILOT_MACH := Linux_x86
  IFLAG += -m32
endif
ifndef AP_GCC_PATH
  AP_GCC_PATH := C:/AMDDesignTools/2025.2/Vitis/tps/mingw/10.0.0/win64.o/nt/bin
endif
AUTOPILOT_TOOL := ${AUTOPILOT_ROOT}/${AUTOPILOT_MACH}/tools
AP_CLANG_PATH := ${AUTOPILOT_ROOT}/win64/tools/clang-16/bin
AUTOPILOT_TECH := ${AUTOPILOT_ROOT}/common/technology


IFLAG += -I "${AUTOPILOT_ROOT}/include"
IFLAG += -I "${AUTOPILOT_ROOT}/include/ap_sysc"
IFLAG += -I "${AUTOPILOT_TECH}/generic/SystemC"
IFLAG += -I "${AUTOPILOT_TECH}/generic/SystemC/AESL_FP_comp"
IFLAG += -I "${AUTOPILOT_TECH}/generic/SystemC/AESL_comp"
IFLAG += -I "${AUTOPILOT_TOOL}/auto_cc/include"
IFLAG += -D__HLS_COSIM__

IFLAG += -D__HLS_CSIM__

IFLAG += -D__VITIS_HLS__

IFLAG += -D__SIM_FPO__

IFLAG += -D__SIM_FFT__

IFLAG += -D__SIM_FIR__

IFLAG += -D__SIM_DDS__

IFLAG += -D__DSP48E2__
LFLAG += -Wl,--stack,0x40000000
IFLAG += -g
IFLAG += -DNT
LFLAG += -Wl,--enable-auto-import 
DFLAG += -D__xilinx_ip_top= -DAESL_TB
CCFLAG += -Werror=return-type
CCFLAG += -Wno-abi
CCFLAG += -fdebug-default-version=4
CCFLAG += --sysroot=C:/AMDDesignTools/2025.2/Vitis/tps/mingw/10.0.0/win64.o/nt
CCFLAG += -Werror=uninitialized
CCFLAG += -Wno-c++11-narrowing
CCFLAG += -Wno-error=sometimes-uninitialized
LFLAG += --sysroot=C:/AMDDesignTools/2025.2/Vitis/tps/mingw/10.0.0/win64.o/nt
CCFLAG += --target=x86_64-w64-windows-gnu
LFLAG += --target=x86_64-w64-windows-gnu



include ./Makefile.rules

all: $(TARGET)



$(ObjDir)/fft_tb.o: ../../../../fft_tb.cpp $(ObjDir)/.dir csim.mk
	$(Echo) "   Compiling ../../../../fft_tb.cpp in $(BuildMode) mode" $(AVE_DIR_DLOG)
	$(Verb)  $(CXX) -std=gnu++17 ${CCFLAG} -c -MMD -Wno-unknown-pragmas -Wno-unknown-pragmas  $(IFLAG) $(DFLAG) $< -o $@ ; \

-include $(ObjDir)/fft_tb.d

$(ObjDir)/fft_top.o: ../../../../fft_top.cpp $(ObjDir)/.dir csim.mk
	$(Echo) "   Compiling ../../../../fft_top.cpp in $(BuildMode) mode" $(AVE_DIR_DLOG)
	$(Verb)  $(CXX) -std=gnu++17 ${CCFLAG} -c -MMD  -fhls-csim -fhlstoplevel=fft_top $(IFLAG) $(DFLAG) $< -o $@ ; \

-include $(ObjDir)/fft_top.d
