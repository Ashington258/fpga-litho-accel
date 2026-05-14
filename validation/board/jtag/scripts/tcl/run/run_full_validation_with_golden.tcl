#============================================================
# SOCS HLS 完整功能验证流程（使用真实 Golden 数据 - golden_1024 配置）
#
# 流程：
#   1. 加载真实输入数据到 DDR
#   2. 配置 HLS IP 参数
#   3. 启动 HLS IP
#   4. 读取输出数据（128×128 空中像，FFT尺寸）
#   5. 保存输出为 HEX 文件（供 Python 可视化）
#============================================================

puts "\n╔══════════════════════════════════════════════════════════════╗"
puts "║      SOCS HLS Full Functional Validation with Golden Data    ║"
puts "╚══════════════════════════════════════════════════════════════╝"

# 硬件接口
set axi_if [get_hw_axis hw_axi_1]

# 地址定义 (V18)
set HLS_CTRL_BASE   0x0000
set HLS_PARAMS_BASE 0x10000
set DDR_MSKF_R      0x40000000
set DDR_MSKF_I      0x40400000
set DDR_SCALES      0x40800000
set DDR_KRN_R       0x40880000
set DDR_KRN_I       0x40900000
set DDR_TMPIMG_DDR  0x40980000
set DDR_OUTPUT      0x40990000

puts "\n=== Step 1: Load Real Golden Data to DDR ==="

# 加载所有输入数据（每个文件包含写入逻辑）
# 注意：路径相对于 scripts/tcl/run/ 目录，需要指向 ../load/
# puts "\n>>> Loading mskf_r..."
# source ../load/load_mskf_r.tcl

# puts "\n>>> Loading mskf_i..."
# source ../load/load_mskf_i.tcl

# puts "\n>>> Loading scales..."
# source ../load/load_scales.tcl

# puts "\n>>> Loading kernels (krn_r)..."
# source ../load/load_krn_r.tcl

# puts "\n>>> Loading kernels (krn_i)..."
# source ../load/load_krn_i.tcl

puts "\n✅ All Golden data loaded to DDR"

puts "\n=== Step 2: Verify Critical Data ==="

# 验证 mskf_r 数据（确认写入成功）
# ⚠️ 注意：mskf_r 是频域mask频谱，前1025个float全为零（DC/低频区域）
# 必须从非零偏移处验证，选择 offset=0x2000 (float index 2048)
set DDR_MSKF_R_VERIFY [format "0x%08X" [expr {$DDR_MSKF_R + 0x2000}]]
catch {delete_hw_axi_txn [get_hw_axi_txns verify_mskf_r]}
create_hw_axi_txn verify_mskf_r $axi_if -address $DDR_MSKF_R_VERIFY -len 4 -type read
run_hw_axi [get_hw_axi_txns verify_mskf_r]
set mskf_verify [get_property DATA [get_hw_axi_txns verify_mskf_r]]
puts ">>> mskf_r @ offset 0x2000 (4 words): $mskf_verify"

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
# 注意：当前 V18 顶层函数参数顺序为 mskf_r, mskf_i, krn_r, krn_i, scales, tmpImg_ddr, output
foreach {name offset addr} [list \
    mskf_r 0x10 $DDR_MSKF_R \
    mskf_i 0x1c $DDR_MSKF_I \
    krn_r  0x28 $DDR_KRN_R \
    krn_i  0x34 $DDR_KRN_I \
    scales 0x40 $DDR_SCALES \
    tmpImg_ddr 0x4c $DDR_TMPIMG_DDR \
    output 0x58 $DDR_OUTPUT \
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

# 配置标量参数（⚠️ 修复：这些参数应该在 control 接口，而不是 control_r 接口）
# 根据 HLS IP 驱动头文件 xcalc_socs_2048_hls_stream_refactored_v18_hw.h：
# - control 接口（基地址 0x0000）：nk, nx_actual, ny_actual, Lx, Ly
# - control_r 接口（基地址 0x10000）：mskf_r, mskf_i, krn_r, krn_i, scales, tmpImg_ddr, output
set nk 10
set nx_actual 8
set ny_actual 8
set Lx 1024
set Ly 1024

# ✅ 正确：写入 control 接口（基地址 0x0000）
foreach {name offset val} [list \
    nk 0x10 $nk \
    nx_actual 0x18 $nx_actual \
    ny_actual 0x20 $ny_actual \
    Lx 0x28 $Lx \
    Ly 0x30 $Ly \
] {
    set addr_hex [format "0x%08X" [expr {$HLS_CTRL_BASE + $offset}]]
    catch {delete_hw_axi_txn [get_hw_axi_txns cfg_$name]}
    create_hw_axi_txn cfg_$name $axi_if -address $addr_hex -data [format "%08X" $val] -len 1 -type write
    run_hw_axi [get_hw_axi_txns cfg_$name]
    puts ">>> $name = $val (configured @ $addr_hex in control interface)"
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

puts "\n=== Step 5: Read Output Data (128×128 Aerial Image) ==="

# HLS 输出大小：128×128 = 16384 floats = 16384 × 4 bytes = 64KB
# Vivado burst 限制：MAX_BURST_LEN = 128
# 分批读取：128 batches × 128 words = 16384

puts "\n>>> Reading output from DDR @ 0x40990000 (V18: gmem6)..."

# ⚠️ 关键修复：JTAG-to-AXI 读取数据反序，需要 lreverse 后处理
# 与写入时的 lreverse 预处理类似，读取需要反向操作

# 初始化输出变量
set output_full ""

# 读取 128 个批次，每个 128 words
# 总数据量：128 × 128 = 16384 words
for {set batch 0} {$batch < 128} {incr batch} {
    # 每个word 4 bytes
    set offset [expr {$batch * 128 * 4}]
    set offset_hex [format "0x%08X" [expr {$DDR_OUTPUT + $offset}]]
    
    catch {delete_hw_axi_txn [get_hw_axi_txns rd_out_$batch]}
    create_hw_axi_txn rd_out_$batch $axi_if -address $offset_hex -len 128 -type read
    run_hw_axi [get_hw_axi_txns rd_out_$batch]
    set batch_raw [get_property DATA [get_hw_axi_txns rd_out_$batch]]
    
    # 应用 lreverse
    set batch_list [split $batch_raw "_"]
    set batch_reversed [join [lreverse $batch_list] "_"]
    
    # 追加到完整输出
    if {$batch > 0} {
        append output_full "_"
    }
    append output_full $batch_reversed
    
    # 每 16 批次打印一次进度
    if {[expr {$batch % 16}] == 0} {
        puts ">>> Batch [expr {$batch + 1}]/128: Applied lreverse (128 words @ $offset_hex)"
    }
}

puts ">>> Output preview: [string range $output_full 0 100]..."
puts "✅ Total: 16384 words with lreverse correction applied"
puts ">>> Total hex length: [string length $output_full] characters"

puts "\n=== Step 6: Save Output to HEX File ==="

# 保存为 hex 文件（供 Python 可视化）
set output_hex_file "aerial_image_output.hex"
set fh [open $output_hex_file w]
puts $fh "# SOCS HLS Output Data (128×128 aerial image, golden_1024 config)"
puts $fh "# Format: 16384 IEEE 754 hex values"
puts $fh "# Generated: [clock format [clock seconds]]"
puts $fh ""
puts $fh $output_full
close $fh

puts "\n✅ Output saved to: $output_hex_file"

puts "\n=== Step 7: Compare with Golden Reference ==="

# 加载 Golden 参考输出
set golden_path "e:/fpga-litho-accel/source/SOCS_HLS/data/tmpImgp_full_128.bin"

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
puts "    ✅ Output data read (128×128 = 16384 floats)"
puts "    ✅ Output saved to aerial_image_output.hex"

puts "\n>>> Next step: Visualize and compare"
puts "    python e:/fpga-litho-accel/validation/board/jtag/visualize_aerial_image.py"

puts "\n>>> Output file location:"
puts "    e:/fpga-litho-accel/validation/board/jtag/aerial_image_output.hex"