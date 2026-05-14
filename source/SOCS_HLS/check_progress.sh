#!/bin/bash
# Quick progress check for v18 Co-Sim

LOG="cosim_v18_background.log"
XSIM_LOG="socs_v18_comp/hls/sim/verilog/xsim.log"

# Process status
PID=$(ps aux | grep "vitis-run.*cosim.*v18" | grep -v grep | awk '{print $2}' | head -1)
if [ -z "$PID" ]; then
    echo "⏹️  Co-Sim stopped"
    
    # Check result
    if grep -q "C/RTL co-simulation finished: PASS" "$LOG" 2>/dev/null; then
        echo "✅ Result: PASS"
    elif grep -q "C/RTL co-simulation finished: FAIL" "$LOG" 2>/dev/null; then
        echo "❌ Result: FAIL"
    fi
    exit 0
else
    echo "▶️  Co-Sim running (PID: $PID)"
fi

# Progress
if [ -f "$XSIM_LOG" ]; then
    PROGRESS=$(grep "RTL Simulation" "$XSIM_LOG" | tail -1)
    if [ ! -z "$PROGRESS" ]; then
        echo "📊 $PROGRESS"
    fi
    
    # Simulation time
    SIM_TIME=$(grep "Time:" "$XSIM_LOG" | tail -1 | grep -oP 'Time: \K[0-9]+')
    if [ ! -z "$SIM_TIME" ]; then
        SIM_US=$((SIM_TIME / 1000000))
        TOTAL_US=8790
        PCT=$((SIM_US * 100 / TOTAL_US))
        echo "⏱️  ${SIM_US}μs / 8790μs (${PCT}%)"
    fi
fi
