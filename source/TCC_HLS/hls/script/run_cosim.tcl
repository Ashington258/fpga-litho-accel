# ============================================================================
# CoSim脚本 - run_cosim.tcl
# ============================================================================
# 用途：RTL级协同仿真验证
# 命令：vitis-run --mode hls --tcl --input_file script/run_cosim.tcl --work_dir hls_litho_proj
# ============================================================================

set project_name "hls_litho_proj"
set solution_name "solution1"

# 打开项目
if {[file exists $project_name]} {
    open_project $project_name
} else {
    puts "INFO: Project will be created by vitis-run"
}

# 设置顶层函数
set_top tcc_top

# 打开solution
open_solution -flow_target vivado $solution_name

# 运行CoSim（需要先完成C综合）
cosim_design

# 输出结果
puts "=========================================="
puts "Co-Simulation Completed"
puts "=========================================="
puts "Check solution1/cosim/report for results"

# 关闭solution和项目
close_solution
close_project

exit