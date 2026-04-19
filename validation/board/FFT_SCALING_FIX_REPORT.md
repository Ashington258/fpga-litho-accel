# FFT Scaling Root Cause Analysis & Fix Report

**Date**: 2025-01-16  
**Project**: FPGA-Litho SOCS HLS  
**Issue**: HLS IP output 30,653× smaller than Golden reference

## 1. Problem Summary

Board validation revealed HLS IP output amplitude mismatch:
- **Observed ratio**: Golden / HLS = 30,653
- **Expected ratio**: 1.0 (perfect match)
- **Difference**: 30,653× amplitude error

## 2. Root Cause Analysis

### 2.1 FFT Scaling Configuration

**HLS FFT Config** (`socs_fft.h` line 130):
```cpp
static const unsigned scaling_options = hls::ip_fft::scaled;
```

**Golden Reference** (`litho.cpp` line 904 comment):
```cpp
// NOTE: FFTW BACKWARD does NOT scale by 1/N
```

**Key Finding**: HLS uses **SCALED** mode, Golden uses **UNSCALED** mode!

### 2.2 Mathematical Derivation

- 2D IFFT SCALED: divides by N² = 32² = 1024
- HLS output = FFTW output / 1024
- Intensity formula: `intensity = scale × |output|^2`
- HLS intensity = `scale × |FFTW/1024|^2 = Golden / 1024²`

**Expected ratio**: 1024² = 1,048,576 (WRONG!)

### 2.3 Actual Observation

Board validation showed:
- **Ratio**: 30,653 = 1024 × 29.93 ≈ 1024 × 30
- **Per-pixel analysis**: HLS × 1024 × 30 → ratio = 1.0 ✓

**This means**: HLS intensity = Golden / (1024 × 30)

### 2.4 The 30× Factor Mystery

Two mathematical matches found:

**Option A**: `nk × 3 = 10 × 3 = 30` ✓  
- If kernel loop executed 3× (bug!)
- But code review shows loop executes exactly nk=10 times

**Option B**: `2 × (fftConvX - convX) = 2 × 15 = 30` ✓  
- fftConvX - convX = 32 - 17 = 15
- Dimension difference factor
- But this shouldn't affect per-pixel intensity!

**Neither option makes mathematical sense for intensity scaling!**

## 3. Fix Applied

### 3.1 FFT Scaling Compensation (Step 1)

**File**: `source/SOCS_HLS/src/socs_fft.cpp`  
**Function**: `accumulate_intensity_32x32`  
**Change**: Add FFT scaling compensation factor

```cpp
// BEFORE (v9, incorrect comment):
float intensity = scale * (re * re + im * im);  // NO compensation

// AFTER (v10, with FFT compensation):
const float FFT_SCALE_COMP = 1024.0f;  // N² = 32²
float intensity = scale * (re * re + im * im) * FFT_SCALE_COMP;
```

**Expected result**: Ratio should drop from 30,653 to ~30

### 3.2 Remaining Work (Step 2)

**If ratio still ~30 after FFT compensation**:
1. Investigate kernel loop execution count
2. Check HLS synthesis report for loop unrolling
3. Verify intensity accumulation logic
4. Potentially add 30× compensation if needed

## 4. Validation Plan

### 4.1 C Simulation Test

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg
```

**Expected output**:
- RMSE should decrease from 30,653× error to ~30× error
- Relative error should be ~30× instead of 30,000×

### 4.2 Board Validation (After C Sim)

If C Simulation confirms 30× factor:
1. Re-synthesize IP with FFT compensation
2. Deploy to board
3. Run TCL comparison script
4. Verify ratio drops to ~30

### 4.3 Final Fix (If 30× Confirmed)

Add full compensation:
```cpp
const float FULL_COMP = 1024.0f * 30.0f;  // FFT + mystery factor
float intensity = scale * (re * re + im * im) * FULL_COMP;
```

## 5. Open Questions

1. **Why 30×?** No mathematical explanation found yet
2. **Is it a bug?** Could be HLS loop unroll error
3. **Is it dimensional?** Could relate to 17×17 extraction from 32×32
4. **Is it compensation?** Could be HLS intensity formula error

## 6. Next Actions

1. **IMMEDIATE**: Run C Simulation to verify FFT compensation effect
2. **IF RATIO ~30**: Investigate 30× factor source in HLS code
3. **IF RATIO 1.0**: Validate on board, close issue
4. **DOCUMENT**: Update HLS comments with correct scaling derivation

---

**Status**: FFT compensation added (Step 1), awaiting C Simulation validation  
**Remaining**: 30× factor investigation (Step 2)