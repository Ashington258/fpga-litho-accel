# ============================================================================
# C仿真脚本 - run_csim.tcl
# ============================================================================
# 用途：验证算法正确性（对比golden数据）
# 命令：vitis-run --mode hls --tcl --input_file script/run_csim.tcl --work_dir hls_litho_proj
# ============================================================================

# 打开项目（如果不存在则创建）
set project_name "hls_litho_proj"
set solution_name "solution1"

# 检查项目是否存在
if {[file exists $project_name]} {
    open_project $project_name
} else {
    # 创建新项目（由vitis-run自动完成）
    puts "INFO: Project will be created by vitis-run"
}

# 设置顶层函数
set_top tcc_top

# 打开solution
open_solution -flow_target vivado $solution_name

# 运行C仿真
csim_design

# 输出结果
puts "=========================================="
puts "C Simulation Completed"
puts "=========================================="
puts "Check csim/report for results"

# 关闭solution和项目
close_solution
close_project

exit