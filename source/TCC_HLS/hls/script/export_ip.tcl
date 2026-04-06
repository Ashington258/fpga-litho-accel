# ============================================================================
# IP导出脚本 - export_ip.tcl
# ============================================================================
# 用途：导出Vivado IP核（.zip格式）
# 命令：vitis-run --mode hls --tcl --input_file script/export_ip.tcl --work_dir hls_litho_proj
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

# 导出IP（需要先完成C综合+CoSim）
export_design -flow_target vivado -format ip_catalog

# 输出结果
puts "=========================================="
puts "IP Export Completed"
puts "=========================================="
puts "Generated IP package:"
puts "  - solution1/impl/export/ip/*.zip"
puts ""
puts "Usage in Vivado:"
puts "  1. Tools -> Settings -> IP -> Repository -> Add IP repository"
puts "  2. Add path to solution1/impl/export/ip/"
puts "  3. Refresh IP Catalog"
puts "  4. Drag tcc_top IP into Block Design"

# 关闭solution和项目
close_solution
close_project

exit