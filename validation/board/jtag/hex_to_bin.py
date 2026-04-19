#!/usr/bin/env python3
"""
Convert IEEE 754 hex format to binary float format.
Inverse of bin_to_hex_for_ddr.py
"""

import struct
import argparse
import os

def hex_to_bin(hex_file: str, bin_file: str, expected_count: int = 289):
    """Convert hex IEEE 754 values to binary floats."""
    
    values = []
    
    with open(hex_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Handle continuous hex string (8 chars per float)
            # Split into 8-character chunks
            for i in range(0, len(line), 8):
                chunk = line[i:i+8]
                if len(chunk) != 8:
                    continue
                try:
                    hex_val = int(chunk, 16)
                    # Convert to float (big-endian IEEE 754)
                    float_val = struct.unpack('!f', struct.pack('!I', hex_val))[0]
                    values.append(float_val)
                except ValueError as e:
                    print(f"Warning: Skipping invalid hex chunk: {chunk}")
                    continue
    
    print(f"Parsed {len(values)} values from hex file")
    
    if len(values) != expected_count:
        print(f"Warning: Expected {expected_count} values, got {len(values)}")
    
    # Write binary file
    with open(bin_file, 'wb') as f:
        for val in values:
            f.write(struct.pack('<f', val))  # Little-endian float
    
    print(f"Written {len(values)} floats to {bin_file}")
    
    # Print statistics
    import numpy as np
    arr = np.array(values)
    print(f"\n[Statistics]")
    print(f"  Mean:   {arr.mean():.6e}")
    print(f"  Std:    {arr.std():.6e}")
    print(f"  Min:    {arr.min():.6e}")
    print(f"  Max:    {arr.max():.6e}")
    print(f"  Range:  {arr.max() - arr.min():.6e}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert hex IEEE 754 to binary floats')
    parser.add_argument('--hex', required=True, help='Input hex file')
    parser.add_argument('--bin', required=True, help='Output binary file')
    parser.add_argument('--count', type=int, default=289, help='Expected number of values')
    
    args = parser.parse_args()
    
    os.makedirs(os.path.dirname(args.bin) if os.path.dirname(args.bin) else '.', exist_ok=True)
    hex_to_bin(args.hex, args.bin, args.count)