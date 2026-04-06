# FPGA-Litho 验证目录

## 目录结构

```
verification/
├── run_verification.py   # 统一验证入口
├── README.md             # 本文档
└── src/                  # 独立编译源代码
    ├── Makefile
    ├── litho.cpp         # 验证程序 (原名 klitho_full)
    ├── litho             # 可执行文件
    └── common/           # 公共库
        ├── file_io.cpp/hpp
        ├── mask.cpp/hpp
        ├── source.cpp/hpp
        └── stb_image_write.h
```

**独立性**: verification 目录完全独立，不依赖 reference 目录。

## 使用方法

### 快捷入口 (项目根目录)

```bash
python verify.py --clean           # 清理后验证
python verify.py --debug           # 调试模式
python verify.py --compile-only    # 仅编译
```

### 直接运行

```bash
cd verification/src
make clean && make
./litho <config.json> <output_dir> --debug
```

## 输出文件

| 文件 | 说明 |
|------|------|
| `source.png` | 照源分布 |
| `mask.png` | Mask 输入 |
| `tcc_r.bin`/`tcc_i.bin` | TCC 矩阵 (81×81) |
| `image_tcc.bin` | TCC 方法空中像 (512×512) |
| `image.bin` | SOCS 方法空中像 (512×512) |
| `kernels/kernel_info.txt` | SOCS 核信息 |

## 验证指标

- TCC vs SOCS 相对差异 < 10% (nk 截断误差)
- 当前配置 (nk=10) 实测差异 ~5.8e-3