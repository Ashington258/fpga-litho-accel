# 全平台 PCIe 板级验证报告

- Config: `experiments/config/E4_optical_config/dipole_x.json`
- Golden output dir: `experiments/data/E4_optical_config/golden/dipole_x`
- Lx/Ly: 1024×1024
- Nx/Ny: 7×7
- kernels: 10 (15×15)
- FPGA tmpImgp: 128×128
- Host FI output: 1024×1024

## 步骤耗时

| Step | Status | Time (s) | Details |
| --- | --- | ---: | --- |
| load_json_config | PASS | 0.000066 | `{}` |
| load_golden_data | PASS | 0.003956 | `{}` |
| pcie_h2c_write_mskf_r | PASS | 0.005409 | `{"address": "0x40000000", "bytes": 4194304, "mib_per_second": 739.5225819525584}` |
| pcie_h2c_write_mskf_i | PASS | 0.006159 | `{"address": "0x40400000", "bytes": 4194304, "mib_per_second": 649.4845527577817}` |
| pcie_h2c_write_scales | PASS | 0.000149 | `{"address": "0x40800000", "bytes": 40, "mib_per_second": 0.2565675178283761}` |
| pcie_h2c_write_krn_r | PASS | 0.000119 | `{"address": "0x40880000", "bytes": 9000, "mib_per_second": 72.11329763149}` |
| pcie_h2c_write_krn_i | PASS | 0.000134 | `{"address": "0x40900000", "bytes": 9000, "mib_per_second": 63.92014490144067}` |
| pcie_h2c_clear_tmpImg_ddr | PASS | 0.000156 | `{"address": "0x40980000", "bytes": 65536}` |
| pcie_h2c_clear_output | PASS | 0.000179 | `{"address": "0x40990000", "bytes": 65536}` |
| hls_configure_axilite | PASS | 0.001244 | `{}` |
| fpga_compute | PASS | 0.020675 | `{"ap_ctrl": "0x0000000e"}` |
| pcie_c2h_read_tmpimgp | PASS | 0.000295 | `{"bytes": 65536}` |
| save_fpga_tmpimgp | PASS | 0.000546 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/dipole_x/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.034623 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002966 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/experiments/runs/E4_optical_config/dipole_x/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000826 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000954 | `{}` |
| compare_against_golden | PASS | 0.085330 | `{}` |

Total measured time: `0.163786s`

## 对比结果

| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| tmpImgp_vs_golden | ✅ | 3.7395091218e-08 | 3.8743019104e-07 | 2.9813315540e-06 | 1.0000000000 | 140.83 |
| host_FI_vs_golden_SOCS | ✅ | 3.7654925251e-08 | 3.8743019104e-07 | 7.6490967147e-06 | 1.0000000000 | 140.77 |
| host_FI_vs_TCC_direct | ✅ | 1.0574377742e-03 | 2.1922886372e-03 | 7.6143124468e-01 | 0.9999979909 | 51.83 |

## 输出文件

- fpga_tmpimgp: `experiments/runs/E4_optical_config/dipole_x/fpga_tmpimgp_full_128.bin`
- host_fi_aerial: `experiments/runs/E4_optical_config/dipole_x/fpga_aerial_fi.bin`
- metrics_csv: `experiments/runs/E4_optical_config/dipole_x/metrics.csv`
- timing_csv: `experiments/runs/E4_optical_config/dipole_x/timing.csv`
- report: `experiments/runs/E4_optical_config/dipole_x/full_platform_report.md`
