# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T5_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T5`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000065 | `{}` |
| load_golden_data | PASS | 0.004122 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005687 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 703.3141392859858}` |
| pcie_h2c_write_mskf_i | PASS | 0.008495 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 470.87086859688804}` |
| pcie_h2c_write_scales | PASS | 0.000146 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.26075198854674264}` |
| pcie_h2c_write_krn_r | PASS | 0.000147 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 74.83606413254032}` |
| pcie_h2c_write_krn_i | PASS | 0.000126 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 87.52570531854215}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000161 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000190 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001271 | `{}` |
| fpga_compute | PASS | 0.020645 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000300 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000412 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T5/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.033344 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002026 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T5/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000852 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000791 | `{}` |
| compare_against_golden | PASS | 0.085449 | `{}` |

Total measured time: `0.164229s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 3.0899670065e-08 | 2.9802322388e-07 | 2.0202188894e-06 | 1.0000000000 | 142.07 |
| host_FI_vs_golden_SOCS | ✅ | 3.1250769016e-08 | 3.4272670746e-07 | 3.8201211895e-06 | 1.0000000000 | 141.98 |
| host_FI_vs_TCC_direct | ✅ | 5.3835510151e-03 | 8.0318450928e-03 | 6.8693356319e-01 | 0.9999780773 | 37.38 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T5/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T5/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T5/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T5/timing.csv`
- report: `experiments/runs/E1_multi_mask/T5/full_platform_report.md`
