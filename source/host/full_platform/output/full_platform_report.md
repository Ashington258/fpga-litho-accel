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
| load_json_config | PASS | 0.000084 | `{}` |
| load_golden_data | PASS | 0.004019 | `{}` |
| load_reused_fpga_tmpimgp | PASS | 0.000066 | `{"path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_tmpimgp_full_128.bin"}` |
| save_fpga_tmpimgp | PASS | 0.000211 | `{"bytes": 65536, "path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_tmpimgp_full_128.bin"}` |
| host_fi_inverse_aerial | PASS | 0.031004 | `{"input_shape": [128, 128], "output_shape": [1024, 1024]}` |
| save_host_fi_aerial | PASS | 0.002949 | `{"bytes": 4194304, "path": "/root/project/fpga-litho-accel/source/host/full_platform/output/fpga_aerial_fi.bin"}` |
| load_golden_socs_aerial | PASS | 0.000843 | `{}` |
| load_golden_tcc_aerial | PASS | 0.000894 | `{}` |
| compare_against_golden | PASS | 0.086230 | `{}` |

Total measured time: `0.126301s`

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
