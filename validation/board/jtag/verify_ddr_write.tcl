# ==============================================================================
# 验证环节2：DDR写入效果一致性验证
#
# 用途：
#   - 验证JTAG写入的HEX数据与期望BIN效果一致
#   - 通过DDR回读对比写入数据验证正确性
#
# 验证方法：
#   1. 写入HEX数据到DDR指定地址
#   2. 从相同地址回读DDR数据
#   3. 对比写入数据与回读数据的一致性
#
# 验收标准：
#   - DDR回读数据与写入HEX完全匹配
#   - 无数据丢失或错误
#
# 使用方法：
#   在Vivado Tcl Console中运行：
#   source verify_ddr_write.tcl
# ==============================================================================

# 配置参数
set AXI_IF [get_hw_axis hw_axi_1]
set TEST_ADDR 0x40000000  ;# 测试地址（mskf_r起始地址）
set TEST_LEN 10           ;# 测试数据长度（10个float32 = 40 bytes）
set TEST_DATA "3F800000_00000000_40000000_40400000_40800000_40A00000_40C00000_40E00000_41000000_41100000"

# ==============================================================================
# 步骤1：写入测试数据到DDR
# ==============================================================================
puts "\n>>> 步骤1：写入测试数据到DDR..."
puts "  地址: $TEST_ADDR"
puts "  数据长度: $TEST_LEN words (40 bytes)"
puts "  测试数据: $TEST_DATA"

# 创建写入事务
catch {delete_hw_axi_txn [get_hw_axi_txns wr_test_txn]}
create_hw_axi_txn wr_test_txn $AXI_IF -address $TEST_ADDR -data $TEST_DATA -len $TEST_LEN -type write

# 执行写入
puts "  执行写入..."
run_hw_axi_txn [get_hw_axi_txns wr_test_txn]
puts "  ✅ 写入完成"

# ==============================================================================
# 步骤2：从DDR回读数据
# ==============================================================================
puts "\n>>> 步骤2：从DDR回读数据..."

# 创建读取事务
catch {delete_hw_axi_txn [get_hw_axi_txns rd_test_txn]}
create_hw_axi_txn rd_test_txn $AXI_IF -address $TEST_ADDR -len $TEST_LEN -type read

# 执行读取
puts "  执行读取..."
run_hw_axi_txn [get_hw_axi_txns rd_test_txn]
puts "  ✅ 读取完成"

# ==============================================================================
# 步骤3：对比写入与回读数据
# ==============================================================================
puts "\n>>> 步骤3：对比写入与回读数据..."

# 获取回读数据
set rd_data [get_property DATA [get_hw_axi_txns rd_test_txn]]
puts "  写入数据: $TEST_DATA"
puts "  回读数据: $rd_data"

# 对比数据一致性
set write_values [split $TEST_DATA "_"]
set read_values [split $rd_data "_"]

set mismatch_count 0
set total_values [llength $write_values]

for {set i 0} {$i < $total_values} {incr i} {
    set w_val [lindex $write_values $i]
    set r_val [lindex $read_values $i]
    
    if {$w_val ne $r_val} {
        puts "  ❌ 不匹配 idx=$i: 写入=$w_val, 回读=$r_val"
        incr mismatch_count
    } else {
        puts "  ✅ 匹配 idx=$i: $w_val"
    }
}

# ==============================================================================
# 验证结果
# ==============================================================================
puts "\n>>> 验证结果:"
puts "  总数据量: $total_values words"
puts "  不匹配数: $mismatch_count words"

if {$mismatch_count == 0} {
    puts "\n✅ 验证通过：DDR写入与回读数据完全一致"
    puts "  DDR写入信任链环节2验证通过 ✅"
} else {
    puts "\n❌ 验证失败：DDR写入与回读数据不一致"
    puts "  不匹配率: [expr {double($mismatch_count) / $total_values * 100}]%"
    puts "  DDR写入信任链环节2验证失败 ❌"
}

# ==============================================================================
# 清理测试事务
# ==============================================================================
catch {delete_hw_axi_txn [get_hw_axi_txns wr_test_txn]}
catch {delete_hw_axi_txn [get_hw_axi_txns rd_test_txn]}

puts "\n验证完成：DDR写入信任链环节2验证结束"