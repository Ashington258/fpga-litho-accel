
# ==============================================================================
# 数据使用说明
# ==============================================================================

# 在 Vivado Tcl Console 中执行：
#   source socs_hls_data.tcl
#   source socs_hls_data_partial.tcl

# 写入 scales 到 DDR:
#   axi_write_data $axi_if 0x44000000 $scales_data "Write scales"

# 写入 kernel 数据:
#   for {{set k 0}} {{$k < 10}} {{incr k}} {{
#       axi_write_data $axi_if [expr 0x44400000 + $k*81*4] $krn_$k_r_data "Write krn_$k_r"
#       axi_write_data $axi_if [expr 0x44800000 + $k*81*4] $krn_$k_i_data "Write krn_$k_i"
#   }}

# 分批写入 mskf_r (需要循环):
#   set mskf_r_batches 2049
#   for {{set b 0}} {{$b < $mskf_r_batches}} {{incr b}} {{
#       axi_write_data $axi_if [expr 0x40000000 + $b*128*4] $mskf_r_batch_$b "Write mskf_r batch $b"
#   }}

