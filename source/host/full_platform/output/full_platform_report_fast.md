# 全平台 PCIe 板级验证报告

- Config: `input/config/golden_1024.json`
- Golden output dir: `output/verification`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000084 | `{}` |
| load_golden_data | PASS | 0.003908 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005746 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 696.1921768859867}` |
| pcie_h2c_write_mskf_i | PASS | 0.007805 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 512.5129393475726}` |
| pcie_h2c_write_scales | PASS | 0.000099 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.38636888413128506}` |
| pcie_h2c_write_krn_r | PASS | 0.000101 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 109.25273609370244}` |
| pcie_h2c_write_krn_i | PASS | 0.000072 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 153.35413070740634}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000125 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000108 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001021 | `{}` |
| fpga_compute | PASS | 0.020517 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000293 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000597 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_tmpimgp_full_128.bin"}` |
| compare_tmpimgp_only | PASS | 0.003697 | `{}` |

Total measured time: `0.044171s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.9303560514e-08 | 3.5762786865e-07 | 2.6540381633e-06 | 1.0000000000 | 142.26 |

## 输出文件

- fpga_tmpimgp: `source/host/full_platform/output/fpga_tmpimgp_full_128.bin`
- metrics_csv: `source/host/full_platform/output/metrics.csv`
- timing_csv: `source/host/full_platform/output/timing.csv`
- report: `source/host/full_platform/output/full_platform_report.md`
