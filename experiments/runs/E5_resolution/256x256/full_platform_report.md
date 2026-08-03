# 全平台 PCIe 板级验证报告

- Config: `experiments/config/resolution/config_256x256_nk10.json`
- Golden output dir: `experiments/data/E5_resolution/golden/256x256_nk10`
- Lx/Ly: 256×256
- Nx/Ny: 2×2
- kernels: 10 (5×5)
- FPGA tmpImgp: 128×128
- Host FI output: 256×256

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000098 | `{}` |
| load_golden_data | PASS | 0.001254 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.000424 | `{"address": "0x40000000", "bytes": 262144, "mib_per_second": 590.0791106677584}` |
| pcie_h2c_write_mskf_i | PASS | 0.000311 | `{"address": "0x40400000", "bytes": 262144, "mib_per_second": 803.4167819424202}` |
| pcie_h2c_write_scales | PASS | 0.000032 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 1.1818259279112513}` |
| pcie_h2c_write_krn_r | PASS | 0.000026 | `{"address": "0x40880000", "bytes": 1000, "mib_per_second": 37.30536796899037}` |
| pcie_h2c_write_krn_i | PASS | 0.000023 | `{"address": "0x40900000", "bytes": 1000, "mib_per_second": 41.31321507854182}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000070 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000065 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.000450 | `{}` |
| fpga_compute | PASS | 0.020735 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000334 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000358 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E5_resolution/256x256/fpga_tmpimgp_full_128.bin"}` |
| compare_tmpimgp_only | PASS | 0.006480 | `{}` |

Total measured time: `0.030660s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 9.9870436674e-09 | 7.4505805969e-08 | 7.6128769421e-07 | 1.0000000000 | 142.20 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E5_resolution/256x256/fpga_tmpimgp_full_128.bin`
- metrics_csv: `experiments/runs/E5_resolution/256x256/metrics.csv`
- timing_csv: `experiments/runs/E5_resolution/256x256/timing.csv`
- report: `experiments/runs/E5_resolution/256x256/full_platform_report.md`
