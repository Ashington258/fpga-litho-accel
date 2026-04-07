# SOCS HLS Golden Data

本目录包含HLS SOCS IP验证所需的所有golden数据。

## 数据清单

### 输入数据（HLS IP的输入）

| 文件 | 尺寸 | 说明 |
|------|------|------|
| `mskf_r.bin` | 512×512 complex | Mask频域实部 |
| `mskf_i.bin` | 512×512 complex | Mask频域虚部 |
| `scales.bin` | 10 floats | 特征值权重 |
| `kernels/krn_k_r.bin` | 9×9 complex | SOCS核k实部（k=0..9） |
| `kernels/krn_k_i.bin` | 9×9 complex | SOCS核k虚部（k=0..9） |

### HLS Golden输出 ⭐

| 文件 | 尺寸 | 说明 |
|------|------|------|
| **`tmpImgp_pad32.bin`** | **17×17 floats** | **HLS IP直接输出目标** |

这是HLS IP应该产生的输出。从32×32 IFFT结果中提取的中心17×17区域。

### 端到端参考（可选）

| 文件 | 尺寸 | 说明 |
|------|------|------|
| `aerial_image_socs_kernel.bin` | 512×512 floats | SOCS完整输出（含Fourier Interpolation） |

## 快速使用

### C Simulation验证

```cpp
#include <fstream>
#include <cmath>

// 加载golden数据
void load_golden(const char* filename, float* data, int size) {
    std::ifstream file(filename, std::ios::binary);
    file.read(reinterpret_cast<char*>(data), size * sizeof(float));
    file.close();
}

// HLS testbench
int main() {
    // 加载输入
    float mskf_r[512*512], mskf_i[512*512];
    float scales[10];
    float krn_r[10][9*9], krn_i[10][9*9];
    
    load_golden("data/mskf_r.bin", mskf_r, 512*512);
    load_golden("data/mskf_i.bin", mskf_i, 512*512);
    load_golden("data/scales.bin", scales, 10);
    for (int k = 0; k < 10; k++) {
        char filename[256];
        sprintf(filename, "data/kernels/krn_%d_r.bin", k);
        load_golden(filename, krn_r[k], 9*9);
        sprintf(filename, "data/kernels/krn_%d_i.bin", k);
        load_golden(filename, krn_i[k], 9*9);
    }
    
    // 加载golden输出
    float golden_tmpImgp[289];
    load_golden("data/tmpImgp_pad32.bin", golden_tmpImgp, 289);
    
    // 调用HLS IP
    float hls_tmpImgp[289];
    socs_top(mskf_r, mskf_i, scales, krn_r, krn_i, hls_tmpImgp);
    
    // 验证
    float max_error = 0.0f;
    for (int i = 0; i < 289; i++) {
        float error = fabs(hls_tmpImgp[i] - golden_tmpImgp[i]);
        max_error = fmax(max_error, error);
    }
    
    printf("Max error: %e\n", max_error);
    if (max_error < 1e-5) {
        printf("✅ PASS\n");
        return 0;
    } else {
        printf("❌ FAIL\n");
        return 1;
    }
}
```

## 配置参数

- **Nx, Ny**: 4（动态计算，非配置文件固定值）
- **IFFT尺寸**: 32×32（zero-padded from 17×17）
- **输出尺寸**: 17×17 = 289 floats
- **SOCS核数量**: 10
- **核尺寸**: 9×9

## 数据格式

- **字节序**: Little-endian
- **存储**: Binary, row-major
- **float**: IEEE 754 single precision (4 bytes)
- **complex**: [real, imag] interleaved (8 bytes)

## 下一步

1. **HLS开发**: 参考 `source/SOCS_HLS/SOCS_TODO.md`
2. **验证流程**: 调用 `hls-full-validation` skill
3. **详细说明**: 查看 `GOLDEN_DATA_LIST.md`

---

**生成时间**: 2026年4月7日  
**来源**: `verification/src/litho.cpp`  
**配置**: NA=0.8, Lx=512, λ=193nm, σ_outer=0.9