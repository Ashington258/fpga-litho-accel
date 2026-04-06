# BIN 文件格式规范

## 概述

本项目中的二进制文件（.bin）采用 **float32（单精度浮点数）** 格式存储，用于光刻仿真中的掩模（Mask）、光源（Source）和核函数（Kernel）数据。

---

## 1. Mask 文件格式

### 文件位置
- 目录：`src/matlab/mask/`
- 示例：`6T_SRAM.bin`, `T1.bin` 等

### 数据格式
| 属性 | 说明 |
|------|------|
| **数据类型** | float32（单精度浮点，4字节/元素） |
| **存储顺序** | 行优先（row-major） |
| **维度** | maskSizeX × maskSizeY（由外部参数指定，如 1024×1024） |
| **值范围** | 0.0 ~ 1.0（0=不透光区域，1=透光区域） |

### 文件结构
```
[字节 0-3]    → float32: 像素(0,0)的值
[字节 4-7]    → float32: 像素(0,1)的值
...
[字节 N*4-4 ~ N*4-1] → float32: 最后一个像素的值
```
其中 N = maskSizeX × maskSizeY

### 读取方式（MATLAB）
```matlab
fid = fopen(filepath, 'rb');
data = fread(fid, [maskX, maskY], 'float');  % 读取为 float32
fclose(fid);
msk = data';  % 转置以匹配 MATLAB 列优先存储
```

---

## 2. Source 文件格式

### 文件位置
- 目录：`src/matlab/source/`
- 示例：`src_test1_size101.bin`

### 数据格式
| 属性 | 说明 |
|------|------|
| **数据类型** | float32（单精度浮点，4字节/元素） |
| **存储顺序** | 行优先（row-major） |
| **维度** | srcSize × srcSize（必须为奇数，如 101×101） |
| **值范围** | 归一化光源强度，总和 = 1.0 |

### 文件结构
```
[字节 0-3]    → float32: 光源点(0,0)的强度
...
[字节 N*4-1]  → float32: 最后一个光源点的强度
```
其中 N = srcSize × srcSize

### 光源类型
- **环形（Annular）**：内外半径之间为有效区域
- **圆形（Circular）**：圆形区域内为有效区域
- **偶极（Dipole）**：两个对称点光源

---

## 3. Kernel 文件格式

### 文件位置
- 目录：`src/matlab/kernel/` 或 `out_validate/kernel/`

### 文件命名规则
| 文件名 | 说明 |
|--------|------|
| `krn_0_r.bin` | 第0个核函数的**实部** |
| `krn_0_i.bin` | 第0个核函数的**虚部** |
| `krn_1_r.bin` | 第1个核函数的实部 |
| `krn_1_i.bin` | 第1个核函数的虚部 |
| `kernel_info.txt` | 元数据文件（尺寸、数量、特征值） |

### 数据格式
| 属性 | 说明 |
|------|------|
| **数据类型** | float32（单精度浮点，4字节/元素） |
| **存储顺序** | 行优先（row-major） |
| **维度** | (2×Ny+1) × (2×Nx+1)，由光学参数计算得出 |
| **内容** | SOCS核函数的实部或虚部 |

### kernel_info.txt 格式
```text
# Kernel Information
## Kernel Size and Count
- Kernel Size: 33x33
- Number of Kernels: 3
## Eigenvalues of Each Kernel
Eigenvalue 0: 92.1831
Eigenvalue 1: 38.5424
Eigenvalue 2: 38.5424
```

---

## 4. 图像输出文件格式

### 文件位置
- 目录：`src/matlab/out/` 或 `out_validate/`

### 文件类型
| 文件名 | 说明 |
|--------|------|
| `image.bin` | 空间像光强分布（float32，行优先） |
| `image_ref.bin` | TCC基准图像 |
| `image_socs.bin` | SOCS重构图像 |

### 数据格式
| 属性 | 说明 |
|------|------|
| **数据类型** | float32 |
| **存储顺序** | 行优先（row-major） |
| **维度** | Lx × Ly（如 2048×2048） |
| **值范围** | 光强值，≥ 0 |

---

## 5. Python 转换工具

使用 `src/tool/png_bin.py` 进行 BIN ↔ PNG 转换：

```bash
# BIN → PNG（可视化）
python src/tool/png_bin.py bin2png input.bin output.png --size 1024 1024

# PNG → BIN（生成掩模）
python src/tool/png_bin.py png2bin input.png output.bin

# 批量转换目录
python src/tool/png_bin.py batch src/matlab/mask --mode bin2png
python src/tool/png_bin.py batch src/matlab/source --mode bin2png
```

---

## 6. 关键注意事项

1. **存储顺序差异**：
   - MATLAB 采用列优先（column-major）
   - C++/Python NumPy 默认采用行优先（row-major）
   - 读写时需注意转置操作

2. **字节序**：
   - 默认使用小端序（little-endian）
   - Python 使用 `<f` 格式，MATLAB 使用 `'float32'`

3. **维度推断**：
   - BIN 文件本身不包含维度信息
   - 需从外部参数或 metadata 文件获取

---

## 7. 文件大小计算

$$
\text{文件大小} = \text{width} \times \text{height} \times 4 \text{ bytes}
$$

例如：
- 1024×1024 mask：1024 × 1024 × 4 = 4,194,304 bytes ≈ 4 MB
- 101×101 source：101 × 101 × 4 = 40,804 bytes ≈ 40 KB
- 33×33 kernel：33 × 33 × 4 = 4,356 bytes ≈ 4 KB