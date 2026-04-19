# ==============================================================================
# DDR 数据加载脚本 - scales_data
# 数据量: 10 floats (40 bytes)
# 分批数: 1 batches (每批 128 words)
# DDR地址: 0x44000000
# ==============================================================================

puts "\n>>> 开始加载 scales_data (10 floats)..."

set axi_if [get_hw_axis hw_axi_1]
set BASE_ADDR 0x44000000

# Batch 0: 10 words
set addr_0 [format "0x%08X" [expr {$BASE_ADDR + 0}]]
set data_0 "40B84D95_401E5152_401E5152_3F3F8012_3F255DA9_3E98F0D4_3E98F0D4_3E29BB8A_3E15D802_3DB0F1CB"
catch {delete_hw_axi_txn [get_hw_axi_txns wr_scales_data_0]}
create_hw_axi_txn wr_scales_data_0 $axi_if -address $addr_0 -data $data_0 -len 10 -type write
run_hw_axi [get_hw_axi_txns wr_scales_data_0]
puts "进度: Batch 0 / 1"

# 验证写入
catch {delete_hw_axi_txn [get_hw_axi_txns verify_scales_data]}
create_hw_axi_txn verify_scales_data $axi_if -address $BASE_ADDR -len 4 -type read
run_hw_axi [get_hw_axi_txns verify_scales_data]
set verify_data [get_property DATA [get_hw_axi_txns verify_scales_data]]
puts "验证读取: $verify_data"

puts "✅ scales_data 加载完成"