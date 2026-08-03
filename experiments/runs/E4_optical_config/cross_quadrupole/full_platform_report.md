# 全平台 PCIe 板级验证报告

- Config: `experiments/config/E4_optical_config/cross_quadrupole.json`
- Golden output dir: `experiments/data/E4_optical_config/golden/cross_quadrupole`
- Lx/Ly: 1024×1024
- Nx/Ny: 7×7
- kernels: 10 (15×15)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000071 | `{}` |
| load_golden_data | PASS | 0.004235 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005528 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 723.642410369921}` |
| pcie_h2c_write_mskf_i | PASS | 0.006426 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 622.5042438380282}` |
| pcie_h2c_write_scales | PASS | 0.000106 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.3585983251850606}` |
| pcie_h2c_write_krn_r | PASS | 0.000108 | `{"address": "0x40880000", "bytes": 9000, "mib_per_second": 79.47285956654022}` |
| pcie_h2c_write_krn_i | PASS | 0.000225 | `{"address": "0x40900000", "bytes": 9000, "mib_per_second": 38.17989143447822}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000198 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000207 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.000979 | `{}` |
| fpga_compute | PASS | 0.020502 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000260 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000641 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/cross_quadrupole/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.034892 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.003058 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/cross_quadrupole/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000959 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000925 | `{}` |
| compare_against_golden | PASS | 0.084485 | `{}` |

Total measured time: `0.163805s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 4.0129175445e-08 | 3.5762786865e-07 | 2.3633847027e-06 | 1.0000000000 | 141.42 |
| host_FI_vs_golden_SOCS | ✅ | 4.0445949066e-08 | 4.1723251343e-07 | 9.9255010358e-06 | 1.0000000000 | 141.36 |
| host_FI_vs_TCC_direct | ✅ | 1.6950235626e-03 | 4.5669823885e-03 | 7.5278438281e-01 | 0.9999864306 | 48.93 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E4_optical_config/cross_quadrupole/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E4_optical_config/cross_quadrupole/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E4_optical_config/cross_quadrupole/metrics.csv`
- timing_csv: `experiments/runs/E4_optical_config/cross_quadrupole/timing.csv`
- report: `experiments/runs/E4_optical_config/cross_quadrupole/full_platform_report.md`
