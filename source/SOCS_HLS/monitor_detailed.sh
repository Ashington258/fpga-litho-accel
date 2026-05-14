#!/bin/bash
# Detailed progress monitor for v18 Co-Sim

LOG="cosim_v18_background.log"
XSIM_LOG="socs_v18_comp/hls/sim/verilog/xsim.log"

echo "========================================="
echo "v18 Co-Sim Detailed Progress"
echo "========================================="

# Process status
PID=$(ps aux | grep "vitis-run.*cosim.*v18" | grep -v grep | awk '{print $2}' | head -1)
if [ -z "$PID" ]; then
    echo "⏹️  Status: STOPPED"
    
    # Check result
    if grep -q "C/RTL co-simulation finished: PASS" "$LOG" 2>/dev/null; then
        echo "✅ Result: PASS"
        exit 0
    elif grep -q "C/RTL co-simulation finished: FAIL" "$LOG" 2>/dev/null; then
        echo "❌ Result: FAIL"
        exit 1
    else
        echo "⚠️  Result: UNKNOWN (check log)"
        exit 2
    fi
else
    echo "▶️  Status: RUNNING (PID: $PID)"
fi

# Determine current stage
if grep -q "Starting C TB testing" "$LOG" 2>/dev/null; then
    if ! grep -q "C TB testing finished" "$LOG" 2>/dev/null; then
        echo "📍 Stage: C Testbench Execution"
    fi
fi

if grep -q "Analyzing SystemVerilog file" "$LOG" 2>/dev/null; then
    if ! grep -q "Starting static elaboration" "$LOG" 2>/dev/null; then
        echo "📍 Stage: RTL Compilation (analyzing Verilog/VHDL)"
    fi
fi

if grep -q "Starting static elaboration" "$LOG" 2>/dev/null; then
    if ! grep -q "static elaboration complete" "$LOG" 2>/dev/null; then
        echo "📍 Stage: RTL Elaboration"
    fi
fi

if grep -q "xsim" "$LOG" 2>/dev/null && [ -f "$XSIM_LOG" ]; then
    echo "📍 Stage: RTL Simulation (xsim)"
    
    # Simulation progress
    SIM_TIME=$(grep "Time:" "$XSIM_LOG" | tail -1 | grep -oP 'Time: \K[0-9]+')
    if [ ! -z "$SIM_TIME" ]; then
        SIM_US=$((SIM_TIME / 1000000))
        TOTAL_US=8790
        PCT=$((SIM_US * 100 / TOTAL_US))
        echo "⏱️  Simulation: ${SIM_US}μs / 8790μs (${PCT}%)"
    fi
fi

# File counts
if [ -d "socs_v18_comp/hls/sim/verilog" ]; then
    VHDL_COUNT=$(find socs_v18_comp/hls/sim/verilog -name "*.vhd" 2>/dev/null | wc -l)
    SV_COUNT=$(find socs_v18_comp/hls/sim/verilog -name "*.sv" 2>/dev/null | wc -l)
    echo "📁 Compiled: ${VHDL_COUNT} VHDL, ${SV_COUNT} SystemVerilog files"
fi

echo ""
echo "Commands:"
echo "  Watch progress: watch -n 10 ./monitor_detailed.sh"
echo "  Stop Co-Sim:    kill $PID"
echo "  Full log:       tail -f $LOG"
