# v18 Co-Sim Status Report

## Current Status (2026-04-26)

✅ **Co-Sim Running in Background**
- PID: 1250986
- Stage: RTL Simulation (xsim)
- Progress: 0/1 transactions, 109μs simulation time

## Monitoring Commands

### Quick Check
```bash
cd /home/ashington/project/optimization/fpga-litho-accel/source/SOCS_HLS
./check_progress.sh
```

### Detailed Progress
```bash
./monitor_detailed.sh
```

### Continuous Monitoring (auto-refresh every 10s)
```bash
watch -n 10 ./monitor_detailed.sh
```

### Full Log
```bash
tail -f cosim_v18_background.log
```

### xsim Log (RTL simulation details)
```bash
tail -f socs_v18_comp/hls/sim/verilog/xsim.log
```

## Expected Timeline

Based on previous experience with FFT RTL simulation:
- **C TB Execution**: ~1-2 minutes ✅ DONE
- **RTL Compilation**: ~3-5 minutes ✅ DONE
- **RTL Elaboration**: ~2-3 minutes ✅ DONE
- **RTL Simulation**: **~2-4 hours** ⏳ IN PROGRESS (0%)

**Total Expected Time**: ~3-5 hours

## Progress Indicators

Watch for these in `xsim.log`:
```
RTL Simulation : 0 / 1 [0.00%] @ "109000"    ← Current
RTL Simulation : 0 / 1 [25.00%] @ "..."      ← 25% intra-transaction
RTL Simulation : 1 / 1 [100.00%] @ "..."     ← Complete
```

## Success Criteria

- ✅ C Simulation: PASS (RMSE=2.93e-08)
- ✅ C Synthesis: PASS (Fmax~310MHz, BRAM 55%, DSP 3%)
- ⏳ Co-Simulation: RUNNING
- ⏹️ Board Validation: NOT STARTED

## If Co-Sim Fails

1. Check error in `cosim_v18_background.log`
2. Look for "FAIL" or "ERROR" messages
3. Common issues:
   - Timeout: Increase `cosim.max_time` in config
   - Mismatch: Check data types and interfaces
   - xsim crash: Check memory usage

## Next Steps After Co-Sim PASS

1. Record experience to memory
2. Prepare board-level validation:
   - Use exported IP: `socs_v18_comp/hls/impl/ip/`
   - Configure DDR addresses
   - Prepare test data
3. Run hardware test via JTAG-to-AXI

## Files

- **Source**: `src/socs_2048_stream_refactored_v18_hls_fft.cpp`
- **Config**: `script/config/hls_config_socs_v18_xsim.cfg`
- **Log**: `cosim_v18_background.log`
- **xsim Log**: `socs_v18_comp/hls/sim/verilog/xsim.log`
- **Monitor Scripts**: `check_progress.sh`, `monitor_detailed.sh`
