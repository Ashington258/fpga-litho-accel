# ✅ 1. Vitis HLS 综合 & 验证指令总结

## 🔹（1）核心命令框架（Vitis 2025.2）

> ⚠️ 关键点：统一使用 `vitis-run`，不是 `vitis_hls`

```bash
vitis-run --mode hls <功能选项> [参数]
```

---

## 🔹（2）C仿真（CSIM）

```bash
vitis-run \
  --mode hls \
  --csim \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj
```

📌 用途：

* 验证**算法逻辑正确性**
* 不涉及RTL

---

## 🔹（3）C综合（CSYNTH）

### 方法1：TCL驱动（推荐）

```bash
vitis-run \
  --mode hls \
  --tcl \
  --input_file script/hls/run_csynth_bram.tcl \
  --work_dir hls_litho_system_bram_proj
```

### 方法2：传统（已不推荐）

```bash
vivado_hls -f script/hls/run_csynth_bram.tcl
```

📌 用途：

* 生成RTL
* 输出资源/时序报告

---

## 🔹（4）RTL协同仿真（COSIM）

```bash
vitis-run \
  --mode hls \
  --cosim \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj
```

📌 用途：

* 验证 HLS RTL ≈ C行为一致

---

## 🔹（5）IP导出（Package）

```bash
vitis-run \
  --mode hls \
  --package \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj
```

📌 输出：

```
solution1/impl/export/ip/*.zip
```

---

## 🔹（6）TCL脚本示例（CSYNTH核心）

```tcl
open_project hls_litho_system_bram_proj
open_solution solution1

# 可选：设置器件/时钟
set_part xcku3p-ffvb676-2-e
create_clock -period 5

csynth_design

exit
```

---

## 🔹（7）完整流程一键顺序（推荐）

```bash
# 1. C仿真
vitis-run --mode hls --csim --config xxx.cfg

# 2. C综合
vitis-run --mode hls --tcl --input_file run_csynth.tcl

# 3. CoSim（可选）
vitis-run --mode hls --cosim --config xxx.cfg

# 4. IP导出
vitis-run --mode hls --package --config xxx.cfg
```

---

# ✅ 2. TCL测试脚本执行代码（板级验证）

这一部分是**你真正控制FPGA的关键代码**。

---

## 🔹（1）脚本执行入口

### 方法1（推荐）

```bash
vivado -mode tcl -source script/verify/bram_test.tcl
```

### 方法2（GUI）

```tcl
source script/verify/bram_test.tcl
```

---

## 🔹（2）硬件初始化

```tcl
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
```

---

## 🔹（3）AXI接口访问（核心）

### 写操作

```tcl
create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 \
  -data 0x00000004 \
  -len 1 -type write

run_hw_axi_txn wr_txn
```

---

### 读操作

```tcl
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 \
  -len 1 -type read

run_hw_axi_txn rd_txn

set val [get_property DATA rd_txn]
```

---

## 🔹（4）控制寄存器操作（HLS IP关键）

### 启动IP（ap_start）

```tcl
# 写 AP_CTRL (0x00) = 1
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 \
  -data 0x00000001 \
  -len 1 -type write
run_hw_axi_txn start_txn
```

---

### 查询状态（ap_idle）

```tcl
# 读 AP_CTRL
# bit[2] = ap_idle
```

---

## 🔹（5）操作码控制（你的算法入口）

```tcl
# OPERATION寄存器 (0x1c)

# 例：RESET
create_hw_axi_txn op_txn [get_hw_axis hw_axi_1] \
  -address 0x4000001c \
  -data 9 \
  -len 1 -type write
run_hw_axi_txn op_txn
```

---

## 🔹（6）典型测试流程（精简版）

```tcl
# 1. Reset
operation_write 9
ap_start_trigger
wait_for_ap_done

# 2. Load data
operation_write 0  ;# LOAD_SOURCE
# 写数据...

# 3. Compute
operation_write 6  ;# SOCS
ap_start_trigger
wait_for_ap_done

# 4. Read result
operation_write 8
val_read
```

---

## 🔹（7）完整数据流测试核心结构

```tcl
# Reset
operation_write 9

# Load source
for {set i 0} {$i < 100} {incr i} {
    idx_write $i
    val_write ...
}

# Compute SOCS
operation_write 6
ap_start_trigger
wait_for_ap_done

# Read output
for {set i 0} {$i < 100} {incr i} {
    idx_write $i
    val_read
}
```

---

# ✅ 总结（给你一个工程级抽象）

## HLS侧（软件流）

```
C仿真 → C综合 → CoSim → IP导出
```

对应命令：

```
--csim
--tcl (csynth)
--cosim
--package
```

---

## 板级验证（硬件流）

```
连接硬件 → AXI写寄存器 → 启动IP → 等待完成 → 读结果
```

核心TCL：

```
create_hw_axi_txn
run_hw_axi_txn
get_property DATA
```
