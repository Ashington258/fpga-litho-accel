# 全平台 PCIe 板级验证报告

- Config: `experiments/config/E4_optical_config/annular_baseline.json`
- Golden output dir: `experiments/data/E4_optical_config/golden/annular_baseline`
- Lx/Ly: 1024×1024
- Nx/Ny: 8×8
- kernels: 10 (17×17)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000064 | `{}` |
| load_golden_data | PASS | 0.004049 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005390 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 742.1464201670624}` |
| pcie_h2c_write_mskf_i | PASS | 0.006080 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 657.8812113072036}` |
| pcie_h2c_write_scales | PASS | 0.000180 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.21169476622677724}` |
| pcie_h2c_write_krn_r | PASS | 0.000122 | `{"address": "0x40880000", "bytes": 11560, "mib_per_second": 90.57688478378934}` |
| pcie_h2c_write_krn_i | PASS | 0.000209 | `{"address": "0x40900000", "bytes": 11560, "mib_per_second": 52.70657092379552}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000205 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000182 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001593 | `{}` |
| fpga_compute | PASS | 0.020414 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000172 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000651 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/annular_baseline/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.033664 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002899 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/annular_baseline/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000889 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000783 | `{}` |
| compare_against_golden | PASS | 0.086463 | `{}` |

Total measured time: `0.164008s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 2.9303560514e-08 | 3.5762786865e-07 | 2.6540381633e-06 | 1.0000000000 | 142.26 |
| host_FI_vs_golden_SOCS | ✅ | 2.9528027459e-08 | 4.1723251343e-07 | 8.6624720582e-06 | 1.0000000000 | 142.20 |
| host_FI_vs_TCC_direct | ✅ | 5.4737755323e-03 | 1.0465636849e-02 | 9.4251801039e-01 | 0.9999560990 | 36.95 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E4_optical_config/annular_baseline/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E4_optical_config/annular_baseline/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E4_optical_config/annular_baseline/metrics.csv`
- timing_csv: `experiments/runs/E4_optical_config/annular_baseline/timing.csv`
- report: `experiments/runs/E4_optical_config/annular_baseline/full_platform_report.md`
