# SOCS Host Preprocessing

This directory contains the C++ host-side preprocessing tool for the SOCS HLS flow.

## Build

```bash
cd /root/project/FPGA-Litho/source/host
make
```

## Run

```bash
cd /root/project/FPGA-Litho/source/host
./socs_host ../../input/config/config.json ../../output/reference/kernels output/host_output
```

## Output

- `output/host_output/mskf_r.bin`
- `output/host_output/mskf_i.bin`
- `output/host_output/scales.bin`
- `output/host_output/kernels/` (copied kernel files and kernel_info.txt)

## Validation

Use `python run_reference_test.py` from the project root to generate the reference kernel directory. Then run `socs_host` with the generated `output/reference/kernels` directory.
