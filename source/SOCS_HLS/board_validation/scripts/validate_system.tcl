# =================================================================
# AMD/Xilinx JTAG-AXI Automation Script for Ashington System
# 适用环境: Vivado 2024.x / 2025.x 硬件管理器
# =================================================================
current_hw_target [get_hw_targets]
set_property PARAM.FREQUENCY 125000 [get_hw_targets]   
proc run_system_check {} {
    # 1. 硬件连接初始化
    puts "--- INFO: Initializing Hardware Connection ---"
    if {[get_hw_targets -quiet] == ""} {
        connect_hw_server -quiet
        open_hw_target
    }
    
    # 定位 JTAG-AXI 实例 (根据你的 BD 命名)
    # set j2a_master [get_hw_axis -quiet *jtag_axi_0*]
    set j2a_master [get_hw_axis]
    if {$j2a_master == ""} {
        puts "--- ERROR: No JTAG-AXI master found! Check your BD design. ---"
        return
    }
    puts "--- INFO: Found Master: $j2a_master"

    # 2. 时钟与复位初步判断 (通过刷新总线)
    # 如果时钟没锁死或处于持续复位，此处刷新可能会报超时错误
    if {[catch {refresh_hw_axi $j2a_master} err]} {
        puts "--- CRITICAL: AXI Bus is unresponsive! Possible Clock/Reset issue. ---"
        puts "Detailed Error: $err"
        return
    }

    # 3. 自动化内存压力扫描 (针对你的 axi_bram_ctrl_0)
    # 请根据你的 Address Editor 修改以下地址
    set BRAM0_BASE 40000000 
    set TEST_DATA  ABCDEFF0
    
    puts "--- INFO: Testing BRAM_0 at $BRAM0_BASE ---"
    
    # 执行写操作
    create_hw_axi_txn wr_test $j2a_master -address $BRAM0_BASE -data $TEST_DATA -type write -force
    run_hw_axi [get_hw_axi_txn wr_test]
    
    # 执行读操作
    create_hw_axi_txn rd_test $j2a_master -address $BRAM0_BASE -type read -force
    run_hw_axi [get_hw_axi_txn rd_test]
    
    # 获取结果并对比
    set rd_val [get_property DATA [get_hw_axi_txn rd_test]]
    
    if {$rd_val == $TEST_DATA} {
        puts "--- RESULT: SYSTEM OK! ---"
        puts "Clock (clk_wiz_0) is running, Reset (proc_sys_reset) is released."
        puts "AXI Interconnect (smartconnect_0) path to BRAM is clear."
    } else {
        puts "--- RESULT: FAILED! ---"
        puts "Data Mismatch: Wrote $TEST_DATA, Read $rd_val"
    }
    
    # 4. HLS 模块响应检查 (验证计算内核是否挂载)
    # 假设 HLS 控制寄存器在 0x44A00000
    set HLS_CTRL 0x44A00000
    create_hw_axi_txn hls_check $j2a_master -address $HLS_CTRL -type read -force
    if {[catch {run_hw_axi [get_hw_axi_txn hls_check]} err]} {
        puts "--- WARNING: HLS Module not responding. ---"
    } else {
        puts "--- INFO: HLS IP is accessible on AXI-Lite bus. ---"
    }
}

# 执行主函数
run_system_check