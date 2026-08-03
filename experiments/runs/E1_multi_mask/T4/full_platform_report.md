# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T4_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T4`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000085 | `{}` |
| load_golden_data | PASS | 0.004122 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005407 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 739.817885970496}` |
| pcie_h2c_write_mskf_i | PASS | 0.006075 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 658.4063008123208}` |
| pcie_h2c_write_scales | PASS | 0.000121 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.31462978574679445}` |
| pcie_h2c_write_krn_r | PASS | 0.000101 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 109.35352053434535}` |
| pcie_h2c_write_krn_i | PASS | 0.000121 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 90.93925566634199}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000160 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000229 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001289 | `{}` |
| fpga_compute | PASS | 0.020470 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000201 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000167 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T4/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.032387 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002004 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T4/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000886 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000884 | `{}` |
| compare_against_golden | PASS | 0.085749 | `{}` |

Total measured time: `0.160460s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 1.6946123865e-08 | 2.8312206268e-07 | 9.2966286938e-06 | 1.0000000000 | 141.23 |
| host_FI_vs_golden_SOCS | ✅ | 1.7102544848e-08 | 2.9802322388e-07 | 1.5176461780e-05 | 1.0000000000 | 141.15 |
| host_FI_vs_TCC_direct | ✅ | 1.9701203510e-03 | 3.1231618486e-03 | 9.8089208189e-01 | 0.9999703977 | 40.00 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T4/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T4/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T4/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T4/timing.csv`
- report: `experiments/runs/E1_multi_mask/T4/full_platform_report.md`
