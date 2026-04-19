# ==============================================================================
# SOCS HLS IP 板级验证脚本
# 适用于：calc_socs_hls IP 核（基于 Vitis HLS 2025.2）
# 硬件平台：xcku3p-ffvb676-2-e + DDR4
# 用法：在 Vivado Hardware Manager Tcl Console 中执行
# 
# 参考文档：
# source E:\\fpga-litho-accel\\tcl\\socs_board_validation.tcl
# - AddressSegments.csv（地址映射）
# - xcalc_socs_hls_hw.h（寄存器定义）
# - BIN_Format_Specification.md（数据格式）
# ==============================================================================

# ========================= 全局参数配置 =========================
# 硬件接口参数
set axi_if          [get_hw_axis hw_axi_1]

# HLS IP 控制接口地址（从 JTAG AXI 映射）
set ctrl_base       0x0000      ;# s_axi_control (ap_ctrl)
set ctrl_r_base     0x10000     ;# s_axi_control_r (参数地址)

# DDR4 内存地址映射（从 AddressSegments.csv）
set ddr_mskf_r      0x40000000  ;# m_axi_gmem0 (mask 实部频域)
set ddr_mskf_i      0x42000000  ;# m_axi_gmem1 (mask 虚部频域)
set ddr_scales      0x44000000  ;# m_axi_gmem2 (特征值)
set ddr_krn_r       0x44400000  ;# m_axi_gmem3 (kernel 实部)
set ddr_krn_i       0x44800000  ;# m_axi_gmem4 (kernel 虚部)
set ddr_output      0x44840000  ;# m_axi_gmem5 (输出结果)

# 数据尺寸参数（从 output/verification/fft_meta.txt）
set output_size     17          ;# conv_size_x = conv_size_y = 17 (Nx=4, Ny=4)
set output_words    [expr {$output_size * $output_size}]  ;# 289 floats

# 寄存器偏移定义（从 xcalc_socs_hls_hw.h）
set ADDR_AP_CTRL    0x0
set ADDR_MSKF_R     0x10
set ADDR_MSKF_I     0x1c
set ADDR_SCALES     0x28
set ADDR_KRN_R      0x34
set ADDR_KRN_I      0x40
set ADDR_OUTPUT_R   0x4c

# ========================= 辅助函数定义 =========================

# AXI burst 最大长度限制（Vivado JTAG-to-AXI Master 限制）
set MAX_BURST_LEN   128  ;# 安全值，避免超过 Vivado 限制

# AXI 写操作（支持数据列表，自动分批）
proc axi_write_data {axi_if addr data_list {desc "AXI Write"}} {
    global MAX_BURST_LEN
    set total_len [llength $data_list]
    puts ">>> $desc START (@ [format 0x%08X $addr], total_len=$total_len)"
    
    # 如果数据量小于限制，直接写入
    if {$total_len <= $MAX_BURST_LEN} {
        set addr_hex [format "0x%08X" $addr]
        set txn [create_hw_axi_txn w_txn $axi_if \
            -type write \
            -address $addr_hex \
            -data $data_list \
            -len $total_len \
            -force]
        run_hw_axi $txn
    } else {
        # 分批写入
        set batch_count [expr {($total_len + $MAX_BURST_LEN - 1) / $MAX_BURST_LEN}]
        puts ">>> Splitting into $batch_count batches (max $MAX_BURST_LEN per batch)"
        
        for {set batch 0} {$batch < $batch_count} {incr batch} {
            set start_idx [expr {$batch * $MAX_BURST_LEN}]
            set end_idx [expr {min($start_idx + $MAX_BURST_LEN - 1, $total_len - 1)}]
            set batch_len [expr {$end_idx - $start_idx + 1}]
            
            # 提取当前批次数据
            set batch_data {}
            for {set i $start_idx} {$i <= $end_idx} {incr i} {
                lappend batch_data [lindex $data_list $i]
            }
            
            # 计算当前批次地址（每个 word = 4 bytes）
            set batch_addr [expr {$addr + $start_idx * 4}]
            set batch_addr_hex [format "0x%08X" $batch_addr]
            
            puts ">>> Batch $batch: writing $batch_len words @ $batch_addr_hex"
            set txn [create_hw_axi_txn w_txn_$batch $axi_if \
                -type write \
                -address $batch_addr_hex \
                -data $batch_data \
                -len $batch_len \
                -force]
            run_hw_axi $txn
        }
    }
    puts ">>> $desc DONE"
}

# AXI 读操作（支持分批读取，返回数据列表）
proc axi_read_data {axi_if addr len {desc "AXI Read"}} {
    global MAX_BURST_LEN
    puts ">>> $desc START (@ [format 0x%08X $addr], total_len=$len)"
    set data_list {}
    
    # 如果数据量小于限制，直接读取
    if {$len <= $MAX_BURST_LEN} {
        set addr_hex [format "0x%08X" $addr]
        set txn [create_hw_axi_txn r_txn $axi_if \
            -type read \
            -address $addr_hex \
            -len $len \
            -force]
        run_hw_axi $txn
        
        # 提取数据并转换为列表
        set raw [get_property DATA $txn]
        set word_len 8  ;# 32-bit = 8 hex chars
        
        for {set i 0} {$i < [string length $raw]} {incr i $word_len} {
            set word [string range $raw $i [expr {$i + $word_len - 1}]]
            lappend data_list [string toupper $word]
        }
    } else {
        # 分批读取
        set batch_count [expr {($len + $MAX_BURST_LEN - 1) / $MAX_BURST_LEN}]
        puts ">>> Splitting into $batch_count batches (max $MAX_BURST_LEN per batch)"
        
        for {set batch 0} {$batch < $batch_count} {incr batch} {
            set start_idx [expr {$batch * $MAX_BURST_LEN}]
            set batch_len [expr {min($MAX_BURST_LEN, $len - $start_idx)}]
            
            # 计算当前批次地址（每个 word = 4 bytes）
            set batch_addr [expr {$addr + $start_idx * 4}]
            set batch_addr_hex [format "0x%08X" $batch_addr]
            
            puts ">>> Batch $batch: reading $batch_len words @ $batch_addr_hex"
            set txn [create_hw_axi_txn r_txn_$batch $axi_if \
                -type read \
                -address $batch_addr_hex \
                -len $batch_len \
                -force]
            run_hw_axi $txn
            
            # 提取当前批次数据
            set raw [get_property DATA $txn]
            set word_len 8  ;# 32-bit = 8 hex chars
            
            for {set i 0} {$i < [string length $raw]} {incr i $word_len} {
                set word [string range $raw $i [expr {$i + $word_len - 1}]]
                lappend data_list [string toupper $word]
            }
        }
    }
    
    puts ">>> $desc DONE (read [llength $data_list] words)"
    return $data_list
}

# 64位地址写入（分为高低32位）
proc write_addr_64bit {axi_if base_addr offset addr_value} {
    set low32  [expr {$addr_value & 0xFFFFFFFF}]
    set high32 [expr {$addr_value >> 32}]
    
    # 写低32位
    axi_write_data $axi_if [expr {$base_addr + $offset}] \
        [format "%08X" $low32] "Write Addr Low32"
    
    # 写高32位
    axi_write_data $axi_if [expr {$base_addr + $offset + 4}] \
        [format "%08X" $high32] "Write Addr High32"
}

# 轮询 ap_done（带超时保护）
proc wait_for_ap_done {axi_if ctrl_base timeout_ms} {
    puts ">>> Waiting for ap_done (timeout=$timeout_ms ms)..."
    set start_time [clock milliseconds]
    set done_mask  0x2  ;# bit[1] = ap_done
    
    while {true} {
        # 读取状态寄存器
        set status_data [axi_read_data $axi_if $ctrl_base 1 "Read ap_ctrl"]
        set status_hex  [lindex $status_data 0]
        set status      [expr "0x$status_hex"]
        
        # 检查 ap_done 位
        if {[expr {$status & $done_mask}] != 0} {
            puts ">>> ap_done detected! (status=0x$status_hex)"
            return true
        }
        
        # 检查超时
        set elapsed [expr {[clock milliseconds] - $start_time}]
        if {$elapsed > $timeout_ms} {
            puts ">>> ERROR: ap_done timeout after $elapsed ms"
            return false
        }
        
        # 等待 1ms 后继续轮询
        after 1
    }
}

# ========================= 主验证流程 =========================

puts "=============================================="
puts " SOCS HLS IP Board Validation Script"
puts " Target: calc_socs_hls (Nx=4, Ny=4)"
puts " Output Size: $output_size × $output_size = $output_words floats"
puts "=============================================="

# # ==================== 步骤 1: 连接硬件并编程 ====================
# puts "\n=== Step 1: Hardware Connection and Programming ==="

# # 断开旧连接（保持你之前的健壮写法）
# close_hw_target -quiet
# disconnect_hw_server -quiet

# # 连接硬件服务器
# puts ">>> Connecting to hardware server..."
# connect_hw_server -url localhost:3121

# # 打开硬件目标
# puts ">>> Opening hardware target..."
# open_hw_target

# # 获取硬件设备
# set hw_device [lindex [get_hw_devices] 0]
# if {$hw_device eq ""} {
#     puts "ERROR: No hardware device found!"
#     return
# }
# current_hw_device $hw_device

# puts ">>> Programming FPGA device with debug probes..."

# # === 关键部分：指定 .bit 和 .ltx 文件 ===
# # 请把下面两行路径改成你实际生成的文件路径
# set bit_file "your_project.runs/impl_1/your_top.bit"          ;# 替换成你的 .bit 文件完整路径
# set ltx_file "your_project.runs/impl_1/debug_nets.ltx"        ;# 替换成你的 .ltx 文件完整路径（通常叫 debug_nets.ltx）

# set_property PROGRAM.FILE $bit_file $hw_device
# set_property PROBES.FILE  $ltx_file $hw_device

# # 执行编程
# program_hw_devices $hw_device

# # 编程完成后必须刷新设备，让 Vivado 读取 debug probes
# puts ">>> Refreshing hardware device to load debug cores..."
# refresh_hw_device $hw_device

# puts ">>> Hardware programmed successfully with ILA debug cores!"

# ==================== 步骤 2: Reset JTAG-to-AXI ====================
puts "\n=== Step 2: Reset AXI Master ==="
reset_hw_axi $axi_if
puts ">>> AXI Master reset complete"

# ==================== 步骤 3: 配置 HLS IP 参数地址 ====================
puts "\n=== Step 3: Configure HLS IP Parameters ==="

# 写入各数据缓冲区的基地址（64位地址）
puts ">>> Configuring mskf_r address (DDR @ [format 0x%08X $ddr_mskf_r])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_MSKF_R $ddr_mskf_r

puts ">>> Configuring mskf_i address (DDR @ [format 0x%08X $ddr_mskf_i])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_MSKF_I $ddr_mskf_i

puts ">>> Configuring scales address (DDR @ [format 0x%08X $ddr_scales])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_SCALES $ddr_scales

puts ">>> Configuring krn_r address (DDR @ [format 0x%08X $ddr_krn_r])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_KRN_R $ddr_krn_r

puts ">>> Configuring krn_i address (DDR @ [format 0x%08X $ddr_krn_i])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_KRN_I $ddr_krn_i

puts ">>> Configuring output_r address (DDR @ [format 0x%08X $ddr_output])..."
write_addr_64bit $axi_if $ctrl_r_base $ADDR_OUTPUT_R $ddr_output

puts ">>> HLS IP parameter configuration complete!"

# ==================== 步骤 4: 准备测试数据（示例模式） ====================
puts "\n=== Step 4: Prepare Test Data (Sample Pattern) ==="

# 注意：实际测试需要从 input/*.bin 文件加载真实数据
# 此处使用示例数据演示流程，实际使用时需替换为真实数据加载逻辑

# 示例：生成简单的测试数据（实际应从 bin 文件读取）
puts ">>> WARNING: Using sample test data (not real golden data)"
puts ">>> For actual validation, please load data from:"
puts "    - input/mask/1024x1024.bin"
puts "    - input/source/src_test1_size101.bin"
puts "    - output/verification/kernels/*.bin"

# 简单测试数据（用于演示流程）
set test_data_pattern {}
for {set i 0} {$i < 16} {incr i} {
    lappend test_data_pattern [format "%08X" $i]
}

# ==================== 步骤 5: 写入测试数据到 DDR4 ====================
puts "\n=== Step 5: Write Test Data to DDR4 ==="

# 注意：实际测试需要写入大量数据（1024×1024 floats = 4MB）
# 此处演示写入少量测试数据，实际使用需分批写入完整数据

puts ">>> Writing sample data to DDR (mskf_r region)..."
axi_write_data $axi_if $ddr_mskf_r $test_data_pattern "Write mskf_r"

puts ">>> Writing sample data to DDR (scales region)..."
axi_write_data $axi_if $ddr_scales $test_data_pattern "Write scales"

puts ">>> Test data write complete (sample mode)"

# ==================== 步骤 6: 启动 HLS IP ====================
puts "\n=== Step 6: Start HLS IP Execution ==="

# 写 ap_start = 1 (bit[0])
puts ">>> Writing ap_start=1 to control register..."
axi_write_data $axi_if $ctrl_base {00000001} "Write ap_start"

puts ">>> HLS IP execution started!"

# ==================== 步骤 7: 等待完成 ====================
puts "\n=== Step 7: Wait for Completion ==="

set done [wait_for_ap_done $axi_if $ctrl_base 10000]
if {!$done} {
    puts "ERROR: HLS IP execution timeout!"
    return
}

puts ">>> HLS IP execution complete!"

# ==================== 步骤 8: 读取输出结果 ====================
puts "\n=== Step 8: Read Output Results ==="

puts ">>> Reading output from DDR @ [format 0x%08X $ddr_output]..."
set output_data [axi_read_data $axi_if $ddr_output $output_words "Read Output"]

puts ">>> Output read complete! (received [llength $output_data] floats)"

# 显示前10个输出值（调试用）
puts ">>> Sample output values (first 10):"
for {set i 0} {$i < 10 && $i < [llength $output_data]} {incr i} {
    puts "    output[$i] = 0x[lindex $output_data $i]"
}

# ==================== 步骤 9: 简单验证（示例） ====================
puts "\n=== Step 9: Validation Summary ==="

puts ">>> Board validation flow completed successfully!"
puts ">>> Hardware execution finished without timeout."
puts ">>> Output data retrieved from DDR memory."

puts "\n⚠️  IMPORTANT NOTES:"
puts "1. This script used SAMPLE test data (not real golden data)"
puts "2. For actual validation, replace Step 4-5 with real data loading:"
puts "   - Load mask data from input/mask/*.bin"
puts "   - Load kernel data from output/verification/kernels/*.bin"
puts "   - Compare output with expected golden reference"
puts "3. Current configuration: Nx=4, Ny=4 (output 17×17)"
puts "4. For Nx=16 configuration, adjust output_size to 65×65"

puts "\n=============================================="
puts " SOCS Board Validation Script Complete"
puts "=============================================="

# ========================= 数据加载辅助说明 =========================
# 
# 实际测试数据加载方法（需外部工具辅助）：
# 
# 方法 1: 使用 Python 预处理
#   - 运行 python validation/golden/run_verification.py 生成 golden 数据
#   - 输出位于 output/verification/ 目录
#   - 使用 Python 脚本将 bin 文件转换为 TCL 可用的 hex 格式
# 
# 方法 2: 使用 MATLAB 生成 hex 文件
#   - 读取 bin 文件并转换为 hex 字符串列表
#   - 保存为 .tcl 文件可直接 source 加载
# 
# 方法 3: 分批写入（适用于大数据量）
#   - 将 1024×1024 数据分为多个 burst（如每次 1024 floats）
#   - 循环写入各批次到 DDR 不同偏移地址
# 
# 示例 Python 转换脚本片段：
#   import struct
#   with open('1024x1024.bin', 'rb') as f:
#       data = f.read()
#   floats = struct.unpack(f'{len(data)//4}f', data)
#   hex_list = [f'{struct.pack("f", val).hex():08X}' for val in floats[:N]]
#   print('set test_data {' + ' '.join(hex_list) + '}')
# 
# ==============================================================================