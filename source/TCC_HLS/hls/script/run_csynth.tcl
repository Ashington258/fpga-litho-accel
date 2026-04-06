# ============================================================================
# C综合脚本 - run_csynth.tcl
# ============================================================================
# 用途：生成RTL代码，优化资源使用和性能
# 命令：vitis-run --mode hls --tcl --input_file hls/script/run_csynth.tcl --work_dir hls_litho_proj
# ============================================================================

set project_name "hls_litho_proj"
set solution_name "solution1"

# 打开或创建项目
if {[file exists $project_name]} {
    open_project $project_name
} else {
    puts "ERROR: Project does not exist. Run 'make csim' first to create the project."
    exit 1
}

# 添加源文件（必须重新添加，因为open_project不会自动加载）
add_files "hls/src/calc_tcc.cpp"
add_files "hls/src/calc_tcc.h"
add_files "hls/src/data_types.h"

# 设置顶层函数
set_top calc_tcc

# 打开solution
open_solution -flow_target vivado $solution_name

# 配置器件和时钟
set_part xcku3p-ffvb676-2-e
create_clock -period 5ns -name default

# 运行C综合
csynth_design

# 输出结果
puts "=========================================="
puts "C Synthesis Completed"
puts "=========================================="
puts "Check reports:"
puts "  - Latency: solution1/syn/report/calc_tcc_csynth.rpt"
puts "  - Utilization: solution1/syn/report/calc_tcc_csynth_util.rpt"

# 关闭solution和项目
close_solution
close_project

exit