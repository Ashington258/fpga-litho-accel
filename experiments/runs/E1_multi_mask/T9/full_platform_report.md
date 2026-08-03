# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T9_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T9`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000069 | `{}` |
| load_golden_data | PASS | 0.004034 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005641 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 709.0863384791058}` |
| pcie_h2c_write_mskf_i | PASS | 0.008603 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 464.97073222924803}` |
| pcie_h2c_write_scales | PASS | 0.000092 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.4125870334218885}` |
| pcie_h2c_write_krn_r | PASS | 0.000137 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 80.45182808219487}` |
| pcie_h2c_write_krn_i | PASS | 0.000152 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 72.71505483290977}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000190 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000162 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001381 | `{}` |
| fpga_compute | PASS | 0.020343 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000171 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000187 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T9/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.032308 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.001999 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T9/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000862 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000848 | `{}` |
| compare_against_golden | PASS | 0.084786 | `{}` |

Total measured time: `0.161964s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 3.3674582862e-08 | 2.2351741791e-07 | 2.8390209231e-06 | 1.0000000000 | 142.35 |
| host_FI_vs_golden_SOCS | ✅ | 3.4104662409e-08 | 2.5331974030e-07 | 4.2745765286e-06 | 1.0000000000 | 142.25 |
| host_FI_vs_TCC_direct | ✅ | 6.1028285961e-03 | 1.0641049594e-02 | 8.0416458157e-01 | 0.9999519078 | 37.29 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T9/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T9/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T9/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T9/timing.csv`
- report: `experiments/runs/E1_multi_mask/T9/full_platform_report.md`
