# 板级验证问题修复报告

**日期**: 2026-05-06
**问题**: HLS IP输出全为零
**状态**: ✅ 已修复

---

## 问题诊断

### 症状
1. HLS IP执行完成（ap_done=1）
2. 但输出数据全为零
3. tmpImg_ddr（中间输出）也为零

### 根本原因

**HLS IP参数配置地址错误**

HLS IP有两个AXI-Lite接口：

#### 1. control 接口（基地址 0x0000）
```
0x00: ap_ctrl
0x10: nk
0x18: nx_actual
0x20: ny_actual
0x28: Lx
0x30: Ly
```

#### 2. control_r 接口（基地址 0x10000）
```
0x10: mskf_r (64-bit)
0x1c: mskf_i (64-bit)
0x28: krn_r (64-bit)
0x34: krn_i (64-bit)
0x40: scales (64-bit)
0x4c: tmpImg_ddr (64-bit)
0x58: output_r (64-bit)
```

### 错误配置

原TCL脚本将**nk, nx_actual, ny_actual, Lx, Ly**写入了**错误的地址**：

```tcl
# ❌ 错误：写入 control_r 接口（0x10000 + offset）
nk        0x64  # 实际地址：0x10064（错误）
nx_actual 0x68  # 实际地址：0x10068（错误）
ny_actual 0x6c  # 实际地址：0x1006c（错误）
Lx        0x70  # 实际地址：0x10070（错误）
Ly        0x74  # 实际地址：0x10074（错误）
```

**结果**：HLS IP读取不到正确的参数值，导致无法正常执行计算。

### 正确配置

```tcl
# ✅ 正确：写入 control 接口（0x0000 + offset）
nk        0x10  # 实际地址：0x00010（正确）
nx_actual 0x18  # 实际地址：0x00018（正确）
ny_actual 0x20  # 实际地址：0x00020（正确）
Lx        0x28  # 实际地址：0x00028（正确）
Ly        0x30  # 实际地址：0x00030（正确）
```

---

## 修复方案

### 1. 已修复文件

**文件**: `validation/board/jtag/scripts/tcl/run/run_full_validation_with_golden.tcl`

**修改内容**:
```tcl
# 修改前（错误）
foreach {name offset val} [list \
    nk 0x64 $nk \
    nx_actual 0x68 $nx_actual \
    ny_actual 0x6c $ny_actual \
    Lx 0x70 $Lx \
    Ly 0x74 $Ly \
] {
    set addr_hex [format "0x%08X" [expr {$HLS_PARAMS_BASE + $offset}]]
    ...
}

# 修改后（正确）
foreach {name offset val} [list \
    nk 0x10 $nk \
    nx_actual 0x18 $nx_actual \
    ny_actual 0x20 $ny_actual \
    Lx 0x28 $Lx \
    Ly 0x30 $Ly \
] {
    set addr_hex [format "0x%08X" [expr {$HLS_CTRL_BASE + $offset}]]
    ...
}
```

### 2. 修复脚本

**位置**: `/tmp/fix_and_restart_hls.tcl`

**用途**: 在Vivado TCL Console中运行，修复参数配置并重启HLS IP

**使用方法**:
```tcl
source /tmp/fix_and_restart_hls.tcl
```

---

## 验证步骤

### Step 1: 运行修复脚本

在Vivado TCL Console中执行：
```tcl
source /tmp/fix_and_restart_hls.tcl
```

### Step 2: 检查输出

预期结果：
```
✅ Output has non-zero data!
>>> First 128 words: <非零数据>
```

### Step 3: 完整验证

重新运行完整的验证流程：
```tcl
source validation/board/jtag/scripts/tcl/run/run_full_validation_with_golden.tcl
```

---

## 经验教训

### 1. HLS IP AXI-Lite接口设计

**关键点**：
- HLS IP可能有**多个AXI-Lite接口**
- 每个接口有独立的地址空间
- 参数必须写入正确的接口

**检查方法**：
- 查看HLS IP驱动头文件：`*_hw.h`
- 确认每个参数的基地址和偏移量
- 使用`#define`常量，避免硬编码地址

### 2. 调试技巧

**诊断步骤**：
1. 检查HLS IP控制寄存器（ap_ctrl）
2. 检查中间输出（tmpImg_ddr）
3. 检查参数配置（读取回显验证）
4. 对比HLS IP驱动头文件

**工具**：
- Vivado TCL Console
- JTAG-to-AXI Master
- HLS IP驱动头文件

### 3. 文档规范

**建议**：
- 在TCL脚本中添加详细注释
- 说明每个参数的接口和地址
- 引用HLS IP驱动头文件路径

---

## 相关文件

- **HLS IP驱动头文件**: `source/SOCS_HLS/socs_v18_synth/hls/impl/ip/drivers/calc_socs_2048_hls_stream_refactored_v18_v1_0/src/xcalc_socs_2048_hls_stream_refactored_v18_hw.h`
- **修复后的TCL脚本**: `validation/board/jtag/scripts/tcl/run/run_full_validation_with_golden.tcl`
- **修复脚本**: `/tmp/fix_and_restart_hls.tcl`

---

## 下一步

1. ✅ 运行修复脚本验证
2. ✅ 重新运行完整验证流程
3. ✅ 对比输出与Golden reference
4. ✅ 更新文档和经验记录

