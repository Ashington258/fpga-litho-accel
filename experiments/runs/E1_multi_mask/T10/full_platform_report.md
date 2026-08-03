# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T10_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T10`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000084 | `{}` |
| load_golden_data | PASS | 0.004336 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.006097 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 656.0732707673313}` |
| pcie_h2c_write_mskf_i | PASS | 0.008546 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 468.0611460202752}` |
| pcie_h2c_write_scales | PASS | 0.000118 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.3242384119066187}` |
| pcie_h2c_write_krn_r | PASS | 0.000116 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 95.15505699378456}` |
| pcie_h2c_write_krn_i | PASS | 0.000104 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 105.53270990804079}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000168 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000140 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001125 | `{}` |
| fpga_compute | PASS | 0.020394 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000295 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000498 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T10/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.033164 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002039 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T10/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000826 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000904 | `{}` |
| compare_against_golden | PASS | 0.086277 | `{}` |

Total measured time: `0.165229s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.0885722969e-08 | 2.2351741791e-07 | 6.6485383189e-06 | 1.0000000000 | 144.11 |
| host_FI_vs_golden_SOCS | ✅ | 2.0976206824e-08 | 2.6822090149e-07 | 3.0535266293e-05 | 1.0000000000 | 144.08 |
| host_FI_vs_TCC_direct | ✅ | 3.4322223061e-03 | 6.2465071678e-03 | 9.7330028304e-01 | 0.9999499864 | 39.86 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T10/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T10/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T10/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T10/timing.csv`
- report: `experiments/runs/E1_multi_mask/T10/full_platform_report.md`
