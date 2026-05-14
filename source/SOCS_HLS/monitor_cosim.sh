#!/bin/bash
# Co-Simulation Progress Monitor for v18
# Usage: ./monitor_cosim.sh

LOG_FILE="cosim_v18_background.log"
XSIM_LOG="socs_v18_comp/hls/sim/verilog/xsim.log"

echo "========================================="
echo "v18 Co-Simulation Progress Monitor"
echo "========================================="
echo ""

# Check if process is running
PID=$(ps aux | grep "vitis-run.*cosim.*v18" | grep -v grep | awk '{print $2}')
if [ -z "$PID" ]; then
    echo "⚠️  Co-Sim process NOT running"
else
    echo "✅ Co-Sim process running (PID: $PID)"
fi
echo ""

# Check log file
if [ -f "$LOG_FILE" ]; then
    echo "📄 Latest log entries:"
    echo "----------------------------------------"
    tail -20 "$LOG_FILE"
    echo ""
fi

# Check simulation progress
if [ -f "$XSIM_LOG" ]; then
    echo "📊 Simulation Progress:"
    echo "----------------------------------------"
    # Extract progress line
    grep "RTL Simulation" "$XSIM_LOG" | tail -5
    echo ""
    
    # Extract simulation time
    SIM_TIME=$(grep "Time:" "$XSIM_LOG" | tail -1 | grep -oP 'Time: \K[0-9]+')
    if [ ! -z "$SIM_TIME" ]; then
        # Convert to microseconds
        SIM_TIME_US=$((SIM_TIME / 1000000))
        TOTAL_TIME_US=8790000  # 8.79ms = 8,790,000ns
        PROGRESS=$((SIM_TIME_US * 100 / TOTAL_TIME_US))
        
        echo "⏱️  Simulation Time: ${SIM_TIME_US} μs / 8,790 μs"
        echo "📈 Estimated Progress: ${PROGRESS}%"
        echo ""
    fi
fi

# Check for completion
if grep -q "C/RTL co-simulation finished: PASS" "$LOG_FILE" 2>/dev/null; then
    echo "✅ Co-Simulation PASSED!"
elif grep -q "C/RTL co-simulation finished: FAIL" "$LOG_FILE" 2>/dev/null; then
    echo "❌ Co-Simulation FAILED!"
    echo "Check log for details: $LOG_FILE"
fi

echo "========================================="
echo "To monitor continuously: watch -n 10 ./monitor_cosim.sh"
echo "To stop Co-Sim: kill $PID"
echo "========================================="
