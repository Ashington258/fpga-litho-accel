# 全平台 PCIe 板级验证报告

- Config: `experiments/config/multi_mask/config_T1_1024_nk10.json`
- Golden output dir: `experiments/data/E1_multi_mask/golden/T1`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000083 | `{}` |
| load_golden_data | PASS | 0.004184 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005696 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 702.2319040242478}` |
| pcie_h2c_write_mskf_i | PASS | 0.008423 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 474.91696356963325}` |
| pcie_h2c_write_scales | PASS | 0.000185 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.20641184266530047}` |
| pcie_h2c_write_krn_r | PASS | 0.000142 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 77.86746091331204}` |
| pcie_h2c_write_krn_i | PASS | 0.000172 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 64.08684454352654}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000211 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000166 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001078 | `{}` |
| fpga_compute | PASS | 0.020512 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000289 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000438 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T1/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.031741 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002015 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E1_multi_mask/T1/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000836 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000817 | `{}` |
| compare_against_golden | PASS | 0.084516 | `{}` |

Total measured time: `0.161503s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.9303560514e-08 | 3.5762786865e-07 | 2.6540381633e-06 | 1.0000000000 | 142.26 |
| host_FI_vs_golden_SOCS | ✅ | 2.9528027459e-08 | 4.1723251343e-07 | 8.6624720582e-06 | 1.0000000000 | 142.20 |
| host_FI_vs_TCC_direct | ✅ | 5.4737755323e-03 | 1.0465636849e-02 | 9.4251801039e-01 | 0.9999560990 | 36.95 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E1_multi_mask/T1/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E1_multi_mask/T1/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E1_multi_mask/T1/metrics.csv`
- timing_csv: `experiments/runs/E1_multi_mask/T1/timing.csv`
- report: `experiments/runs/E1_multi_mask/T1/full_platform_report.md`
