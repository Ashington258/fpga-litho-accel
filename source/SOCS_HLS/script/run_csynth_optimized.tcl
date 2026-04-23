# ==============================================================================
# TCL Script for Optimized SOCS HLS C Synthesis
# FPGA-Litho Project - Vitis FFT IP Integration
# ==============================================================================

# Project configuration
set project_name "socs_optimized_fft_ip"
set solution_name "sol_optimized_fft"
set top_function "calc_socs_optimized_hls"
set target_part "xcku3p-ffvb676-2-e"
set target_clock "5ns"
set flow_target "vitis"

# Create new project
open_project -reset $project_name

# Add source files
add_files "src/socs_optimized.cpp"
add_files "src/socs_config_optimized.h"
set_top $top_function

# Add testbench
add_files -tb "tb/tb_socs_optimized.cpp"

# Create solution
open_solution -reset $solution_name

# Set target device and clock
set_part $target_part
config_timeclock -clock $target_clock

# Set flow target
config_flow -target $flow_target

# ==============================================================================
# HLS Optimization Directives
# ==============================================================================

# Pipeline all loops for maximum throughput
directive set /calc_socs_optimized_hls/fft_2d_optimized -PIPELINE_STYLE flp
directive set /calc_socs_optimized_hls/embed_kernel_mask_product -PIPELINE_STYLE flp
directive set /calc_socs_optimized_hls/accumulate_intensity -PIPELINE_STYLE flp
directive set /calc_socs_optimized_hls/fftshift_2d -PIPELINE_STYLE flp
directive set /calc_socs_optimized_hls/extract_center_region -PIPELINE_STYLE flp

# Array partitioning for parallel access
directive set /calc_socs_optimized_hls/fft_input -ARRAY_PARTITION cyclic factor=2 dim=2
directive set /calc_socs_optimized_hls/fft_output -ARRAY_PARTITION cyclic factor=2 dim=2
directive set /calc_socs_optimized_hls/tmpImg -ARRAY_PARTITION cyclic factor=2 dim=2

# Resource specification
directive set /calc_socs_optimized_hls/fft_input -RESOURCE core=RAM_2P_BRAM
directive set /calc_socs_optimized_hls/fft_output -RESOURCE core=RAM_2P_BRAM
directive set /calc_socs_optimized_hls/tmpImg -RESOURCE core=RAM_2P_BRAM

# Interface optimization
directive set /calc_socs_optimized_hls/mskf_r -INTERFACE mode=m_axi bundle=gmem0 latency=32 max_read_burst_length=64
directive set /calc_socs_optimized_hls/mskf_i -INTERFACE mode=m_axi bundle=gmem1 latency=32 max_read_burst_length=64
directive set /calc_socs_optimized_hls/scales -INTERFACE mode=m_axi bundle=gmem2 latency=16 max_read_burst_length=4
directive set /calc_socs_optimized_hls/krn_r -INTERFACE mode=m_axi bundle=gmem3 latency=16 max_read_burst_length=16
directive set /calc_socs_optimized_hls/krn_i -INTERFACE mode=m_axi bundle=gmem4 latency=16 max_read_burst_length=16
directive set /calc_socs_optimized_hls/output_full -INTERFACE mode=m_axi bundle=gmem5 latency=8 max_write_burst_length=16
directive set /calc_socs_optimized_hls/output_center -INTERFACE mode=m_axi bundle=gmem6 latency=8 max_write_burst_length=8
directive set /calc_socs_optimized_hls -INTERFACE mode=s_axilite port=return bundle=control

# Dataflow optimization for kernel processing loop
directive set /calc_socs_optimized_hls -DATAFLOW

# ==============================================================================
# Run C Synthesis
# ==============================================================================
csynth_design

# Close project
close_project

# Print completion message
puts "========================================"
puts "  C Synthesis Completed Successfully"
puts "========================================"
puts "Project: $project_name"
puts "Solution: $solution_name"
puts "Top Function: $top_function"
puts "Target Device: $target_part"
puts "Target Clock: $target_clock"
puts "========================================"