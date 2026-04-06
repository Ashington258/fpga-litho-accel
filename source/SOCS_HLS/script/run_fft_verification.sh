#!/bin/bash
# SOCS HLS FFT 正确验证流程
# FPGA-Litho Project
#
# 参考文档：copilot-instructions.md - "hls::fft C仿真限制（重要）"
#
# 关键原则：
#   1. HLS FFT IP在纯C Simulation时不输出数据（已知限制）
#   2. 推荐流程：C Synthesis → Co-Simulation（使用cycle-accurate RTL模型）
#   3. 临时绕过：csim.flags添加-DALLOW_EMPTY_HLS_STREAM_READS（错误降级为警告）
#   4. 最佳实践：csim/cosim都加-stdmath保证FFT数学准确性

set -e  # 遇到错误立即退出

WORK_DIR="/root/project/FPGA-Litho/source/SOCS_HLS"
CONFIG_FILE="hls/script/config/hls_config_fft_safe.cfg"
PROJECT_DIR="hls/fft_proj"

echo "=========================================="
echo "SOCS HLS FFT Verification Pipeline"
echo "=========================================="
echo ""
echo "参考：copilot-instructions.md - FFT验证流程"
echo ""

cd $WORK_DIR

# ============================================================================
# Step 1: C Synthesis（验证硬件结构）
# ============================================================================
echo "[Step 1] Running C Synthesis..."
echo "    目的：生成RTL硬件结构，验证接口和资源估计"
echo ""

vitis-run --mode hls --tcl --input_file hls/script/run_csynth.tcl --work_dir $PROJECT_DIR

# 检查综合结果
if [ -f "$PROJECT_DIR/solution1/syn/report/fft_2d_forward_32_csynth.rpt" ]; then
    echo "✓ C Synthesis completed successfully"
    echo ""
    echo "关键指标："
    grep -A 5 "Estimated Resources" $PROJECT_DIR/solution1/syn/report/fft_2d_forward_32_csynth.rpt || true
    echo ""
else
    echo "✗ C Synthesis failed"
    exit 1
fi

# ============================================================================
# Step 2: C/RTL Co-Simulation（真正验证FFT功能）
# ============================================================================
echo "[Step 2] Running C/RTL Co-Simulation..."
echo "    目的：使用cycle-accurate RTL模型验证FFT输出"
echo "    关键：CoSim使用Verilog/VHDL仿真器，FFT IP会真正计算"
echo ""

vitis-run --mode hls --cosim --config $CONFIG_FILE --work_dir $PROJECT_DIR

# 检查CoSim结果
if [ -f "$PROJECT_DIR/solution1/sim/report/fft_2d_forward_32_cosim.rpt" ]; then
    echo "✓ Co-Simulation completed"
    echo ""
    
    # 检查PASS/FAIL状态
    if grep -q "PASS" $PROJECT_DIR/solution1/sim/report/fft_2d_forward_32_cosim.rpt; then
        echo "✓✓✓ FFT VERIFICATION PASSED ✓✓✓"
        echo ""
        echo "关键指标："
        grep -A 10 "Performance Estimates" $PROJECT_DIR/solution1/sim/report/fft_2d_forward_32_cosim.rpt || true
        echo ""
    else
        echo "✗✗✗ FFT VERIFICATION FAILED ✗✗✗"
        echo ""
        echo "请检查仿真日志："
        tail -50 $PROJECT_DIR/solution1/sim/report/fft_2d_forward_32_cosim.log || true
        exit 1
    fi
else
    echo "✗ Co-Simulation failed"
    exit 1
fi

# ============================================================================
# Step 3: IP Packaging（可选）
# ============================================================================
echo "[Step 3] Packaging IP (optional)..."
echo ""

vitis-run --mode hls --package --config $CONFIG_FILE --work_dir $PROJECT_DIR

if [ -f "$PROJECT_DIR/solution1/impl/export/ip/fft_2d_forward_32.zip" ]; then
    echo "✓ IP Package created: $PROJECT_DIR/solution1/impl/export/ip/fft_2d_forward_32.zip"
    echo ""
fi

echo "=========================================="
echo "FFT Verification Pipeline Completed"
echo "=========================================="
echo ""
echo "下一步："
echo "  1. 检查综合报告（资源利用率、时序）"
echo "  2. 检查CoSim报告（性能、正确性）"
echo "  3. 将FFT模块集成到calcSOCS主流程"
echo ""