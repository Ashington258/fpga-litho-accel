#============================================================
# SOCS HLS 完整功能验证流程（使用真实 Golden 数据）
#
# 流程：
#   1. 加载真实输入数据到 DDR
#   2. 配置 HLS IP 参数
#   3. 启动 HLS IP
#   4. 读取输出数据（17×17 空中像）
#   5. 保存输出为 HEX 文件（供 Python 可视化）
#============================================================

puts "\n╔══════════════════════════════════════════════════════════════╗"
puts "║      SOCS HLS Full Functional Validation with Golden Data    ║"
puts "╚══════════════════════════════════════════════════════════════╝"

# 硬件接口
set axi_if [get_hw_axis hw_axi_1]

# 地址定义
set HLS_CTRL_BASE   0x0000
set HLS_PARAMS_BASE 0x10000
set DDR_MSKF_R      0x40000000
set DDR_MSKF_I      0x42000000
set DDR_SCALES      0x44000000
set DDR_KRN_R       0x44400000
set DDR_KRN_I       0x44800000
set DDR_OUTPUT      0x44840000

puts "\n=== Step 1: Load Real Golden Data to DDR ==="

# 加载所有输入数据（每个文件包含写入逻辑）
puts "\n>>> Loading mskf_r..."
source load_mskf_r.tcl

puts "\n>>> Loading mskf_i..."
source load_mskf_i.tcl

puts "\n>>> Loading scales..."
source load_scales.tcl

puts "\n>>> Loading kernels (krn_r)..."
source load_krn_r.tcl

puts "\n>>> Loading kernels (krn_i)..."
source load_krn_i.tcl

puts "\n✅ All Golden data loaded to DDR"

puts "\n=== Step 2: Verify Critical Data ==="

# 验证 mskf_r 首批数据（确认写入成功）
catch {delete_hw_axi_txn [get_hw_axi_txns verify_mskf_r]}
create_hw_axi_txn verify_mskf_r $axi_if -address $DDR_MSKF_R -len 4 -type read
run_hw_axi [get_hw_axi_txns verify_mskf_r]
set mskf_verify [get_property DATA [get_hw_axi_txns verify_mskf_r]]
puts ">>> mskf_r first 4 words: $mskf_verify"

# 验证 scales
catch {delete_hw_axi_txn [get_hw_axi_txns verify_scales]}
create_hw_axi_txn verify_scales $axi_if -address $DDR_SCALES -len 4 -type read
run_hw_axi [get_hw_axi_txns verify_scales]
set scales_verify [get_property DATA [get_hw_axi_txns verify_scales]]
puts ">>> scales first 4 words: $scales_verify"

if {[regexp {^0+$} $mskf_verify] || [regexp {^0+$} $scales_verify]} {
    puts "\n❌ DDR data verification failed! Data still zeros."
    puts ">>> Check: 1) DDR4 configuration  2) TCL load scripts executed correctly"
    return
} else {
    puts "\n✅ DDR data verification passed (non-zero readback)"
}

puts "\n=== Step 3: Configure HLS IP Parameters ==="

reset_hw_axi $axi_if

# 配置指针参数（64-bit 地址拆分为 low/high 32-bit）
foreach {name offset addr} [list \
    mskf_r 0x10 $DDR_MSKF_R \
    mskf_i 0x1c $DDR_MSKF_I \
    scales 0x28 $DDR_SCALES \
    krn_r  0x34 $DDR_KRN_R \
    krn_i  0x40 $DDR_KRN_I \
    output 0x4c $DDR_OUTPUT \
] {
    set low  [expr {$addr & 0xFFFFFFFF}]
    set high [expr {$addr >> 32}]

    set addr_low_hex  [format "0x%08X" [expr {$HLS_PARAMS_BASE + $offset}]]
    set addr_high_hex [format "0x%08X" [expr {$HLS_PARAMS_BASE + $offset + 4}]]

    catch {delete_hw_axi_txn [get_hw_axi_txns cfg_$name\_low]}
    create_hw_axi_txn cfg_$name\_low $axi_if -address $addr_low_hex -data [format "%08X" $low] -len 1 -type write
    run_hw_axi [get_hw_axi_txns cfg_$name\_low]

    catch {delete_hw_axi_txn [get_hw_axi_txns cfg_$name\_high]}
    create_hw_axi_txn cfg_$name\_high $axi_if -address $addr_high_hex -data [format "%08X" $high] -len 1 -type write
    run_hw_axi [get_hw_axi_txns cfg_$name\_high]

    puts ">>> $name = $addr (configured)"
}

puts "\n✅ HLS IP parameters configured"

puts "\n=== Step 4: Start HLS IP ==="

# 启动前状态检查
catch {delete_hw_axi_txn [get_hw_axi_txns ctrl_before]}
create_hw_axi_txn ctrl_before $axi_if -address $HLS_CTRL_BASE -len 1 -type read
run_hw_axi [get_hw_axi_txns ctrl_before]
set ctrl_before_data [get_property DATA [get_hw_axi_txns ctrl_before]]
puts ">>> ap_ctrl BEFORE start: $ctrl_before_data"

# 启动 HLS IP (ap_start = 1)
catch {delete_hw_axi_txn [get_hw_axi_txns start_ip]}
create_hw_axi_txn start_ip $axi_if -address $HLS_CTRL_BASE -data 00000001 -len 1 -type write
run_hw_axi [get_hw_axi_txns start_ip]
puts ">>> ap_start written"

# 等待 ap_done（轮询最多 10 秒）
puts "\n>>> Waiting for HLS IP completion..."
set done 0
set timeout 100

for {set i 0} {$i < $timeout} {incr i} {
    catch {delete_hw_axi_txn [get_hw_axi_txns poll_ctrl]}
    create_hw_axi_txn poll_ctrl $axi_if -address $HLS_CTRL_BASE -len 1 -type read
    run_hw_axi [get_hw_axi_txns poll_ctrl]
    set ctrl_status [get_property DATA [get_hw_axi_txns poll_ctrl]]

    # 转换 hex 字符串为数值
    set ctrl_val 0
    if {[regexp {^[0-9A-Fa-f]+$} $ctrl_status]} {
        set ctrl_val [expr "0x$ctrl_status"]
    }

    set ap_done [expr {($ctrl_val >> 1) & 0x01}]

    if {$ap_done == 1} {
        puts ">>> ap_ctrl = $ctrl_status (done after $i polls)"
        set done 1
        break
    }

    # 每 10 次轮询打印一次状态
    if {[expr {$i % 10}] == 0} {
        puts ">>> Polling... ($i/$timeout) ap_ctrl = $ctrl_status"
    }
}

if {!$done} {
    puts "\n❌ HLS IP timeout! Check HLS IP design or input data validity"
    return
} else {
    puts "\n✅ HLS IP execution completed"
}

puts "\n=== Step 5: Read Output Data (17×17 Aerial Image) ==="

# HLS 输出大小：17×17 = 289 floats = 289 × 4 bytes
# Vivado burst 限制：MAX_BURST_LEN = 128
# 分批读取：128 + 128 + 33

puts "\n>>> Reading output from DDR @ 0x44840000..."

# Batch 1: 128 words (地址偏移 0)
catch {delete_hw_axi_txn [get_hw_axi_txns rd_out_1]}
create_hw_axi_txn rd_out_1 $axi_if -address $DDR_OUTPUT -len 128 -type read
run_hw_axi [get_hw_axi_txns rd_out_1]
set output_batch1 [get_property DATA [get_hw_axi_txns rd_out_1]]

# Batch 2: 128 words (地址偏移 128×4 = 512 bytes = 0x200)
catch {delete_hw_axi_txn [get_hw_axi_txns rd_out_2]}
create_hw_axi_txn rd_out_2 $axi_if -address [format "0x%08X" [expr {$DDR_OUTPUT + 0x200}]] -len 128 -type read
run_hw_axi [get_hw_axi_txns rd_out_2]
set output_batch2 [get_property DATA [get_hw_axi_txns rd_out_2]]

# Batch 3: 33 words (地址偏移 256×4 = 1024 bytes = 0x400)
catch {delete_hw_axi_txn [get_hw_axi_txns rd_out_3]}
create_hw_axi_txn rd_out_3 $axi_if -address [format "0x%08X" [expr {$DDR_OUTPUT + 0x400}]] -len 33 -type read
run_hw_axi [get_hw_axi_txns rd_out_3]
set output_batch3 [get_property DATA [get_hw_axi_txns rd_out_3]]

puts ">>> Output batch 1 (first 128 words): [string range $output_batch1 0 50]..."
puts ">>> Output batch 2 (mid 128 words):    [string range $output_batch2 0 50]..."
puts ">>> Output batch 3 (last 33 words):    [string range $output_batch3 0 50]..."

# 合并完整输出
set output_full "${output_batch1}${output_batch2}${output_batch3}"

puts "\n=== Step 6: Save Output to HEX File ==="

# 保存为 hex 文件（供 Python 可视化）
set output_hex_file "aerial_image_output.hex"
set fh [open $output_hex_file w]
puts $fh "# SOCS HLS Output Data (17×17 aerial image)"
puts $fh "# Format: 289 IEEE 754 hex values"
puts $fh "# Generated: [clock format [clock seconds]]"
puts $fh ""
puts $fh $output_full
close $fh

puts "\n✅ Output saved to: $output_hex_file"

puts "\n=== Step 7: Compare with Golden Reference ==="

# 加载 Golden 参考输出
set golden_path "e:/fpga-litho-accel/source/SOCS_HLS/data/tmpImgp_pad32.bin"

if {[file exists $golden_path]} {
    puts "\n>>> Golden reference found: $golden_path"
    puts ">>> Run Python visualization script to compare:"
    puts "    python visualize_aerial_image.py"
} else {
    puts "\n⚠️ Golden reference not found at $golden_path"
}

puts "\n╔══════════════════════════════════════════════════════════════╗"
puts "║                  VALIDATION SUMMARY                          ║"
puts "╚══════════════════════════════════════════════════════════════╝"

puts "\n>>> Steps completed:"
puts "    ✅ DDR data loaded (mskf_r/i, scales, krn_r/i)"
puts "    ✅ DDR data verified (non-zero readback)"
puts "    ✅ HLS IP parameters configured"
puts "    ✅ HLS IP execution completed"
puts "    ✅ Output data read (17×17 = 289 floats)"
puts "    ✅ Output saved to aerial_image_output.hex"

puts "\n>>> Next step: Visualize and compare"
puts "    python e:/fpga-litho-accel/validation/board/jtag/visualize_aerial_image.py"

puts "\n>>> Output file location:"
puts "    e:/fpga-litho-accel/validation/board/jtag/aerial_image_output.hex"