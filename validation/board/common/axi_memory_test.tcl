# ============================================
# 标准 AXI 内存测试脚本（32-bit 版）
# 适用于：BRAM、DDR、HBM 等各类 AXI 内存测试
# 用法：修改 base_addr 参数以适配不同内存类型
# ============================================
# source E:\\1.Project\\4.FPGA\\TCL\\test\\axi_memory_test_32bit.tcl

# ========= 用户参数 =========
set axi_if      [get_hw_axis hw_axi_1]
set base_addr   0xC0000000
set test_len    4          ;# 测试的 word 数量，可自行修改

# ========= 生成测试数据（正常顺序） =========
proc gen_test_data {len} {
    set data_list {}
    for {set i 0} {$i < $len} {incr i} {
        set val [format "%08X" $i]
        lappend data_list $val
    }
    return $data_list
}

# ========= AXI 写 =========
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

# ========= AXI 读（大小写修复版） =========
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
        lappend data_list [string toupper $word]   ;# 强制大写，确保比较一致
    }

    puts ">>> AXI READ DONE"
    return $data_list
}

# ========= 数据检查 =========
proc check_data {expected actual} {
    set err 0
    for {set i 0} {$i < [llength $expected]} {incr i} {
        set w [lindex $expected $i]
        set r [lindex $actual $i]
        if {$w ne $r} {
            puts "ERROR @ $i : W=$w  R=$r"
            incr err
        }
    }
    return $err
}

# ========= 主流程 =========
puts "======================================"
puts " AXI 内存测试（32-bit 版）"
puts " Reversed Write 模式"
puts " 适用于：BRAM / DDR / HBM 等各类存储器"
puts "======================================"

reset_hw_axi $axi_if

# 生成正常顺序的测试数据（你希望内存最终收到的数据）
set normal_data [gen_test_data $test_len]
puts "Normal Write Data (expected in Memory) = $normal_data"

# ==================== 关键修改：反序后写入 ====================
set write_data [lreverse $normal_data]
puts "Actual Write Data (sent to JTAG-AXI) = $write_data"

# 写入（反序数据）
axi_write $axi_if $base_addr $write_data

# 读取
set read_data [axi_read $axi_if $base_addr $test_len]
puts "READ DATA (from Memory) = $read_data"

# =============================
# 检查（直接对比正常顺序）
# =============================
puts "\n=== Normal Order Verification ==="
set err [check_data $normal_data $read_data]

if {$err == 0} {
    puts ">>> ✅ PASS !"
    puts ">>> 内存测试成功：写入与读回完全一致（正常顺序）"
    puts ">>> JTAG-to-AXI burst 反序问题已通过反序写入解决"
} else {
    puts ">>> ❌ FAIL : 共 $err 个 word 不匹配"
    puts ">>> 请检查地址映射、数据宽度或 AXI slave 逻辑"
}

puts "\n======================================"
puts " AXI MEMORY TEST END (32-bit)"
puts "======================================"