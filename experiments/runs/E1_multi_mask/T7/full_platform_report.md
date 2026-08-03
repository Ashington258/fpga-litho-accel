# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T7_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T7`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000064 | `{}` |
| load_golden_data | PASS | 0.004090 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005772 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 692.9777611953955}` |
| pcie_h2c_write_mskf_i | PASS | 0.008375 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 477.6129097257405}` |
| pcie_h2c_write_scales | PASS | 0.000147 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.2593973324359026}` |
| pcie_h2c_write_krn_r | PASS | 0.000125 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 88.18028023569721}` |
| pcie_h2c_write_krn_i | PASS | 0.000122 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 90.44164601048668}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000218 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000202 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001108 | `{}` |
| fpga_compute | PASS | 0.020417 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000259 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000566 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T7/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.034461 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002083 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T7/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000731 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000874 | `{}` |
| compare_against_golden | PASS | 0.085448 | `{}` |

Total measured time: `0.165062s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.5917962604e-08 | 1.7881393433e-07 | 3.3026550187e-06 | 1.0000000000 | 143.62 |
| host_FI_vs_golden_SOCS | ✅ | 2.6207797170e-08 | 1.9371509552e-07 | 6.4335812667e-06 | 1.0000000000 | 143.53 |
| host_FI_vs_TCC_direct | ✅ | 5.0280611164e-03 | 8.1042507663e-03 | 8.8091832673e-01 | 0.9999758802 | 37.97 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T7/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T7/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T7/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T7/timing.csv`
- report: `experiments/runs/E1_multi_mask/T7/full_platform_report.md`
