# ==============================================================================
# 验证环节2：DDR写入效果一致性验证
#
# 用途：
#   - 验证JTAG写入的HEX数据与期望BIN效果一致
#   - 通过DDR回读对比写入数据验证正确性
#
# 验证方法：
#   1. 写入HEX数据到DDR指定地址（反序写入以抵消JTAG-to-AXI burst反序）
#   2. 从相同地址回读DDR数据
#   3. 对比写入数据与回读数据的一致性
#
# 关键发现：
#   JTAG-to-AXI burst传输特性：数据会反序写入内存
#   解决方法：写入前先反序，使内存最终存储正常顺序数据
#
# 验收标准：
#   - DDR回读数据与期望数据完全匹配（正常顺序）
#   - 无数据丢失或错误
#
# 使用方法：
#   在Vivado Tcl Console中运行：
#   source verify_ddr_write.tcl
#
# 参考：
#   validation/board/common/axi_memory_test.tcl（标准AXI测试脚本）
# ==============================================================================

# 配置参数
set AXI_IF [get_hw_axis hw_axi_1]
set TEST_ADDR 0x40000000  ;# 测试地址（mskf_r起始地址）
set TEST_LEN 10           ;# 测试数据长度（10个float32 = 40 bytes）

# 测试数据（IEEE 754 float32 hex）
# 期望内存中的正常顺序：1.0, 0.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0
set NORMAL_DATA "3F800000 00000000 40000000 40400000 40800000 40A00000 40C00000 40E00000 41000000 41100000"

# ==============================================================================
# AXI写入函数（使用正确命令：run_hw_axi）
# ==============================================================================
proc axi_write {axi_if addr data_list} {
    puts ">>> AXI WRITE START (@ [format %08X $addr])"
    set txn [create_hw_axi_txn w_txn $axi_if \
        -type write \
        -address $addr \
        -data $data_list \
        -len [llength $data_list] \
        -force]
    run_hw_axi $txn
    puts ">>> AXI WRITE DONE"
}

# ==============================================================================
# AXI读取函数（使用正确命令：run_hw_axi）
# ==============================================================================
proc axi_read {axi_if addr len} {
    puts ">>> AXI READ START (@ [format %08X $addr])"
    set txn [create_hw_axi_txn r_txn $axi_if \
        -type read \
        -address $addr \
        -len $len \
        -force]
    run_hw_axi $txn

    set raw [get_property DATA $txn]
    set data_list {}
    set word_len 8    ;# 32bit = 8 hex characters

    for {set i 0} {$i < [string length $raw]} {incr i $word_len} {
        set word [string range $raw $i [expr {$i + $word_len - 1}]]
        lappend data_list [string toupper $word]
    }

    puts ">>> AXI READ DONE"
    return $data_list
}

# ==============================================================================
# 数据对比函数
# ==============================================================================
proc check_data {expected actual} {
    set err 0
    for {set i 0} {$i < [llength $expected]} {incr i} {
        set exp_val [lindex $expected $i]
        set act_val [lindex $actual $i]
        if {$exp_val ne $act_val} {
            puts "ERROR @ $i : Expected=$exp_val  Actual=$act_val"
            incr err
        }
    }
    return $err
}

# ==============================================================================
# 主流程
# ==============================================================================
puts "======================================"
puts " DDR写入效果一致性验证"
puts "======================================"

# 重置AXI接口
reset_hw_axi $AXI_IF

# ==============================================================================
# 步骤1：写入测试数据到DDR
# ==============================================================================
puts "\n>>> 步骤1：写入测试数据到DDR..."
puts "  地址: $TEST_ADDR"
puts "  数据长度: $TEST_LEN words (40 bytes)"
puts "  期望内存数据（正常顺序）: $NORMAL_DATA"

# ==================== 关键修改：反序后写入 ====================
# JTAG-to-AXI burst传输特性：数据会反序写入内存
# 解决方法：写入前先反序，使内存最终存储正常顺序数据
set WRITE_DATA [lreverse $NORMAL_DATA]
puts "  实际写入数据（反序发送）: $WRITE_DATA"
axi_write $AXI_IF $TEST_ADDR $WRITE_DATA

# ==============================================================================
# 步骤2：从DDR回读数据
# ==============================================================================
puts "\n>>> 步骤2：从DDR回读数据..."
set read_data [axi_read $AXI_IF $TEST_ADDR $TEST_LEN]
puts "  ✅ 读取完成"
puts "  回读数据: $read_data"

# ==============================================================================
# 步骤3：对比期望数据与回读数据
# ==============================================================================
puts "\n>>> 步骤3：对比期望数据与回读数据..."
puts "  期望（正常顺序）: $NORMAL_DATA"
puts "  回读（内存存储）: $read_data"

set mismatch_count [check_data $NORMAL_DATA $read_data]

# ==============================================================================
# 验证结果
# ==============================================================================
puts "\n======================================"
puts " 验证结果"
puts "======================================"

if {$mismatch_count == 0} {
    puts "✅ 验证通过：DDR回读数据与期望完全匹配"
    puts "  期望数据: $NORMAL_DATA"
    puts "  回读数据: $read_data"
    puts "  数据完整性：100%"
    puts "  JTAG-to-AXI反序写入策略验证成功"
} else {
    puts "❌ 验证失败：DDR回读数据存在 $mismatch_count 个不匹配"
    puts "  期望数据: $NORMAL_DATA"
    puts "  回读数据: $read_data"
    puts "  数据完整性：[expr {100 - $mismatch_count * 100.0 / $TEST_LEN}]%"
}

puts "\n======================================"
puts " 验证完成"
puts "======================================"
puts "DDR写入信任链环节2验证结束"