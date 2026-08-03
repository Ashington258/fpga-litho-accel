# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T3_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T3`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000066 | `{}` |
| load_golden_data | PASS | 0.004185 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005669 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 705.605755548402}` |
| pcie_h2c_write_mskf_i | PASS | 0.010206 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 391.94409855298943}` |
| pcie_h2c_write_scales | PASS | 0.000106 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.3585578845216121}` |
| pcie_h2c_write_krn_r | PASS | 0.000115 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 95.94676194184515}` |
| pcie_h2c_write_krn_i | PASS | 0.000095 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 115.90313685872299}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000177 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000154 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001322 | `{}` |
| fpga_compute | PASS | 0.020523 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000290 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000500 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T3/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.037230 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002009 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T3/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000840 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000867 | `{}` |
| compare_against_golden | PASS | 0.084675 | `{}` |

Total measured time: `0.169031s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.7490688510e-08 | 2.5331974030e-07 | 2.7229501531e-06 | 1.0000000000 | 143.41 |
| host_FI_vs_golden_SOCS | ✅ | 2.7715328076e-08 | 2.8312206268e-07 | 6.2021986536e-06 | 1.0000000000 | 143.35 |
| host_FI_vs_TCC_direct | ✅ | 4.8490692451e-03 | 7.9291462898e-03 | 9.2826170566e-01 | 0.9999616057 | 38.60 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T3/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T3/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T3/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T3/timing.csv`
- report: `experiments/runs/E1_multi_mask/T3/full_platform_report.md`
