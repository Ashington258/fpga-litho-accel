# 全平台 PCIe 板级验证报告

- Config: `input/config/golden_1024.json`
- Golden output dir: `output/verification`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000079 | `{}` |
| load_golden_data | PASS | 0.003965 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005846 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 684.2019644818095}` |
| pcie_h2c_write_mskf_i | PASS | 0.006412 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 623.7986027093111}` |
| pcie_h2c_write_scales | PASS | 0.000071 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.5373191169563368}` |
| pcie_h2c_write_krn_r | PASS | 0.000066 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 166.02625774677165}` |
| pcie_h2c_write_krn_i | PASS | 0.000050 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 219.0916874150766}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000118 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000080 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.000772 | `{}` |
| fpga_compute | PASS | 0.020726 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000359 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000563 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.033450 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.003018 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000918 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000923 | `{}` |
| compare_against_golden | PASS | 0.086550 | `{}` |

Total measured time: `0.163968s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.9303560514e-08 | 3.5762786865e-07 | 2.6540381633e-06 | 1.0000000000 | 142.26 |
| host_FI_vs_golden_SOCS | ✅ | 2.9528027459e-08 | 4.1723251343e-07 | 8.6624720582e-06 | 1.0000000000 | 142.20 |
| host_FI_vs_TCC_direct | ✅ | 5.4737755323e-03 | 1.0465636849e-02 | 9.4251801039e-01 | 0.9999560990 | 36.95 |

## 输出文件

- fpga_tmpimgp: `source/host/full_platform/output/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `source/host/full_platform/output/fpga_aerial_fi.bin`
- metrics_csv: `source/host/full_platform/output/metrics.csv`
- timing_csv: `source/host/full_platform/output/timing.csv`
- report: `source/host/full_platform/output/full_platform_report.md`
- tmpimgp_visual: `source/host/full_platform/output/tmpimgp_comparison.png`
- aerial_visual: `source/host/full_platform/output/aerial_comparison.png`
