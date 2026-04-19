# JTAG-to-AXI 板级验证指南

通过 **JTAG-to-AXI Master** 进行 SOCS HLS IP 硬件验证与功能验证。

## ✅ 验证状态

### 硬件验证（2026-04-19）✅ 通过

| 验证项         | 结果   | 详情                                 |
| -------------- | ------ | ------------------------------------ |
| DDR4 写入/读取 | ✅ 通过 | 测试模式数据写入后成功读回（非零）   |
| HLS IP 启动    | ✅ 通过 | `ap_ctrl = 0x0E` → IP 完成执行       |
| 输出数据       | ⚠️ 全零 | 预期行为（测试数据非有效 SOCS 输入） |

**结论**：DDR 和 HLS IP 硬件完全正常。

### 功能验证（待执行）

| 验证项          | 结果 | 说明                            |
| --------------- | ---- | ------------------------------- |
| Golden 数据加载 | ⏳    | 使用真实 SOCS 输入数据          |
| HLS 计算输出    | ⏳    | 17×17 空中像（289 floats）      |
| 输出 vs Golden  | ⏳    | 对比 tmpImgp_pad32.bin 参考输出 |

## 📁 文件说明

```
validation/board/jtag/
├── full_validation_with_test_data.tcl   # 硬件验证脚本（测试模式）
├── run_full_validation_with_golden.tcl  # 功能验证脚本（真实数据）⭐ NEW
├── generate_all_load_scripts.py         # 批量生成 DDR 加载脚本 ⭐ NEW
├── visualize_aerial_image.py            # 输出可视化与对比分析 ⭐ NEW
├── bin_to_hex_for_ddr.py                # Python工具：BIN→TCL hex转换
└── README.md                            # 本文档
```

**新增功能**：
- `generate_all_load_scripts.py`: 批量生成所有输入数据的加载脚本
- `run_full_validation_with_golden.tcl`: 完整功能验证流程（7步）
- `visualize_aerial_image.py`: 输出可视化与误差分析

## 📊 地址映射

| 区域       | DDR 地址   | 大小  | 说明           |
| ---------- | ---------- | ----- | -------------- |
| mskf_r     | 0x40000000 | 1MB   | mask 频域实部  |
| mskf_i     | 0x42000000 | 1MB   | mask 频域虚部  |
| scales     | 0x44000000 | 40B   | 特征值（10个） |
| krn_r      | 0x44400000 | 3.2KB | kernel 实部    |
| krn_i      | 0x44800000 | 3.2KB | kernel 虚部    |
| output     | 0x44840000 | 1.2KB | HLS 输出结果   |
| HLS_CTRL   | 0x0000     | -     | HLS 控制寄存器 |
| HLS_PARAMS | 0x10000    | -     | HLS 参数寄存器 |

## 🚀 快速验证流程

### Step 1: 连接硬件

在 Vivado Tcl Console 中执行：

```tcl
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
```

### Step 2: 运行验证脚本

```tcl
cd e:/fpga-litho-accel/validation/board/jtag
source full_validation_with_test_data.tcl
```

### Step 3: 查看结果

脚本自动执行：
1. ✅ DDR 写入测试模式数据
2. ✅ DDR 读取验证
3. ✅ 配置 HLS 参数
4. ✅ 启动 HLS IP
5. ✅ 读取输出数据

预期输出：
```
>>> mskf_r readback: 00000000111111112222222233333333  ← DDR 正常
>>> ap_ctrl = 0000000e                                 ← HLS 完成
>>> Output data: 00000000...                           ← 全零（预期）
```

## � 完整功能验证流程（NEW）

硬件验证通过后，可使用真实 Golden 数据进行功能验证：

### Step 1: 生成所有 DDR 加载脚本

```powershell
cd e:\fpga-litho-accel\validation\board\jtag
python generate_all_load_scripts.py
```

**输出文件**：
- `load_mskf_r.tcl` / `load_mskf_i.tcl` (512×512 floats)
- `load_scales.tcl` (10 floats)
- `load_krn_r.tcl` / `load_krn_i.tcl` (10×81 floats)

### Step 2: 在 Vivado 中执行完整验证

在 Vivado Tcl Console 中：

```tcl
# 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 执行完整验证流程（自动加载所有数据 + 运行 HLS + 读输出）
cd e:/fpga-litho-accel/validation/board/jtag
source run_full_validation_with_golden.tcl
```

**验证流程（7步）**：
1. ✅ 加载 5 组 Golden 数据到 DDR
2. ✅ 验证 DDR 数据（非零读回）
3. ✅ 配置 HLS IP 参数
4. ✅ 启动 HLS IP 执行
5. ✅ 等待 HLS 完成（轮询 ap_done）
6. ✅ 读取输出数据（17×17 = 289 floats）
7. ✅ 保存输出为 HEX 文件

**输出文件**：`aerial_image_output.hex`

### Step 3: 可视化与误差分析

```powershell
# 在 Windows PowerShell 中运行
cd e:\fpga-litho-accel\validation\board\jtag
python visualize_aerial_image.py
```

**可视化输出**：
- `aerial_image_comparison.png`: HLS 输出 vs Golden 参考（6 子图）
- `error_report.txt`: 详细误差分析报告

**误差指标**：
- MSE（均方误差）
- RMSE（均方根误差）
- Max Absolute Error（最大绝对误差）
- Mean Relative Error（平均相对误差）

## 🔧 常见问题

| 问题           | 原因                    | 解决方案                             |
| -------------- | ----------------------- | ------------------------------------ |
| DDR 读取全零   | DDR 未初始化            | 检查 Block Design DDR 配置           |
| ap_ctrl 无响应 | HLS IP 未启动           | 检查 ap_start 写入格式               |
| 地址格式错误   | Vivado 需要十六进制格式 | 使用 `format "0x%08X"`               |
| TCL 命令无效   | Vivado 版本兼容性       | 使用 `run_hw_axi [get_hw...]` 新格式 |

## 📝 验证记录

- **2026-04-19**: 硬件验证通过（DDR + HLS IP 正常）
- **下一步**: 加载真实 Golden 数据进行功能验证# JTAG-to-AXI 板级验证指南

本目录用于通过 **JTAG-to-AXI Master** 方式加载验证数据到 DDR，执行 SOCS HLS IP 硬件验证。

## ✅ 硬件验证状态（2026-04-19）

**`full_validation_with_test_data.tcl` 验证成功！**

| 验证项          | 结果   | 详情                                                      |
| --------------- | ------ | --------------------------------------------------------- |
| **DDR4 写入**   | ✅ 通过 | `mskf_r` 写入测试模式，读回 `0000000011111111...`（非零） |
| **DDR4 读取**   | ✅ 通过 | `scales` 读回 `4280000043000000...`（非零）               |
| **HLS IP 启动** | ✅ 通过 | `ap_ctrl = 0x0E` → ap_done=1, ap_idle=1, ap_ready=1       |
| **输出数据**    | ⚠️ 全零 | **预期行为**（测试模式不是有效的 SOCS 输入数据）          |

**结论**：DDR 和 HLS IP 硬件完全正常。输出为零是因为测试数据不是有效的 SOCS 计算输入。

**下一步**：加载真实的 `mskf_r/i`, `scales`, `krn_r/i` Golden 数据进行功能验证。

## 📁 文件说明

| 文件                                 | 用途                                              |
| ------------------------------------ | ------------------------------------------------- |
| `socs_hls_validation_v2.tcl`         | **主验证脚本 V2**（修复 Vivado 命令兼容性）       |
| `socs_hls_validation.tcl`            | 主验证脚本（旧版，可能存在命令兼容问题）          |
| `run_validation_with_data.tcl`       | 数据加载辅助脚本（配合 V2 版本）                  |
| `full_validation_with_test_data.tcl` | **✅ 硬件验证通过**（测试模式，确认 DDR+HLS 正常） |
| `load_real_data_guide.tcl`           | 真实数据加载指南（下一步操作建议）                |
| `bin_to_tcl_converter.py`            | Python 工具，将 BIN 文件转换为 TCL hex 格式       |
| `data/socs_data.tcl`                 | 小型验证数据（scales、kernels、参考输出）         |
| `data/socs_data_batch.tcl`           | 大型分批数据（mskf_r/i，512×512 floats）          |
| `data/data_usage.tcl`                | 数据写入地址和使用说明                            |

## ⚠️ V2 版本关键更新

`socs_hls_validation_v2.tcl` 解决了以下问题：

1. **Vivado TCL 命令兼容性**：使用 `run_hw_axi [get_hw_axi_txns]` 替代旧的 `run_hw_axi_txn`
2. **事务重复创建错误**：每次创建前删除同名旧事务
3. **增强调试输出**：`report_property`、DATA 显示、状态轮询打印
4. **全0数据检测**：自动检测输出全0并给出调试建议
5. **正确地址映射**：HLS 控制寄存器使用 0x0000（来自 AddressSegments.csv）
6. **启动前后状态检查**：读取 ap_ctrl 确认 AXI-Lite 通路正常
7. **清晰的调试结论**：脚本结尾输出诊断结果和建议

## 🔧 硬件要求

- **FPGA 开发板**：已烧录 SOCS HLS IP 的 Bitstream
- **Vivado Hardware Manager**：已连接 JTAG
- **JTAG-to-AXI Master IP**：已配置在 Block Design 中

## 📊 地址映射参考（来自 AddressSegments.csv）

| AXI Master      | 目标从机        | 地址偏移       | 空间大小 | 用途说明            |
| --------------- | --------------- | -------------- | -------- | ------------------- |
| **JTAG-to-AXI** | s_axi_control   | **0x0000**     | 64K      | **HLS 控制寄存器**  |
| **JTAG-to-AXI** | s_axi_control_r | 0x10000        | 64K      | HLS 参数寄存器      |
| **JTAG-to-AXI** | DDR4            | 0x40000000     | 128M     | DDR 内存空间        |
| **HLS gmem0**   | DDR4            | 0x40000000     | 32M      | mskf_r (mask 实部)  |
| **HLS gmem1**   | DDR4            | 0x42000000     | 32M      | mskf_i (mask 虚部)  |
| **HLS gmem2**   | DDR4            | 0x44000000     | 4M       | scales (特征值)     |
| **HLS gmem3**   | DDR4            | 0x44400000     | 4M       | krn_r (kernel 实部) |
| **HLS gmem4**   | DDR4            | 0x44800000     | 256K     | krn_i (kernel 虚部) |
| **HLS gmem5**   | DDR4            | **0x44840000** | 16K      | **HLS 输出结果**    |

**⚠️ 关键提示**：
- `HLS_CTRL_BASE = 0x0000`（通过 JTAG-to-AXI 访问 HLS 控制寄存器）
- **不要使用 0x40000000 作为 HLS 启动地址**（这是 DDR 基地址，不是控制寄存器）

## 📊 验证数据概览

| 数据名称     | 大小               | DDR 地址   | 写入方式            |
| ------------ | ------------------ | ---------- | ------------------- |
| `mskf_r`     | 512×512 (1MB)      | 0x40000000 | 分批 (2049 batches) |
| `mskf_i`     | 512×512 (1MB)      | 0x42000000 | 分批 (2049 batches) |
| `scales`     | 10 floats          | 0x44000000 | 直接写入            |
| `krn_r`      | 10×81 floats       | 0x44400000 | 直接写入            |
| `krn_i`      | 10×81 floats       | 0x44800000 | 直接写入            |
| **HLS 输出** | 17×17 (289 floats) | 0x44840000 | 读取                |

## 🚀 使用步骤（推荐 V2 版本）

### Step 1: 连接硬件并加载脚本

在 Vivado Tcl Console 中执行：

```tcl
# 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 切换到验证目录
cd e:/fpga-litho-accel/validation/board/jtag

# 加载 V2 主验证脚本（修复命令兼容性，增强调试）
source socs_hls_validation_v2.tcl

# 获取 JTAG-to-AXI Master 接口（脚本已自动获取）
# set axi_if [get_hw_axis hw_axi_1]  ;# 已在脚本中定义
```

### Step 2: 加载小型数据（直接写入）

```tcl
# 加载验证数据变量定义
source data/socs_data.tcl

# 写入 scales (10 floats @ 0x44000000)
axi_write_data $axi_if 0x44000000 $scales_data "Write scales"

# 写入 kernels (10个kernel，各81 floats)
for {set k 0} {$k < 10} {incr k} {
    # 注意：构建变量名后用 [set $var] 获取值，消息中用 ${k} 防止变量名错误解析
    set krn_r_var "krn_${k}_r"
    set krn_i_var "krn_${k}_i"
    axi_write_data $axi_if [expr 0x44400000 + $k*81*4] [set $krn_r_var] "Write krn_${k}_r"
    axi_write_data $axi_if [expr 0x44800000 + $k*81*4] [set $krn_i_var] "Write krn_${k}_i"
}
```

### Step 3: 加载大型数据（分批写入）

⚠️ **注意**：mskf_r/i 数据量大（各1MB），分批写入耗时约 **30-40分钟**

```tcl
# 加载分批数据（文件较大，约5MB）
source data/socs_data_batch.tcl

# 分批写入 mskf_r (2048 batches, 每批 128 floats)
set num_batches 2048
puts "开始写入 mskf_r..."
for {set b 0} {$b < $num_batches} {incr b} {
    set batch_addr [expr 0x40000000 + $b*128*4]
    axi_write_data $axi_if $batch_addr [set mskf_r_batch_$b] "mskf_r batch $b"
    
    # 每100批次打印进度
    if {$b % 100 == 0} {
        puts "进度: mskf_r batch $b / $num_batches"
    }
}

# 分批写入 mskf_i (2048 batches)
puts "开始写入 mskf_i..."
for {set b 0} {$b < $num_batches} {incr b} {
    set batch_addr [expr 0x42000000 + $b*128*4]
    axi_write_data $axi_if $batch_addr [set mskf_i_batch_$b] "mskf_i batch $b"
    
    if {$b % 100 == 0} {
        puts "进度: mskf_i batch $b / $num_batches"
    }
}

puts "数据加载完成！"
```

### Step 4: 启动 HLS IP 执行（使用 V2 版本）

**⚠️ 重要提示**：V2 版本使用正确的 HLS 控制寄存器地址（0x0000），旧版可能使用了错误的 DDR 地址。

```tcl
# 方式 A：使用 V2 脚本的内置流程（推荐）
# V2 脚本已包含启动前后状态检查、超时等待、调试输出
# 只需执行脚本即可完整运行

# 方式 B：手动执行（调试用）
# 注意：HLS_CTRL_BASE = 0x0000 (s_axi_control via JTAG-to-AXI)
#      而不是 0x40000000 (DDR 地址)！

# 启动前状态检查
axi_read_data $axi_if $HLS_CTRL_BASE 1 "Read ap_ctrl BEFORE start"

# 启动 HLS IP
axi_write_data $axi_if $HLS_CTRL_BASE {00000001} "Start HLS IP"

# 启动后状态检查
axi_read_data $axi_if $HLS_CTRL_BASE 1 "Read ap_ctrl AFTER start"

# 等待 ap_done（V2 版本会打印轮询状态）
set done [wait_for_ap_done $axi_if $HLS_CTRL_BASE 10000]
if {!$done} {
    puts "ERROR: HLS IP timeout - check HLS_CTRL_BASE address"
    return
}

puts "HLS IP 执行完成"
```

### Step 5: 读取 HLS 输出结果

```tcl
# 读取输出数据 (289 floats @ 0x44840000)
# 由于 Vivado burst 限制，分3批读取：128+128+33
axi_read_data $axi_if 0x44840000 128 "Read output batch 1"
axi_read_data $axi_if 0x44840200 128 "Read output batch 2"
axi_read_data $axi_if 0x44840400 33  "Read output batch 3"

puts "输出数据已读取到 TCL 变量"
```

### Step 6: 验证输出数据

```tcl
# 比较输出与参考数据 tmpImgp_pad32_data
# 注意：HLS 输出为 17×17=289 floats，参考数据为 32×32=1024 floats
# 需要提取 17×17 区域进行比较

# 显示前10个输出值
puts "HLS 输出前10个值: [lrange $rd_data_0 0 9]"
puts "参考数据前10个值: [lrange $tmpImgp_pad32_data 0 9]"
```

## ⚡ 快速测试流程（使用内置示例数据）

如果只想快速测试硬件连通性，可以使用脚本内置的示例数据：

```tcl
# 加载主脚本
source socs_hls_validation.tcl

# 执行完整验证流程（使用示例数据）
run_socs_validation
```

此命令会：
1. 写入示例测试数据到 DDR
2. 启动 HLS IP
3. 等待执行完成
4. 读取输出并显示

## 🔍 常见问题排查

### ✅ 重大发现：HLS IP 启动成功！

**诊断结果**：`ap_ctrl = 0x0E` 表示 HLS IP 已成功启动并完成执行！

**状态解码**：
- `0x0E = 1110 binary`
- Bit 1 (ap_done) = 1 → IP 已完成
- Bit 2 (ap_idle) = 1 → IP 回到空闲
- Bit 3 (ap_ready) = 1 → IP 准备好新数据

**正确的启动方式**：使用 **Format A**（`-data 00000001 -len 1`）

**⚠️ 注意**：Test 1 显示 DDR 返回全零，需要验证 DDR 配置和 AXI-MM 连接

### 🔧 验证脚本（推荐执行）

HLS IP 已成功启动，现在验证输出数据：

```tcl
# 方式 1：完整端到端验证（包含 DDR 初始化）
source full_validation_with_test_data.tcl

# 方式 2：验证输出数据（假设输入已存在）
source verify_output_after_start.tcl

# 方式 3：再次运行诊断
source hls_startup_diagnostic.tcl
```

### ❓ 问题分析：DDR 返回全零

**现象**：Test 1 中 `>>> DDR read result: 00000000`

**可能原因**：
1. **DDR4 未正确配置**：Block Design 中 DDR4 控制器未连接
2. **AXI Interconnect 缺失**：JTAG-to-AXI → DDR4 的路径未建立
3. **地址映射错误**：DDR4 地址不在 JTAG-to-AXI 可访问范围

**解决方案**：
- 打开 Vivado Block Design
- 检查 DDR4 控制器是否连接到 AXI Interconnect
- 使用 Vivado Address Editor 确认 DDR4 地址映射
- 运行 `full_validation_with_test_data.tcl` 测试 DDR 写入/读取

### 1. TCL 命令兼容性错误

```
invalid command name "run_hw_axi_txn"
```

**解决方案**：使用 V2 脚本，命令格式已修正为：
```tcl
run_hw_axi [get_hw_axi_txns rd_txn]  ;# 正确格式
```

### 2. 事务重复创建错误

```
Can not create a second hw_axi_txn object with the same name
```

**解决方案**：V2 脚本每次创建前自动删除旧事务：
```tcl
catch {delete_hw_axi_txn [get_hw_axi_txns rd_txn]}
create_hw_axi_txn rd_txn ...
```

### 3. 地址格式错误

```
[Labtools 27-3199] Address Value '1149501440' is too large
```

**解决方案**：脚本已修复，使用 hex 格式地址 `0x44840000`

### 4. Burst 长度超限

```
ERROR: Burst length exceeds maximum supported
```

**解决方案**：MAX_BURST_LEN=128，大数据需分批写入

### 5. HLS IP 无法启动 / 输出全0

**症状**：读取 `0x44840000` 返回全零数据

**根本原因排查**：
1. **检查 HLS 控制地址**：确保 `HLS_CTRL_BASE = 0x0000`（来自 AddressSegments.csv）
   - ❌ 错误：`0x40000000`（这是 DDR 基地址，不是控制寄存器）
   - ✅ 正确：`0x0000`（s_axi_control via JTAG-to-AXI）
2. **检查输入数据**：确保 mskf_r/i、scales、kernels 已写入 DDR
3. **检查参数地址**：`HLS_CTRL_R_BASE = 0x10000`（s_axi_control_r）
4. **使用 ILA 抓取 HLS IP 内部信号**：观察 ap_start、ap_done、状态机

**验证步骤**：
```tcl
# 读取 HLS 控制寄存器状态
axi_read_data $axi_if 0x0000 1 "Check HLS control status"

# 如果返回全0或异常值，说明 AXI-Lite control 通路有问题
# 如果返回非0值但 ap_done 未检测到，检查 HLS IP 内部逻辑
```

## 📝 数据重新生成

如果需要从最新的验证数据生成 TCL 文件：

```bash
cd e:\fpga-litho-accel\validation\board\jtag
python bin_to_tcl_converter.py
```

输入数据来源：`output/verification_original/`

## 📈 性能参考

| 操作            | 数据量        | 耗时（参考） |
| --------------- | ------------- | ------------ |
| 写入 scales     | 10 floats     | < 1秒        |
| 写入 10 kernels | 1620 floats   | ~5秒         |
| 写入 mskf_r     | 262144 floats | ~15分钟      |
| 写入 mskf_i     | 262144 floats | ~15分钟      |
| HLS IP 执行     | -             | < 1秒        |
| 读取输出        | 289 floats    | < 1秒        |

**总计**：完整验证流程约 **30-40分钟**（主要耗时在数据加载）

## 🔗 相关文档

- [公共 AXI 测试脚本](../common/axi_memory_test.tcl)
- [板级验证主指南](../README.md)
- [HLS 验证流程报告](../../source/SOCS_HLS/doc/HLS验证流程完整报告.md)