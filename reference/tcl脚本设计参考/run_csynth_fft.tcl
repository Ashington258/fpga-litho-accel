# =====================================================
# run_csynth_fft.tcl  --- 稳定版（Vitis HLS 2025.2）
# 使用绝对路径，避免相对路径解析问题
# =====================================================

# 使用绝对路径定义项目位置
set proj_dir "/root/project/FPGA-Litho/source/SOCS_HLS"

open_component -reset fft_2d_forward_32_comp -flow_target vivado

set_top fft_2d_forward_32

# === 使用绝对路径添加文件（最保险）===
add_files ${proj_dir}/src/fft_2d.cpp
add_files ${proj_dir}/src/fft_2d.h

# Testbench（如果暂时不需要 csim，可以注释掉下面一行）
add_files -tb ${proj_dir}/tb/tb_fft_2d.cpp

# 配置
set_part xcku3p-ffvb676-2-e
create_clock -period 5 -name default

config_compile -name_max_length 256
config_interface -clock_enable=false

# ================== 运行 C 综合 ==================
csynth_design

# 可选：运行 C Simulation
# csim_design -O

# 可选：导出 RTL/IP
# export_design -flow syn -rtl verilog -format ip_catalog

exit