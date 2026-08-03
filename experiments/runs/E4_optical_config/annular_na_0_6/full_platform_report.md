# 全平台 PCIe 板级验证报告

- Config: `experiments/config/E4_optical_config/annular_na_0_6.json`
- Golden output dir: `experiments/data/E4_optical_config/golden/annular_na_0_6`
- Lx/Ly: 1024×1024
- Nx/Ny: 6×6
- kernels: 10 (13×13)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000066 | `{}` |
| load_golden_data | PASS | 0.004957 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005291 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 756.0301917213156}` |
| pcie_h2c_write_mskf_i | PASS | 0.005022 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 796.5379272018282}` |
| pcie_h2c_write_scales | PASS | 0.000043 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.8890825858430326}` |
| pcie_h2c_write_krn_r | PASS | 0.000036 | `{"address": "0x40880000", "bytes": 6760, "mib_per_second": 178.316044699825}` |
| pcie_h2c_write_krn_i | PASS | 0.000040 | `{"address": "0x40900000", "bytes": 6760, "mib_per_second": 159.6344815359847}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000119 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000086 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.000698 | `{}` |
| fpga_compute | PASS | 0.020623 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000260 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000620 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/annular_na_0_6/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.033374 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002931 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/annular_na_0_6/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000733 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000900 | `{}` |
| compare_against_golden | PASS | 0.086369 | `{}` |

Total measured time: `0.162169s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 1.9346394125e-08 | 1.3411045074e-07 | 1.2204888839e-06 | 1.0000000000 | 143.07 |
| host_FI_vs_golden_SOCS | ✅ | 1.9540976939e-08 | 1.7881393433e-07 | 2.3690621333e-06 | 1.0000000000 | 142.98 |
| host_FI_vs_TCC_direct | ✅ | 4.1902554350e-03 | 7.1894377470e-03 | 8.1384945138e-01 | 0.9999558590 | 36.45 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E4_optical_config/annular_na_0_6/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E4_optical_config/annular_na_0_6/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E4_optical_config/annular_na_0_6/metrics.csv`
- timing_csv: `experiments/runs/E4_optical_config/annular_na_0_6/timing.csv`
- report: `experiments/runs/E4_optical_config/annular_na_0_6/full_platform_report.md`
