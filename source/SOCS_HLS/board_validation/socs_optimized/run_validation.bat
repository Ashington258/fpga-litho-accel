@echo off
REM SOCS Optimized HLS IP Board Validation Batch Script
REM Generated: 2026-04-23

echo ========================================
echo SOCS Optimized HLS IP Board Validation
echo ========================================
echo.

REM Check if Vivado is available
where vivado >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Vivado not found in PATH
    echo Please ensure Vivado 2025.2 is installed and in PATH
    pause
    exit /b 1
)

REM Check if Python is available
where python >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Python not found in PATH
    echo Please ensure Python 3.8+ is installed and in PATH
    pause
    exit /b 1
)

echo Starting validation process...
echo.

REM Step 1: Generate TCL script if needed
if not exist "run_socs_optimized_validation.tcl" (
    echo Step 1: Generating TCL validation script...
    python generate_socs_optimized_tcl.py
    if %errorlevel% neq 0 (
        echo ERROR: Failed to generate TCL script
        pause
        exit /b 1
    )
    echo TCL script generated successfully.
) else (
    echo Step 1: TCL script already exists.
)
echo.

REM Step 2: Test TCL syntax
echo Step 2: Testing TCL script syntax...
python test_tcl_syntax.py
if %errorlevel% neq 0 (
    echo ERROR: TCL syntax test failed
    pause
    exit /b 1
)
echo TCL syntax test passed.
echo.

REM Step 3: Display instructions
echo ========================================
echo Validation Setup Complete
echo ========================================
echo.
echo Next steps:
echo 1. Open Vivado Hardware Manager
echo 2. Connect to hardware (localhost:3121)
echo 3. Run the following TCL command:
echo    source e:/fpga-litho-accel/source/SOCS_HLS/board_validation/socs_optimized/run_socs_optimized_validation.tcl
echo.
echo 4. After validation completes, run:
echo    python verify_socs_optimized_results.py
echo.
echo ========================================
echo.
echo Press any key to open Vivado Hardware Manager...
pause >nul

REM Try to open Vivado Hardware Manager
start vivado -mode hardware
echo.
echo Vivado Hardware Manager started.
echo Please follow the instructions above.
echo.
pause
