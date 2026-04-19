# =====================================================
# run_csynth_socs_full.tcl --- SOCS Complete Implementation
# Vitis HLS 2025.2
# =====================================================

# 项目目录
set proj_dir "E:/fpga-litho-accel/source/SOCS_HLS"

# 创建 HLS 组件
open_component -reset socs_full_comp -flow_target vivado

# 设置顶层函数
set_top calc_socs_hls

# 添加源文件
add_files ${proj_dir}/src/socs_fft.h
add_files ${proj_dir}/src/socs_fft.cpp
add_files ${proj_dir}/src/socs_hls.cpp

# 添加测试平台
add_files -tb ${proj_dir}/tb/tb_socs_hls.cpp

# 配置目标器件
set_part xcku3p-ffvb676-2-e
create_clock -period 5 -name default

# HLS 配置
config_compile -name_max_length 256
config_interface -clock_enable=false

# ================== 运行 C 综合 ==================
csynth_design

# 可选：导出 RTL/IP
# export_design -flow syn -rtl verilog -format ip_catalog

exit