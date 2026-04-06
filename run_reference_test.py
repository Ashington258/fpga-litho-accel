#!/usr/bin/env python3
"""
Litho-TCC Reference Test Script
================================
This script runs the reference C++ implementation of Litho-TCC simulation
and generates output files (aerial image + SOCS kernels) for HLS verification.

All parameters are read from config.json by default:
  - mask.inputFile: input mask file path
  - source.inputFile: input source file path  
  - mask.size, mask.period: mask dimensions
  - optics.NA, optics.defocus: optical parameters
  - kernel.count: number of SOCS kernels

Usage:
    python run_reference_test.py                    # Use config.json for all parameters
    python run_reference_test.py --verbose          # Enable debug logging
    python run_reference_test.py --mask-file X.bin  # Override mask file
    python run_reference_test.py --output-dir out   # Override output directory
"""

import json
import os
import sys
import argparse
import subprocess
import time
from pathlib import Path
from datetime import datetime

# Constants
DEFAULT_CONFIG = "input/config/config.json"
DEFAULT_OUTPUT_DIR = "output/reference"
KLITHO_TCC_PATH = "reference/CPP_reference/Litho-TCC/klitho_tcc"
# Default files (used only when config.json inputFile is invalid)
DEFAULT_MASK_FILE = "input/mask/test_256.bin"
DEFAULT_SOURCE_FILE = "input/source/src_test1_size101.bin"

class Colors:
    """ANSI color codes for terminal output"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def log_info(msg: str, verbose: bool = True):
    """Log info message with timestamp"""
    if verbose:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"{Colors.OKBLUE}[INFO {timestamp}] {msg}{Colors.ENDC}")

def log_debug(msg: str, verbose: bool = False):
    """Log debug message"""
    if verbose:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"{Colors.OKCYAN}[DEBUG {timestamp}] {msg}{Colors.ENDC}")

def log_success(msg: str):
    """Log success message"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"{Colors.OKGREEN}[SUCCESS {timestamp}] {msg}{Colors.ENDC}")

def log_error(msg: str):
    """Log error message"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"{Colors.FAIL}[ERROR {timestamp}] {msg}{Colors.ENDC}")

def log_warning(msg: str):
    """Log warning message"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"{Colors.WARNING}[WARNING {timestamp}] {msg}{Colors.ENDC}")

def load_config(config_path: str) -> dict:
    """Load and validate configuration file"""
    log_info(f"Loading config from: {config_path}")
    
    if not os.path.exists(config_path):
        log_error(f"Config file not found: {config_path}")
        sys.exit(1)
    
    with open(config_path, 'r') as f:
        config = json.load(f)
    
    log_debug(f"Config loaded successfully", verbose=True)
    return config

def validate_mask_file_size(mask_file: str, expected_size_x: int, expected_size_y: int) -> bool:
    """
    Validate that mask file size matches expected dimensions.
    Returns True if valid, False with warning if mismatch.
    """
    if os.path.exists(mask_file):
        file_size = os.path.getsize(mask_file)
        expected_bytes = expected_size_x * expected_size_y * 4  # float32 = 4 bytes
        if file_size != expected_bytes:
            log_warning(f"Mask file size mismatch!")
            log_warning(f"  File: {mask_file} ({file_size} bytes)")
            log_warning(f"  Expected for {expected_size_x}x{expected_size_y}: {expected_bytes} bytes")
            log_warning(f"  Please update config.json mask.size to match actual file!")
            return False
        else:
            log_info(f"Mask file size validated: {expected_size_x}x{expected_size_y} ({file_size} bytes)")
            return True
    return False

def build_command_args(config: dict, mask_file: str, source_file: str, output_dir: str) -> list:
    """
    Build command line arguments for klitho_tcc
    
    All parameters are read from config.json - no auto-detection.
    
    Parameter mapping (23 arguments total):
    1.  Lx - mask period x
    2.  Ly - mask period y
    3.  maskSizeX - mask size x
    4.  maskSizeY - mask size y
    5.  maskType - "Import" or "LineSpace"
    6.  lineWidth - for LineSpace pattern
    7.  spaceWidth - for LineSpace pattern
    8.  isHorizontal - 0 or 1
    9.  maskFile - input mask file path
    10. srcSize - source grid size
    11. srcType - "Annular", "Dipole", "CrossQuadrupole", "Point", "Import"
    12. innerRadius - for Annular
    13. outerRadius - for Annular
    14. radius - for Dipole/CrossQuadrupole
    15. offset - for Dipole/CrossQuadrupole
    16. onXAxis - 0 or 1 for Dipole
    17. ptX - for Point source
    18. ptY - for Point source
    19. sourceFile - input source file path
    20. NA - numerical aperture
    21. defocus - normalized defocus
    22. nk - number of kernels
    23. outputDir - output directory
    """
    args = []
    
    # Mask parameters (args 1-9) - ALL from config.json
    Lx = config['mask']['period']['Lx']
    Ly = config['mask']['period']['Ly']
    maskSizeX = config['mask']['size']['maskSizeX']
    maskSizeY = config['mask']['size']['maskSizeY']
    maskType = config['mask']['type']
    lineWidth = config['mask']['lineSpace']['lineWidth']
    spaceWidth = config['mask']['lineSpace']['spaceWidth']
    isHorizontal = 1 if config['mask']['lineSpace']['isHorizontal'] else 0
    
    # Validate mask file size against config if Import type
    if maskType == "Import":
        validate_mask_file_size(mask_file, maskSizeX, maskSizeY)
    
    args.extend([str(Lx), str(Ly), str(maskSizeX), str(maskSizeY), 
                 maskType, str(lineWidth), str(spaceWidth), str(isHorizontal),
                 mask_file])
    
    # Source parameters (args 10-19)
    srcSize = config['source']['gridSize']
    srcType = config['source']['type']
    innerRadius = config['source']['annular']['innerRadius']
    outerRadius = config['source']['annular']['outerRadius']
    radius = config['source']['dipole']['radius']
    offset = config['source']['dipole']['offset']
    onXAxis = 1 if config['source']['dipole']['onXAxis'] else 0
    ptX = config['source']['point']['x']
    ptY = config['source']['point']['y']
    
    args.extend([str(srcSize), srcType, 
                 str(innerRadius), str(outerRadius),
                 str(radius), str(offset), str(onXAxis),
                 str(ptX), str(ptY),
                 source_file])
    
    # Optics parameters (args 20-21)
    NA = config['optics']['NA']
    defocus = config['optics']['defocus']
    
    args.extend([str(NA), str(defocus)])
    
    # Kernel parameter (arg 22)
    nk = config['kernel']['count']
    args.append(str(nk))
    
    # Output directory (arg 23)
    args.append(output_dir)
    
    return args

def check_input_files(mask_file: str, source_file: str):
    """Verify input files exist"""
    log_info(f"Checking input files...")
    
    files_to_check = [
        ("Mask file", mask_file),
        ("Source file", source_file),
    ]
    
    all_exist = True
    for name, path in files_to_check:
        if os.path.exists(path):
            size = os.path.getsize(path)
            log_debug(f"{name}: {path} ({size} bytes)", verbose=True)
        else:
            log_warning(f"{name} not found: {path}")
            all_exist = False
    
    return all_exist

def ensure_output_dir(output_dir: str):
    """Create output directory structure"""
    log_info(f"Creating output directory: {output_dir}")
    
    dirs_to_create = [
        output_dir,
        os.path.join(output_dir, "kernels"),
        os.path.join(output_dir, "kernels", "png"),
    ]
    
    for dir_path in dirs_to_create:
        Path(dir_path).mkdir(parents=True, exist_ok=True)
        log_debug(f"Created: {dir_path}", verbose=True)

def run_simulation(exe_path: str, args: list, verbose: bool = False):
    """Execute klitho_tcc simulation"""
    log_info(f"Running simulation...")
    log_info(f"Executable: {exe_path}")
    log_debug(f"Arguments: {args}", verbose=True)
    
    if not os.path.exists(exe_path):
        log_error(f"Executable not found: {exe_path}")
        sys.exit(1)
    
    start_time = time.time()
    
    # Run simulation and capture output
    cmd = [exe_path] + args
    
    log_info(f"Command: {' '.join(cmd)}")
    print(f"\n{'='*60}")
    print(f"  Litho-TCC Reference Simulation Output")
    print(f"{'='*60}\n")
    
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
        
        # Print output in real-time
        for line in process.stdout:
            print(line, end='')
            if verbose:
                log_debug(line.strip(), verbose=verbose)
        
        process.wait()
        elapsed_time = time.time() - start_time
        
        print(f"\n{'='*60}")
        print(f"  Simulation Completed")
        print(f"{'='*60}\n")
        
        if process.returncode == 0:
            log_success(f"Simulation finished successfully in {elapsed_time:.2f}s")
            return True
        else:
            log_error(f"Simulation failed with exit code: {process.returncode}")
            return False
            
    except Exception as e:
        log_error(f"Failed to run simulation: {e}")
        return False

def verify_output_files(output_dir: str, config: dict):
    """Verify output files were generated correctly"""
    log_info(f"Verifying output files...")
    
    Lx = config['mask']['period']['Lx']
    Ly = config['mask']['period']['Ly']
    nk = config['kernel']['count']
    
    expected_image_size = Lx * Ly * 4  # float32 = 4 bytes
    
    files_to_check = [
        ("Aerial image (BIN)", os.path.join(output_dir, "image.bin"), expected_image_size),
        ("Aerial image (PNG)", os.path.join(output_dir, "image.png"), None),
        ("Segmented image (PNG)", os.path.join(output_dir, "image_s.png"), None),
        ("Mask preview (PNG)", os.path.join(output_dir, "mask.png"), None),
        ("Source preview (PNG)", os.path.join(output_dir, "source.png"), None),
        ("TCC real (PNG)", os.path.join(output_dir, "tcc_r.png"), None),
        ("TCC imag (PNG)", os.path.join(output_dir, "tcc_i.png"), None),
        ("Kernel info (TXT)", os.path.join(output_dir, "kernels", "kernel_info.txt"), None),
    ]
    
    # Add kernel files
    for i in range(nk):
        files_to_check.extend([
            (f"Kernel {i} real (BIN)", os.path.join(output_dir, "kernels", f"krn_{i}_r.bin"), None),
            (f"Kernel {i} imag (BIN)", os.path.join(output_dir, "kernels", f"krn_{i}_i.bin"), None),
            (f"Kernel {i} real (PNG)", os.path.join(output_dir, "kernels", "png", f"krn_{i}_r.png"), None),
            (f"Kernel {i} imag (PNG)", os.path.join(output_dir, "kernels", "png", f"krn_{i}_i.png"), None),
        ])
    
    all_valid = True
    for name, path, expected_size in files_to_check:
        if os.path.exists(path):
            actual_size = os.path.getsize(path)
            if expected_size is not None:
                if actual_size == expected_size:
                    log_success(f"{name}: {path} ({actual_size} bytes - correct size)")
                else:
                    log_warning(f"{name}: {path} ({actual_size} bytes, expected {expected_size})")
                    all_valid = False
            else:
                log_success(f"{name}: {path} ({actual_size} bytes)")
        else:
            log_error(f"{name}: NOT FOUND at {path}")
            all_valid = False
    
    # Check kernel_info.txt content
    kernel_info_path = os.path.join(output_dir, "kernels", "kernel_info.txt")
    if os.path.exists(kernel_info_path):
        log_debug(f"Kernel info content:", verbose=True)
        with open(kernel_info_path, 'r') as f:
            content = f.read()
            log_debug(content.strip(), verbose=True)
    
    return all_valid

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Run Litho-TCC reference simulation for HLS verification",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        '--config', '-c',
        default=DEFAULT_CONFIG,
        help=f"Configuration file path (default: {DEFAULT_CONFIG})"
    )
    
    parser.add_argument(
        '--mask-file', '-m',
        default=None,
        help="Override mask file path (default: read from config.json mask.inputFile)"
    )
    
    parser.add_argument(
        '--source-file', '-s',
        default=None,
        help="Override source file path (default: read from config.json source.inputFile)"
    )
    
    parser.add_argument(
        '--output-dir', '-o',
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory (default: {DEFAULT_OUTPUT_DIR})"
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Enable verbose debug output"
    )
    
    parser.add_argument(
        '--skip-verify',
        action='store_true',
        help="Skip output file verification"
    )
    
    args = parser.parse_args()
    
    # Print header
    print(f"\n{Colors.HEADER}{Colors.BOLD}")
    print("="*60)
    print("  Litho-TCC Reference Test Script")
    print("  FPGA-Litho HLS Verification Golden Reference")
    print("="*60)
    print(f"{Colors.ENDC}\n")
    
    # Resolve paths relative to project root
    project_root = Path(__file__).parent.absolute()
    
    config_path = str(Path(args.config).absolute())
    output_dir = str(Path(args.output_dir).absolute())
    
    # Load configuration FIRST to get inputFile paths
    config = load_config(config_path)
    
    # Get mask/source file from config.json inputFile field
    # Command line args can override if provided
    if args.mask_file:
        mask_file = args.mask_file
        log_info(f"Using override mask file from command line: {mask_file}")
    else:
        mask_file = config['mask'].get('inputFile', DEFAULT_MASK_FILE)
        log_debug(f"Using mask file from config.json: {mask_file}", verbose=True)
    
    if args.source_file:
        source_file = args.source_file
        log_info(f"Using override source file from command line: {source_file}")
    else:
        source_file = config['source'].get('inputFile', DEFAULT_SOURCE_FILE)
        log_debug(f"Using source file from config.json: {source_file}", verbose=True)
    
    mask_file = str(Path(mask_file).absolute())
    source_file = str(Path(source_file).absolute())
    
    exe_path = str(project_root / KLITHO_TCC_PATH)
    
    log_info(f"Project root: {project_root}")
    log_info(f"Config: {config_path}")
    log_info(f"Mask: {mask_file}")
    log_info(f"Source: {source_file}")
    log_info(f"Output: {output_dir}")
    log_info(f"Executable: {exe_path}")
    
    # Check input files
    if not check_input_files(mask_file, source_file):
        log_error("Input files missing. Please check paths.")
        sys.exit(1)
    
    # Ensure output directory exists
    ensure_output_dir(output_dir)
    
    # Build command arguments
    cmd_args = build_command_args(config, mask_file, source_file, output_dir)
    
    log_debug(f"Command args count: {len(cmd_args)}", verbose=True)
    log_debug(f"Full command: {exe_path} {' '.join(cmd_args)}", verbose=args.verbose)
    
    # Run simulation
    success = run_simulation(exe_path, cmd_args, args.verbose)
    
    if not success:
        log_error("Simulation failed!")
        sys.exit(1)
    
    # Verify output files
    if not args.skip_verify:
        if not verify_output_files(output_dir, config):
            log_warning("Some output files missing or incorrect size")
    
    log_success(f"All done! Output saved to: {output_dir}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())