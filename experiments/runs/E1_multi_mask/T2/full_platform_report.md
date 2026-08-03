# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T2_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T2`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000081 | `{}` |
| load_golden_data | PASS | 0.004003 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005709 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 700.6801334065061}` |
| pcie_h2c_write_mskf_i | PASS | 0.006521 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 613.3784904493131}` |
| pcie_h2c_write_scales | PASS | 0.000076 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.5030989319722323}` |
| pcie_h2c_write_krn_r | PASS | 0.000096 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 115.13571333858407}` |
| pcie_h2c_write_krn_i | PASS | 0.000066 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 166.7999414347818}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000160 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000148 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001142 | `{}` |
| fpga_compute | PASS | 0.020257 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000143 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000152 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T2/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.031789 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002007 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T2/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000865 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000856 | `{}` |
| compare_against_golden | PASS | 0.084614 | `{}` |

Total measured time: `0.158684s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.1413237371e-08 | 1.7881393433e-07 | 6.3230199024e-06 | 1.0000000000 | 144.53 |
| host_FI_vs_golden_SOCS | ✅ | 2.1655738413e-08 | 2.0861625671e-07 | 2.6056951991e-05 | 1.0000000000 | 144.44 |
| host_FI_vs_TCC_direct | ✅ | 4.5231786656e-03 | 7.3056081310e-03 | 9.7738113580e-01 | 0.9999521530 | 38.14 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T2/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T2/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T2/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T2/timing.csv`
- report: `experiments/runs/E1_multi_mask/T2/full_platform_report.md`
