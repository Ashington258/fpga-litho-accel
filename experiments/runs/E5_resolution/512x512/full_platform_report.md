# 全平台 PCIe 板级验证报告

- Config: `experiments/config/resolution/config_512x512_nk10.json`
- Golden output dir: `experiments/data/E5_resolution/golden/512x512_nk10`
- Lx/Ly: 512×512
- Nx/Ny: 4×4
- kernels: 10 (9×9)
- FPGA tmpImgp: 128×128
- Host FI output: 512×512

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000086 | `{}` |
| load_golden_data | PASS | 0.001943 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.001498 | `{"address": "0x40000000", "bytes": 1048576, "mib_per_second": 667.4738648870699}` |
| pcie_h2c_write_mskf_i | PASS | 0.001300 | `{"address": "0x40400000", "bytes": 1048576, "mib_per_second": 769.3029651884623}` |
| pcie_h2c_write_scales | PASS | 0.000048 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.7872335218102311}` |
| pcie_h2c_write_krn_r | PASS | 0.000038 | `{"address": "0x40880000", "bytes": 3240, "mib_per_second": 80.44113382348624}` |
| pcie_h2c_write_krn_i | PASS | 0.000038 | `{"address": "0x40900000", "bytes": 3240, "mib_per_second": 81.23845263435173}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000096 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000093 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.000765 | `{}` |
| fpga_compute | PASS | 0.020416 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000226 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000231 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E5_resolution/512x512/fpga_tmpimgp_full_128.bin"}` |
| compare_tmpimgp_only | PASS | 0.003699 | `{}` |

Total measured time: `0.030479s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 1.3601567429e-08 | 7.4505805969e-08 | 1.0217566040e-06 | 1.0000000000 | 141.85 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E5_resolution/512x512/fpga_tmpimgp_full_128.bin`
- metrics_csv: `experiments/runs/E5_resolution/512x512/metrics.csv`
- timing_csv: `experiments/runs/E5_resolution/512x512/timing.csv`
- report: `experiments/runs/E5_resolution/512x512/full_platform_report.md`
