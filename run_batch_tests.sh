#!/bin/bash

# Batch test script for Different_resolution_tests
# Uses 48 cores (OMP_NUM_THREADS=48)

set -e

PROJECT_ROOT="/home/ashington/project/optimization/fpga-litho-accel"
CONFIG_FILE="$PROJECT_ROOT/input/config/batch_tests/Different_resolution_tests.json"
OUTPUT_BASE="$PROJECT_ROOT/output/Different_resolution_tests"
EXECUTABLE="$PROJECT_ROOT/validation/golden/src/litho_thread"

# Set number of threads
export OMP_NUM_THREADS=48

echo "=========================================="
echo "Batch Test: Different Resolution Tests"
echo "=========================================="
echo "Using $OMP_NUM_THREADS cores"
echo "Config: $CONFIG_FILE"
echo "Output: $OUTPUT_BASE"
echo ""

# Test cases from JSON
declare -A TEST_CASES=(
    ["256x256"]="256:256:input/mask/Different_resolution_tests/256x256.bin"
    ["512x512"]="512:512:input/mask/Different_resolution_tests/512x512.bin"
    ["1024x1024"]="1024:1024:input/mask/Different_resolution_tests/1024x1024.bin"
    ["2048x2048"]="2048:2048:input/mask/Different_resolution_tests/2048x2048.bin"
    ["4096x4096"]="4096:4096:input/mask/Different_resolution_tests/4096x4096.bin"
    ["8192x8192"]="8192:8192:input/mask/Different_resolution_tests/8192x8192.bin"
)

# Function to create config file for a test case
create_config() {
    local test_name=$1
    local lx=$2
    local ly=$3
    local mask_file=$4
    local config_path=$5
    
    cat > "$config_path" << EOF
{
    "simulation": {
        "description": "Batch test: $test_name",
        "version": "1.0"
    },
    "mask": {
        "period": {
            "Lx": $lx,
            "Ly": $ly
        },
        "size": {
            "maskSizeX": $lx,
            "maskSizeY": $ly
        },
        "type": "Import",
        "lineSpace": {
            "lineWidth": 128,
            "spaceWidth": 128,
            "isHorizontal": false
        },
        "inputFile": "$mask_file",
        "dose": 1.0
    },
    "source": {
        "gridSize": 101,
        "type": "Annular",
        "annular": {
            "innerRadius": 0.6,
            "outerRadius": 0.9
        },
        "dipole": {
            "radius": 0.5,
            "offset": 0.3,
            "onXAxis": true
        },
        "crossQuadrupole": {
            "radius": 0.5,
            "offset": 0.3
        },
        "point": {
            "x": 0.0,
            "y": 0.0
        },
        "inputFile": ""
    },
    "optics": {
        "NA": 0.8,
        "wavelength": 193,
        "defocus": 0.2
    },
    "kernel": {
        "count": 128,
        "targetIntensity": 0.225
    },
    "output": {
        "baseDir": "$OUTPUT_BASE/$test_name",
        "kernelsDir": "$OUTPUT_BASE/$test_name/kernels",
        "kernelsPngDir": "$OUTPUT_BASE/$test_name/kernels/png",
        "imageFile": "$OUTPUT_BASE/$test_name/image.bin",
        "imagePng": "$OUTPUT_BASE/$test_name/image.png",
        "imageSegmentedPng": "$OUTPUT_BASE/$test_name/image_s.png",
        "maskPng": "$OUTPUT_BASE/$test_name/mask.png",
        "sourcePng": "$OUTPUT_BASE/$test_name/source.png",
        "tccRealPng": "$OUTPUT_BASE/$test_name/tcc_r.png",
        "tccImagPng": "$OUTPUT_BASE/$test_name/tcc_i.png"
    }
}
EOF
}

# Run each test case
for test_name in "${!TEST_CASES[@]}"; do
    IFS=':' read -r lx ly mask_file <<< "${TEST_CASES[$test_name]}"
    
    echo "----------------------------------------"
    echo "Running test: $test_name"
    echo "  Mask size: ${lx}x${ly}"
    echo "  Mask file: $mask_file"
    
    # Create output directory
    mkdir -p "$OUTPUT_BASE/$test_name"
    
    # Create config file
    config_path="$OUTPUT_BASE/${test_name}_config.json"
    create_config "$test_name" "$lx" "$ly" "$mask_file" "$config_path"
    
    # Run test
    echo "  Starting simulation..."
    start_time=$(date +%s)
    
    $EXECUTABLE "$config_path" "$OUTPUT_BASE/$test_name" --verbose 2>&1 | tee "$OUTPUT_BASE/${test_name}.log"
    
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    
    echo "  Completed in ${elapsed}s"
    echo ""
done

echo "=========================================="
echo "All tests completed!"
echo "Results saved to: $OUTPUT_BASE"
echo "=========================================="
