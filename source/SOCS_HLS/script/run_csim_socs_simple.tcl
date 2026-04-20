# =====================================================
# run_csim_socs_simple.tcl --- C Simulation for SOCS Simple (Float+LUT FFT)
# Target: Verify compilation + functional correctness
# =====================================================

set proj_dir "E:/fpga-litho-accel/source/SOCS_HLS"

# Clean previous component
open_component -reset socs_simple_csim_comp -flow_target vivado

set_top socs_simple

# Add source files
add_files ${proj_dir}/src/socs_simple.cpp
add_files ${proj_dir}/src/socs_config.h

# Add testbench
add_files -tb ${proj_dir}/tb/tb_socs_simple.cpp

# Target device (keep consistent with previous synth)
set_part xcku3p-ffvb676-2-e

# Clock constraint (200 MHz target)
create_clock -period 5 -name default

# ================== Run C Simulation ==================
puts "=== Starting C Simulation (Float FFT + LUT mode) ==="
csim_design -O  # -O enables optimization for faster execution

puts "=== C Simulation completed ==="
# Do NOT run synthesis yet (csim_design only)

exit