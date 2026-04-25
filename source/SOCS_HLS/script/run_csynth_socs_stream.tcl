# TCL script for SOCS Stream-based C Synthesis
# Usage: vitis-run --mode hls --tcl script/run_csynth_socs_stream.tcl

# Open project
open_project socs_2048_stream_proj

# Add source files
add_files src/socs_2048_stream.cpp
add_files src/socs_2048.cpp
add_files src/hls_fft_2048.cpp

# Add test bench
add_files -tb src/tb_socs_2048_stream.cpp

# Set top function
set_top calc_socs_2048_hls_stream

# Open solution
open_solution -reset solution1

# Set target device
set_part {xcku3p-ffvb676-2-e}

# Set clock period
create_clock -period 5.00 -name default

# Run C simulation
csim_design

# Run C synthesis
csynth_design

# Run C/RTL co-simulation
# cosim_design

# Export IP
# export_design -format ip_catalog

# Exit
exit
