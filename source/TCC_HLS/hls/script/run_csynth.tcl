# ============================================================================
# C综合脚本 - run_csynth.tcl
# ============================================================================
# 用途：生成RTL代码，优化资源使用和性能
# 命令：vitis-run --mode hls --tcl --input_file script/run_csynth.tcl --work_dir hls_litho_proj
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

# 配置器件和时钟（已在hls_config.cfg中定义）
# set_part xcku3p-ffvb676-2-e
# create_clock -period 5ns -name default

# 运行C综合
csynth_design

# 输出结果
puts "=========================================="
puts "C Synthesis Completed"
puts "=========================================="
puts "Check reports:"
puts "  - solution1/syn/report/*.rpt"
puts "  - Latency: solution1/syn/report/tcc_top_csynth.rpt"
puts "  - Utilization: solution1/syn/report/tcc_top_csynth_util.rpt"

# 关闭solution和项目
close_solution
close_project

exit