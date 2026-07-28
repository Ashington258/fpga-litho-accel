# Linux 主机 SCI 出图交接手册

## 1. 交接目标

本手册用于在连接 FPGA 板卡的 Linux 主机上统一完成以下工作：

1. 使用论文固定配置生成 CPU Golden 原始数据。
2. 通过 PCIe XDMA 运行 FPGA 在线 SOCS 重构并回读结果。
3. 在 Host 侧执行 Fourier Interpolation（FI）。
4. 只从配置 JSON 和原始 float32 BIN 生成 I01 至 I10 SCI 流程图素材。
5. 输出数据来源、数值范围、校验指标、环境信息和 SHA-256 清单。
6. 将素材包回传至 Windows 工作站进行 Draw.io 排版。

Linux 是该组论文图数据的唯一生成平台。Windows 只负责最终裁剪、10 核拼版和 Draw.io 组合，不负责重新计算或修改数值结果。

## 2. 不可变约束

### 2.1 唯一论文配置

必须使用 [golden_1024.json](../../../input/config/golden_1024.json)：

| 参数 | 固定值 |
| --- | ---: |
| 掩模和最终图像 | 1024×1024 |
| NA | 0.8 |
| 波长 | 193 nm |
| 离焦 | 0.2 |
| 光源 | Annular，inner=0.6，outer=0.9 |
| $N_x,N_y$ | 8, 8 |
| SOCS kernel | 17×17 |
| kernel 数量 | 10 |
| 在线 FFT 网格 | 128×128 |

计算关系为：

$$
N_x=N_y=\left\lfloor\frac{NA\cdot L\cdot(1+\sigma_{out})}{\lambda}\right\rfloor=8.
$$

### 2.2 禁止事项

- 禁止使用 `2048×2048 / 33×33` 数据替代论文的 1024 配置。
- 禁止从已有 PNG、论文图或比较图中裁剪出数值素材。
- 禁止使用人工高斯核、手绘核或示意数据替代 SOCS 复数核。
- 禁止对 CPU 与 FPGA 输出分别执行独立最大值归一化。
- 禁止将 10 核 SOCS 相对完整 TCC 的截断误差描述为 FPGA 数值误差。
- 禁止在原始数据不完整时继续生成“看起来正确”的替代图片。
- 禁止使用 Windows 原型脚本 `generate_concise_assets.ps1` 作为最终投稿数据来源。

如果任何原始数据缺失，流程必须停止并报告缺失文件。

## 3. Linux 主机职责

目标主机同时承担以下角色：

- CPU Golden 数据生成；
- FPGA/XDMA 数据写入、控制和回读；
- Host FI；
- I01 至 I10 数值素材生成；
- 精度验证和数据归档。

参考验证平台为 Ubuntu 24.04 LTS、Linux 6.8、GCC 13.3.0。FPGA PCIe endpoint 为 Xilinx `10ee:9038`，当前验证链路目标为 PCIe Gen3 x8。

## 4. 接收仓库并固定版本

在 Linux 主机上进入独立工作目录：

```bash
git clone https://github.com/Ashington258/fpga-litho-accel.git
cd fpga-litho-accel
git checkout feature/2048-optimization
git pull --ff-only origin feature/2048-optimization
```

虽然分支名称包含 `2048-optimization`，本次论文出图仍只允许使用 `golden_1024.json`。

记录运行基线：

```bash
mkdir -p output/figure_data/logs

git rev-parse HEAD | tee output/figure_data/logs/git_sha.txt
git status --short --branch | tee output/figure_data/logs/git_status.txt
git submodule status 2>/dev/null | tee output/figure_data/logs/git_submodules.txt || true
```

工作树如果存在与出图相关的未提交修改，应先停止并由项目负责人确认。不要使用 `git reset --hard` 或其他命令清理未知改动。

## 5. 安装系统依赖

### 5.1 apt 软件包

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  g++ \
  make \
  libfftw3-dev \
  liblapack-dev \
  libblas-dev \
  libgomp1 \
  pkg-config \
  linux-headers-$(uname -r) \
  python3 \
  python3-venv \
  python3-pip \
  pciutils \
  jq \
  file \
  zstd
```

### 5.2 Python 虚拟环境

```bash
python3 -m venv .venv-figure
source .venv-figure/bin/activate
python -m pip install --upgrade pip wheel setuptools
python -m pip install \
  'numpy>=1.26,<3' \
  'matplotlib>=3.8,<4' \
  'pillow>=10,<12'
```

记录版本：

```bash
python --version | tee output/figure_data/logs/python_version.txt
python -m pip freeze | tee output/figure_data/logs/pip_freeze.txt
gcc --version | head -1 | tee output/figure_data/logs/gcc_version.txt
g++ --version | head -1 | tee output/figure_data/logs/gxx_version.txt
ldconfig -p | grep -E 'fftw3|lapack|blas' \
  | tee output/figure_data/logs/numerical_libraries.txt
```

### 5.3 可复现运行变量

```bash
export MPLBACKEND=Agg
export PYTHONHASHSEED=0
export OMP_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1
```

出图脚本不得依赖 X11、Wayland 或交互式 Matplotlib 后端。

## 6. 记录主机和 PCIe 环境

```bash
{
  echo '=== hostname ==='
  hostnamectl
  echo '=== kernel ==='
  uname -a
  echo '=== cpu ==='
  lscpu
  echo '=== memory ==='
  free -h
  echo '=== board ==='
  cat /sys/class/dmi/id/board_vendor 2>/dev/null || true
  cat /sys/class/dmi/id/board_name 2>/dev/null || true
} | tee output/figure_data/logs/platform_info.txt

lspci -nn -d 10ee:9038 \
  | tee output/figure_data/logs/lspci_endpoint.txt

sudo lspci -vv -s 01:00.0 \
  | tee output/figure_data/logs/lspci_verbose.txt

sudo dmesg | grep -iE 'xdma|xilinx|10ee' \
  | tee output/figure_data/logs/dmesg_xdma.txt

ls -l /dev/xdma* \
  | tee output/figure_data/logs/xdma_devices.txt
```

若 FPGA BDF 不是 `01:00.0`，使用 `lspci -nn -d 10ee:9038` 返回的实际地址替换。

必须确认：

- endpoint 为 `10ee:9038`；
- `LnkSta` 为 `Speed 8GT/s, Width x8`；
- `/dev/xdma0_h2c_0` 存在；
- `/dev/xdma0_c2h_0` 存在；
- `/dev/xdma0_user` 存在，或当前 bitstream 支持通过 M_AXI 访问 HLS AXI-Lite；
- dmesg 中无持续 DMA、AER 或 lane 错误。

如果链路降级为 x2，应先检查插槽接触和 PCIe lane 状态，不要直接采集性能数据。

## 7. 创建专用输出目录

不要直接覆盖 `output/verification/` 或已提交的 full-platform 结果。

```bash
RUN_TAG="$(date -u +%Y%m%dT%H%M%SZ)_$(git rev-parse --short HEAD)"
export RUN_TAG

mkdir -p \
  output/figure_data/golden_1024 \
  output/figure_data/full_platform_1024 \
  output/figure_data/logs \
  "doc/小论文/流程图/assets_concise/1024_17x17_10kernels"

printf '%s\n' "$RUN_TAG" \
  | tee output/figure_data/logs/run_tag.txt
```

建议每次正式出图前清空专用目录，而不是覆盖后混用旧数据：

```bash
rm -rf output/figure_data/golden_1024
rm -rf output/figure_data/full_platform_1024
rm -rf "doc/小论文/流程图/assets_concise/1024_17x17_10kernels"

mkdir -p \
  output/figure_data/golden_1024 \
  output/figure_data/full_platform_1024 \
  "doc/小论文/流程图/assets_concise/1024_17x17_10kernels"
```

只允许删除上述专用出图目录。

## 8. 配置预检

### 8.1 检查 JSON

```bash
jq '{
  Lx: .mask.period.Lx,
  Ly: .mask.period.Ly,
  maskSizeX: .mask.size.maskSizeX,
  maskSizeY: .mask.size.maskSizeY,
  source: .source.type,
  sigmaInner: .source.annular.innerRadius,
  sigmaOuter: .source.annular.outerRadius,
  NA: .optics.NA,
  wavelength: .optics.wavelength,
  defocus: .optics.defocus,
  kernels: .kernel.count
}' input/config/golden_1024.json \
  | tee output/figure_data/logs/config_summary.json
```

预期：

```json
{
  "Lx": 1024,
  "Ly": 1024,
  "maskSizeX": 1024,
  "maskSizeY": 1024,
  "source": "Annular",
  "sigmaInner": 0.6,
  "sigmaOuter": 0.9,
  "NA": 0.8,
  "wavelength": 193,
  "defocus": 0.2,
  "kernels": 10
}
```

### 8.2 检查输入掩模

```bash
stat -c '%n %s bytes' input/mask/1024x1024.bin \
  | tee output/figure_data/logs/mask_size.txt
```

预期大小：

```text
4194304 bytes
```

即 $1024\times1024\times4$ bytes，float32 row-major。

## 9. 编译并生成 CPU Golden

### 9.1 单独检查编译

```bash
source .venv-figure/bin/activate

set -o pipefail
python validation/golden/run_verification.py \
  --config input/config/golden_1024.json \
  --output output/figure_data/golden_1024 \
  --compile-only \
  2>&1 | tee output/figure_data/logs/golden_compile.log
```

如果编译失败，优先检查：

```bash
pkg-config --modversion fftw3 2>/dev/null || true
ldconfig -p | grep fftw3
ldconfig -p | grep -E 'lapack|blas'
```

### 9.2 生成完整 Golden

```bash
set -o pipefail
python validation/golden/run_verification.py \
  --config input/config/golden_1024.json \
  --output output/figure_data/golden_1024 \
  2>&1 | tee output/figure_data/logs/golden_generation.log
```

不要添加 `--no-clean`。正式出图要求每次从干净的专用目录生成。

### 9.3 Golden 必需文件

```text
output/figure_data/golden_1024/
├── fft_meta.txt
├── mskf_r.bin
├── mskf_i.bin
├── scales.bin
├── tmpImgp_full_128.bin
├── aerial_image_socs_kernel.bin
├── aerial_image_tcc_direct.bin
└── kernels/
    ├── kernel_info.txt
    ├── krn_0_r.bin
    ├── krn_0_i.bin
    ├── ...
    ├── krn_9_r.bin
    └── krn_9_i.bin
```

### 9.4 Golden 字节数

| 文件 | shape | float 数量 | 预期字节数 |
| --- | ---: | ---: | ---: |
| `mskf_r.bin` | 1024×1024 | 1,048,576 | 4,194,304 |
| `mskf_i.bin` | 1024×1024 | 1,048,576 | 4,194,304 |
| `scales.bin` | 10 | 10 | 40 |
| 每个 `krn_k_r.bin` | 17×17 | 289 | 1,156 |
| 每个 `krn_k_i.bin` | 17×17 | 289 | 1,156 |
| `tmpImgp_full_128.bin` | 128×128 | 16,384 | 65,536 |
| `aerial_image_socs_kernel.bin` | 1024×1024 | 1,048,576 | 4,194,304 |
| `aerial_image_tcc_direct.bin` | 1024×1024 | 1,048,576 | 4,194,304 |

检查命令：

```bash
find output/figure_data/golden_1024 \
  -type f \( -name '*.bin' -o -name '*.txt' \) \
  -printf '%p %s bytes\n' \
  | sort \
  | tee output/figure_data/logs/golden_files.txt
```

必须检查 `kernel_info.txt`：

```bash
cat output/figure_data/golden_1024/kernels/kernel_info.txt \
  | tee output/figure_data/logs/kernel_info.txt
```

预期包含：

```text
Kernel Size: 17x17
Number of Kernels: 10
```

### 9.5 128×128 元数据注意事项

部分历史 `fft_meta.txt` 可能仍将数学卷积最小尺寸写为 64×64，但当前 V18 FPGA 在线网格和 `tmpImgp_full_128.bin` 是固定 128×128。

对板上验证和论文出图，BIN 长度是最终权威：

```text
16384 float32 = 65536 bytes = 128×128
```

如果 `tmpImgp_full_128.bin` 不存在或不是 65,536 bytes，必须停止。

## 10. XDMA 全平台预检

### 10.1 Dry Run

```bash
source .venv-figure/bin/activate

set -o pipefail
bash source/host/full_platform/run.sh \
  --config input/config/golden_1024.json \
  --golden-output output/figure_data/golden_1024 \
  --output-dir output/figure_data/full_platform_1024 \
  --dry-run \
  2>&1 | tee output/figure_data/logs/full_platform_dry_run.log
```

必须看到：

```text
[DRY-RUN] Full platform inputs and layout checks passed.
```

Dry Run 失败时不要执行上板。

### 10.2 可选 DMA-only 检查

如果需要先隔离 DMA 与 HLS 控制问题：

```bash
VENV_PYTHON="$(realpath .venv-figure/bin/python)"

set -o pipefail
sudo -E env PYTHON="$VENV_PYTHON" \
  bash validation/board/pcie/run.sh \
  --config input/config/golden_1024.json \
  --golden-output output/figure_data/golden_1024 \
  --dma-only \
  2>&1 | tee output/figure_data/logs/pcie_dma_only.log
```

DMA-only 只证明 H2C/C2H DDR 数据通路，不证明 HLS IP 能通过 AXI-Lite 启动。

## 11. 完整 FPGA + Host FI 运行

为避免 `sudo` 丢失虚拟环境，显式传入解释器绝对路径：

```bash
VENV_PYTHON="$(realpath .venv-figure/bin/python)"

set -o pipefail
sudo -E env PYTHON="$VENV_PYTHON" \
  bash source/host/full_platform/run.sh \
  --config input/config/golden_1024.json \
  --golden-output output/figure_data/golden_1024 \
  --output-dir output/figure_data/full_platform_1024 \
  --no-visualize \
  2>&1 | tee output/figure_data/logs/full_platform_run.log
```

使用 `--no-visualize` 是为了确保最终论文图只由后续统一 SCI 脚本生成，而不是使用 full-platform 自带的调试比较图。

## 12. FPGA 结果验收

必需文件：

```text
output/figure_data/full_platform_1024/
├── fpga_tmpimgp_full_128.bin
├── fpga_aerial_fi.bin
├── metrics.csv
├── timing.csv
├── summary.json
└── full_platform_report.md
```

字节数：

```bash
stat -c '%n %s bytes' \
  output/figure_data/full_platform_1024/fpga_tmpimgp_full_128.bin \
  output/figure_data/full_platform_1024/fpga_aerial_fi.bin \
  | tee output/figure_data/logs/fpga_output_sizes.txt
```

预期：

| 文件 | 预期字节数 |
| --- | ---: |
| `fpga_tmpimgp_full_128.bin` | 65,536 |
| `fpga_aerial_fi.bin` | 4,194,304 |

读取关键指标：

```bash
jq '.metrics' output/figure_data/full_platform_1024/summary.json \
  | tee output/figure_data/logs/full_platform_metrics.json
```

关键验收条件：

```bash
jq -e '
  .metrics.tmpImgp_vs_golden.passed == true and
  .metrics.host_FI_vs_golden_SOCS.passed == true and
  .metrics.tmpImgp_vs_golden.rmse < 1e-5 and
  .metrics.host_FI_vs_golden_SOCS.rmse < 1e-5
' output/figure_data/full_platform_1024/summary.json
```

历史通过值可用于合理性检查，但不是硬编码判定值：

| 对比 | 历史 RMSE |
| --- | ---: |
| FPGA tmpImgp vs CPU Golden | $2.930356\times10^{-8}$ |
| FPGA+Host FI vs CPU SOCS | $2.952803\times10^{-8}$ |
| FPGA+Host FI vs完整 TCC | $5.473776\times10^{-3}$ |

第三项主要包含 10 核 SOCS 低秩截断误差，不用于判断 FPGA 数值实现是否正确。

## 13. Linux 统一 SCI 出图脚本契约

### 13.1 权威入口

最终 Linux 入口约定为：

```text
doc/小论文/流程图/generate_concise_assets.py
```

标准调用：

```bash
source .venv-figure/bin/activate

python "doc/小论文/流程图/generate_concise_assets.py" \
  --config input/config/golden_1024.json \
  --golden-dir output/figure_data/golden_1024 \
  --fpga-dir output/figure_data/full_platform_1024 \
  --output-dir "doc/小论文/流程图/assets_concise/1024_17x17_10kernels" \
  --image-size 1200 \
  --dpi 300 \
  --colormap viridis \
  2>&1 | tee output/figure_data/logs/figure_generation.log
```

当前 checkout 如果尚未包含 `generate_concise_assets.py`，应停止出图并将以下信息反馈给开发侧：

```bash
ls -l "doc/小论文/流程图/"
git rev-parse HEAD
```

不要改用 `generate_concise_assets.ps1`，也不要自行从已有 PNG 裁剪。

### 13.2 必需输入

脚本只能读取以下数据：

- `input/config/golden_1024.json`；
- Golden 目录中的 `mskf_r/i.bin`；
- Golden 目录中的 `scales.bin`；
- Golden 目录中的 10 对 `krn_k_r/i.bin`；
- Golden 目录中的 `tmpImgp_full_128.bin`；
- Golden 目录中的 `aerial_image_socs_kernel.bin`；
- full-platform 目录中的 `fpga_tmpimgp_full_128.bin`；
- full-platform 目录中的 `fpga_aerial_fi.bin`。

脚本不得读取任何 `.png`、`.jpg`、`.pdf` 或 Draw.io 导出图作为数值输入。

### 13.3 失败策略

以下任一条件成立时，脚本必须非零退出：

- 配置不是 1024×1024、10 核；
- 计算得到的 $N_x,N_y$ 不是 8；
- kernel 文件不是 17×17 float32；
- kernel 数量不是 10；
- 权重数量不是 10；
- CPU 或 FPGA 中间结果不是 128×128；
- CPU 或 FPGA 最终结果不是 1024×1024；
- CPU 重算 SOCS 累加与 Golden `tmpImgp_full_128.bin` 的 RMSE 超过阈值；
- 任何输入包含 NaN 或 Inf；
- CPU 与 FPGA 同类图无法建立共享色标范围。

## 14. I01–I10 输出目录契约

输出根目录：

```text
doc/小论文/流程图/assets_concise/1024_17x17_10kernels/
```

### I01：频域输入

```text
I01_frequency_inputs/
├── mask_spectrum.png
├── weights.csv
└── kernels/
    ├── K01.png
    ├── K02.png
    ├── ...
    └── K10.png
```

数据来源和变换：

- `mask_spectrum.png`：`log1p(abs(mskf_r + j*mskf_i))`；
- `K01.png` 至 `K10.png`：`log1p(abs(krn_k_r + j*krn_k_i))`；
- 10 张 kernel 图共享同一个 `vmin/vmax`；
- `weights.csv` 直接来自 `scales.bin`；
- 不生成 10 核拼图。

### I02：第 1 核频域乘积

```text
I02_frequency_product/
├── K01_mask_window.png
├── K01_kernel.png
└── K01_product.png
```

分别显示：

$$
|\hat M_{17\times17}|,\quad |\Phi_1|,\quad |\hat M_{17\times17}\Phi_1|.
$$

全部采用 `log1p(abs())`。

### I03：第 1 核空间域强度

```text
I03_single_kernel_intensity/
└── K01_intensity.png
```

$$
I_1(x,y)=\sigma_1\left|\mathcal F^{-1}\{\hat M\Phi_1\}\right|^2.
$$

使用线性强度，不使用对数色标。

### I04：CPU 10 核累加

```text
I04_cpu_weighted_sum/
└── cpu_tmpimgp_128.png
```

直接读取或由原始 Golden 核重算：

$$
I_{CPU}^{128}=\operatorname{FFTshift}\left(\sum_{k=1}^{10}\sigma_k|E_k|^2\right).
$$

### I05：CPU 最终空中像

```text
I05_cpu_aerial/
└── cpu_aerial_1024.png
```

数据来源：`aerial_image_socs_kernel.bin`。

### I06：FPGA 输入引用

```text
I06_frequency_inputs/
├── mask_spectrum.png
├── weights.csv
├── kernels/
│   ├── K01.png
│   ├── ...
│   └── K10.png
└── shared_with_I01.json
```

图片和权重与 I01 使用相同原始数据及显示范围，便于直接替换 Draw.io 占位框；JSON 同时记录共享关系。CPU 和 FPGA 使用完全相同的频谱、核与权重。

### I07：固定网格嵌入

```text
I07_fixed_grid_embedding/
├── K01_embedded_128.png
└── region.json
```

- `K01_embedded_128.png` 只显示纯数值热力图；
- 不在图片中烘焙红框、箭头或文字；
- `region.json` 记录有效 17×17 区域坐标、网格尺寸和坐标约定；
- 当前 HLS 对齐实现的预期嵌入起点为 `(x=94,y=94)`，但应由脚本根据权威实现核对后写入，不应仅依赖文档硬编码。

### I08：FPGA 单核强度引用

```text
I08_single_kernel_intensity/
├── K01_intensity.png
└── shared_with_I03.json
```

`K01_intensity.png` 与 I03 使用相同数值结果及显示范围，便于直接排版；JSON 同时记录共享关系。区别由 Draw.io 中的 CPU 软件循环和 FPGA 流水线标签表达。

### I09：FPGA 10 核板上累加

```text
I09_fpga_weighted_sum/
└── fpga_tmpimgp_128.png
```

数据来源：`fpga_tmpimgp_full_128.bin`。

I04 与 I09 必须共享 `vmin/vmax`。

### I10：FPGA + Host FI 最终空中像

```text
I10_fpga_host_fi/
└── fpga_aerial_1024.png
```

数据来源：`fpga_aerial_fi.bin`。

I05 与 I10 必须共享 `vmin/vmax`。

## 15. SCI 统一视觉规范

### 15.1 图像格式

| 项目 | 规范 |
| --- | --- |
| 文件格式 | 无损 PNG，sRGB |
| 单图画布 | 1200×1200 px |
| DPI metadata | 300 dpi |
| colormap | `viridis` |
| 背景 | 白色或透明，整组统一 |
| 标题 | 不包含 |
| 坐标轴和刻度 | 不包含 |
| colorbar | 不包含 |
| 内嵌文字和编号 | 不包含 |
| 外边框 | 不包含 |
| 插值 | 小矩阵建议 nearest；连续空中像建议 bilinear/bicubic，需记录 |

### 15.2 数值显示

- 频谱、复数 kernel 和频域乘积：`log1p(abs(data))`；
- 单核强度、累加结果和最终空中像：线性强度；
- 10 个 kernel 使用统一范围；
- I04/I09 使用统一范围；
- I05/I10 使用统一范围；
- 不允许按每张图单独归一化后再声称可定量对比。

### 15.3 排版边界

Linux 只输出纯数据图和 manifest。以下工作留给 Windows/Draw.io：

- 10 核 SOCS kernel 拼版；
- 裁剪核图之间的间距；
- `(a)` 至 `(e)` 编号；
- 箭头、框、图例和说明文字；
- 最终双栏宽度适配；
- PDF/SVG 导出。

## 16. Manifest 和生成报告

素材根目录必须包含：

```text
manifest.json
style_manifest.json
generation_report.json
SHA256SUMS
```

### 16.1 manifest.json

每个图至少记录：

```json
{
  "id": "I09",
  "file": "I09_fpga_weighted_sum/fpga_tmpimgp_128.png",
  "source": "output/figure_data/full_platform_1024/fpga_tmpimgp_full_128.bin",
  "source_type": "FPGA_BOARD",
  "dtype": "float32",
  "shape": [128, 128],
  "display_transform": "linear",
  "colormap": "viridis",
  "vmin": 0.0,
  "vmax": 0.3804,
  "sha256": "..."
}
```

### 16.2 style_manifest.json

至少记录：

- 图像尺寸；
- DPI；
- colormap；
- 背景；
- 插值方式；
- kernel 全局范围；
- CPU/FPGA tmpImgp 共享范围；
- CPU/FPGA aerial 共享范围。

### 16.3 generation_report.json

至少记录：

- Git SHA；
- config SHA-256；
- 运行时间 UTC；
- Python/NumPy/Matplotlib 版本；
- 所有输入字节数；
- NaN/Inf 检查结果；
- kernel 数量和尺寸；
- CPU 重算 tmpImgp 与 Golden 的 RMSE；
- FPGA tmpImgp 与 Golden 的 RMSE；
- FPGA Host FI 与 CPU SOCS 的 RMSE；
- 所有生成文件状态；
- 总体 `PASS/FAIL`。

## 17. 自动验收

### 17.1 文件完整性

```bash
ASSET_ROOT="doc/小论文/流程图/assets_concise/1024_17x17_10kernels"

find "$ASSET_ROOT" -type f -printf '%P %s bytes\n' \
  | sort \
  | tee output/figure_data/logs/asset_files.txt

test "$(find "$ASSET_ROOT/I01_frequency_inputs/kernels" -name 'K*.png' | wc -l)" -eq 10
test "$(wc -l < "$ASSET_ROOT/I01_frequency_inputs/weights.csv")" -ge 11
```

### 17.2 Manifest 路径

```bash
jq -e '.status == "PASS"' "$ASSET_ROOT/generation_report.json"
jq -e 'length > 0' "$ASSET_ROOT/manifest.json"
```

### 17.3 SHA-256

```bash
(
  cd "$ASSET_ROOT"
  find . -type f ! -name SHA256SUMS -print0 \
    | sort -z \
    | xargs -0 sha256sum > SHA256SUMS
  sha256sum -c SHA256SUMS
)
```

### 17.4 人工抽查

至少检查：

- I01 mask spectrum；
- I01 的 K01、K05、K10；
- I02 product；
- I03 single-kernel intensity；
- I04 和 I09；
- I05 和 I10。

确认：

- 图像方向一致；
- 没有标题、坐标和 colorbar；
- 正方形无拉伸；
- I04/I09 视觉范围一致；
- I05/I10 视觉范围一致；
- 没有从历史 PNG 裁剪得到的内容。

## 18. 回传包

### 18.1 必需内容

```text
linux_figure_handoff_<RUN_TAG>/
├── README_EXECUTION.txt
├── config/
│   └── golden_1024.json
├── environment/
│   ├── git_sha.txt
│   ├── git_status.txt
│   ├── platform_info.txt
│   ├── python_version.txt
│   ├── pip_freeze.txt
│   ├── lspci_endpoint.txt
│   ├── lspci_verbose.txt
│   └── xdma_devices.txt
├── logs/
│   ├── golden_compile.log
│   ├── golden_generation.log
│   ├── full_platform_dry_run.log
│   ├── full_platform_run.log
│   └── figure_generation.log
├── reports/
│   ├── summary.json
│   ├── metrics.csv
│   ├── timing.csv
│   ├── full_platform_report.md
│   └── generation_report.json
├── raw/
│   ├── scales.bin
│   ├── tmpImgp_full_128.bin
│   ├── fpga_tmpimgp_full_128.bin
│   ├── aerial_image_socs_kernel.bin
│   └── fpga_aerial_fi.bin
└── assets/
    └── 1024_17x17_10kernels/
```

完整 `mskf_r/i.bin` 和 10 对 kernel BIN 体积可接受时也应一并回传；若不回传，必须在 manifest 中提供其 SHA-256。

### 18.2 打包命令

```bash
HANDOFF_DIR="output/figure_data/handoff_${RUN_TAG}"
mkdir -p "$HANDOFF_DIR"/{config,environment,logs,reports,raw,assets}

cp input/config/golden_1024.json "$HANDOFF_DIR/config/"
cp output/figure_data/logs/* "$HANDOFF_DIR/logs/"
cp output/figure_data/logs/git_sha.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/git_status.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/platform_info.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/python_version.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/pip_freeze.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/lspci_endpoint.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/lspci_verbose.txt "$HANDOFF_DIR/environment/"
cp output/figure_data/logs/xdma_devices.txt "$HANDOFF_DIR/environment/"

cp output/figure_data/full_platform_1024/{summary.json,metrics.csv,timing.csv,full_platform_report.md} \
  "$HANDOFF_DIR/reports/"
cp "doc/小论文/流程图/assets_concise/1024_17x17_10kernels/generation_report.json" \
  "$HANDOFF_DIR/reports/"

cp output/figure_data/golden_1024/{scales.bin,tmpImgp_full_128.bin,aerial_image_socs_kernel.bin} \
  "$HANDOFF_DIR/raw/"
cp output/figure_data/full_platform_1024/{fpga_tmpimgp_full_128.bin,fpga_aerial_fi.bin} \
  "$HANDOFF_DIR/raw/"

cp -a "doc/小论文/流程图/assets_concise/1024_17x17_10kernels/." \
  "$HANDOFF_DIR/assets/"

(
  cd output/figure_data
  tar --zstd -cf "linux_figure_handoff_${RUN_TAG}.tar.zst" "handoff_${RUN_TAG}"
  sha256sum "linux_figure_handoff_${RUN_TAG}.tar.zst" \
    > "linux_figure_handoff_${RUN_TAG}.tar.zst.sha256"
)
```

不要打包：

- `.venv-figure/`；
- `validation/golden/src/*.o`；
- Python `__pycache__/`；
- HLS/Vivado build 目录；
- 与本次 1024 配置无关的 2048 测试输出。

## 19. 常见故障

### 19.1 Golden 编译找不到 FFTW

现象：

```text
cannot find -lfftw3
fftw3.h: No such file or directory
```

处理：

```bash
sudo apt install --reinstall libfftw3-dev
ldconfig -p | grep fftw3
```

### 19.2 LAPACK/BLAS 链接失败

```bash
sudo apt install --reinstall liblapack-dev libblas-dev
ldconfig -p | grep -E 'lapack|blas'
```

### 19.3 Python 找不到 NumPy

使用虚拟环境绝对路径：

```bash
source .venv-figure/bin/activate
python -c 'import numpy; print(numpy.__version__)'
```

上板时：

```bash
sudo -E env PYTHON="$(realpath .venv-figure/bin/python)" \
  bash source/host/full_platform/run.sh --help
```

### 19.4 `/dev/xdma*` 不存在

系统自带 `drivers/dma/xilinx/xdma.ko` 不一定提供 Xilinx XDMA 字符设备。需要加载 Xilinx XDMA Reference Driver，并确认当前内核版本与模块匹配。

检查：

```bash
lsmod | grep xdma
modinfo xdma
sudo dmesg | tail -100
```

### 19.5 user BAR 不可达

若 dmesg 显示 `user -1`，需要确认 bitstream 是否允许 XDMA M_AXI 访问 HLS `control` 和 `control_r` 地址段。DMA-only 通过不代表 HLS 控制路径通过。

### 19.6 PCIe 降级为 x2

```bash
sudo lspci -vv -s 01:00.0 | grep -E 'LnkCap|LnkSta'
```

先检查板卡插接、插槽和 lane 错误。精度通常不受链路宽度影响，但传输性能数据不可直接用于论文。

### 19.7 kernel 变成 33×33

说明误用了 2048 配置。立即停止并检查：

```bash
cat output/figure_data/golden_1024/kernels/kernel_info.txt
jq '.mask.period, .optics, .source.annular, .kernel.count' \
  input/config/golden_1024.json
```

不要裁剪 33×33 kernel 伪装成 17×17。

### 19.8 RMSE 超限

依次检查：

1. config 与 Golden 目录是否匹配；
2. kernel 数量、顺序和权重是否一致；
3. `tmpImgp_full_128.bin` 是否真为 128×128；
4. FPGA bitstream 是否为 V18 17×17 配置；
5. AXI 地址和寄存器映射是否匹配；
6. 输入实部/虚部是否交换；
7. FFTshift、嵌入位置和块浮点补偿是否一致。

不要通过修改图像归一化掩盖数值错误。

## 20. 最终交付检查表

- [ ] Git SHA 和工作树状态已记录。
- [ ] Ubuntu、GCC、Python、依赖版本已记录。
- [ ] PCIe endpoint、Gen3 x8 和 XDMA 设备已确认。
- [ ] 配置为 1024×1024、17×17、10 核。
- [ ] Golden 从干净的专用目录重新生成。
- [ ] 10 对 kernel BIN 完整且每个分量为 1,156 bytes。
- [ ] CPU `tmpImgp_full_128.bin` 为 65,536 bytes。
- [ ] FPGA `fpga_tmpimgp_full_128.bin` 为 65,536 bytes。
- [ ] CPU 和 FPGA 最终空中像均为 4,194,304 bytes。
- [ ] 两项硬件关键 RMSE 均小于 $10^{-5}$。
- [ ] I01 至 I10 全部从 JSON/BIN 生成。
- [ ] 未读取或裁剪历史 PNG。
- [ ] 10 个 kernel 分别输出，未自动拼版。
- [ ] 所有图片为统一 SCI 风格。
- [ ] I04/I09 和 I05/I10 分别共享色标范围。
- [ ] manifest、style manifest、generation report 和 SHA256SUMS 完整。
- [ ] 回传包已生成并通过 SHA-256 校验。

完成以上检查后，Linux 数值出图阶段结束。后续只允许在不改变像素数据和数值映射的前提下进行 Draw.io 排版。
