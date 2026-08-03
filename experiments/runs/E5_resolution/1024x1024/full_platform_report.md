# 全平台 PCIe 板级验证报告

- Config: `experiments/config/resolution/config_1024x1024_nk10.json`
- Golden output dir: `experiments/data/E5_resolution/golden/1024x1024_nk10`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000098 | `{}` |
| load_golden_data | PASS | 0.004019 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005847 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 684.1122116083295}` |
| pcie_h2c_write_mskf_i | PASS | 0.007626 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 524.5203424187912}` |
| pcie_h2c_write_scales | PASS | 0.000100 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.38316331761984285}` |
| pcie_h2c_write_krn_r | PASS | 0.000113 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 97.91873931310613}` |
| pcie_h2c_write_krn_i | PASS | 0.000084 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 130.7751314015364}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000167 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000187 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001138 | `{}` |
| fpga_compute | PASS | 0.020537 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000286 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000467 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E5_resolution/1024x1024/fpga_tmpimgp_full_128.bin"}` |
| compare_tmpimgp_only | PASS | 0.003322 | `{}` |

Total measured time: `0.043991s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.9303560514e-08 | 3.5762786865e-07 | 2.6540381633e-06 | 1.0000000000 | 142.26 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E5_resolution/1024x1024/fpga_tmpimgp_full_128.bin`
- metrics_csv: `experiments/runs/E5_resolution/1024x1024/metrics.csv`
- timing_csv: `experiments/runs/E5_resolution/1024x1024/timing.csv`
- report: `experiments/runs/E5_resolution/1024x1024/full_platform_report.md`
