#!/usr/bin/env python3
"""
FPGA-Litho 验证快捷入口
"""

import sys
from pathlib import Path

verification_dir = Path(__file__).parent / "verification"
sys.path.insert(0, str(verification_dir))

from run_verification import main

if __name__ == "__main__":
    sys.exit(main())