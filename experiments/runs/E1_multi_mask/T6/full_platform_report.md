# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T6_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T6`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000069 | `{}` |
| load_golden_data | PASS | 0.004278 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005639 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 709.3385845226306}` |
| pcie_h2c_write_mskf_i | PASS | 0.006464 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 618.8140831352299}` |
| pcie_h2c_write_scales | PASS | 0.000132 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.2895076396591584}` |
| pcie_h2c_write_krn_r | PASS | 0.000127 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 86.92324682001691}` |
| pcie_h2c_write_krn_i | PASS | 0.000138 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 79.64970772530384}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000207 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000198 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001286 | `{}` |
| fpga_compute | PASS | 0.020417 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000200 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000237 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T6/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.034198 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002010 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T6/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000723 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000880 | `{}` |
| compare_against_golden | PASS | 0.085754 | `{}` |

Total measured time: `0.162959s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 3.5407818936e-08 | 3.2782554626e-07 | 2.0078729756e-06 | 1.0000000000 | 145.14 |
| host_FI_vs_golden_SOCS | ✅ | 3.5809791160e-08 | 3.5762786865e-07 | 3.5029740249e-06 | 1.0000000000 | 145.05 |
| host_FI_vs_TCC_direct | ✅ | 6.2157981328e-03 | 1.0165255517e-02 | 7.5972805887e-01 | 0.9999689082 | 40.31 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T6/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T6/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T6/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T6/timing.csv`
- report: `experiments/runs/E1_multi_mask/T6/full_platform_report.md`
