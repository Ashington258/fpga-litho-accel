# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T8_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T8`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000085 | `{}` |
| load_golden_data | PASS | 0.004304 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005296 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 755.2499318259341}` |
| pcie_h2c_write_mskf_i | PASS | 0.006420 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 623.0505334787849}` |
| pcie_h2c_write_scales | PASS | 0.000144 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.2644870911404129}` |
| pcie_h2c_write_krn_r | PASS | 0.000196 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 56.27919465520257}` |
| pcie_h2c_write_krn_i | PASS | 0.000146 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 75.70039179847646}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000173 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000128 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001127 | `{}` |
| fpga_compute | PASS | 0.020458 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000303 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000455 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T8/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.034496 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002022 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T8/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000726 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000903 | `{}` |
| compare_against_golden | PASS | 0.090921 | `{}` |

Total measured time: `0.168303s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.0106136079e-08 | 2.0861625671e-07 | 2.9638127582e-06 | 1.0000000000 | 146.58 |
| host_FI_vs_golden_SOCS | ✅ | 2.0398408012e-08 | 2.3841857910e-07 | 1.8035124694e-05 | 1.0000000000 | 146.45 |
| host_FI_vs_TCC_direct | ✅ | 3.5033675417e-03 | 5.8210194111e-03 | 9.6290271767e-01 | 0.9999847924 | 41.83 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T8/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T8/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T8/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T8/timing.csv`
- report: `experiments/runs/E1_multi_mask/T8/full_platform_report.md`
