# SOCS_HLS 目录结构说明

**最后更新**: 2026-04-23  
**状态**: ✅ 已整理

---

## 📁 目录结构概览

```
source/SOCS_HLS/
├── src/                    # HLS源代码
├── tb/                     # 测试平台
├── script/                 # HLS综合脚本
├── board_validation/       # 板级验证工具
├── data/                   # 数据文件
├── doc/                    # 文档目录（已整理）
├── hls/                    # HLS综合结果
├── logs/                   # 日志文件
├── archive/                # 归档文件
├── versions/               # 版本管理
├── validation/             # 验证脚本
├── scripts/                # 工具脚本
├── socs_optimized_comp/    # 优化版本综合结果
├── socs_optimized_csim/    # 优化版本C仿真
├── socs_simple_cosim_*/    # 各版本Co-Simulation结果
├── socs_simple_csim_*/     # 各版本C仿真结果
├── socs_simple_synth_*/    # 各版本综合结果
├── vivado_bd/              # Vivado Block Design
├── README.md               # 项目说明
├── SOCS_TODO.md            # 任务清单
├── AddressSegments.csv     # 地址段定义
└── dfx_runtime.txt         # DFX运行时配置
```

---

## 📋 核心目录说明

### 1. `src/` - HLS源代码
- `socs_optimized.cpp` - 主实现文件（Vivado FFT IP版本）
- `socs_config_optimized.h` - 配置头文件
- 其他版本源代码

### 2. `tb/` - 测试平台
- `tb_socs_optimized.cpp` - 优化版本测试平台
- 其他版本测试平台

### 3. `script/` - HLS综合脚本
- `config/hls_config_optimized.cfg` - 优化版本综合配置
- `run_csynth_socs_optimized.tcl` - 综合脚本
- 其他版本脚本

### 4. `board_validation/` - 板级验证工具
- `socs_optimized/` - 优化版本验证工具
  - `run_socs_optimized_validation.tcl` - 主验证脚本
  - `verify_socs_optimized_results.py` - 结果验证脚本
  - `address_map.json` - DDR地址映射
  - 其他工具和文档

### 5. `doc/` - 文档目录（已整理）
- `project_status/` - 项目状态文件
- `performance_reports/` - 性能报告
- `verification_reports/` - 验证报告
- `final_reports/` - 最终报告
- `summaries/` - 总结文件
- `INDEX.md` - 文档索引

### 6. `validation/` - 验证脚本
- `verify_hls_output.py` - HLS输出验证
- `visualize_aerial_image.py` - 空中像可视化
- 其他验证工具

### 7. `scripts/` - 工具脚本
- `organize_directory.py` - 目录整理脚本
- `run_organize.bat` - 整理脚本批处理
- 其他工具脚本

---

## 🎯 主要文件索引

### 核心代码
- `src/socs_optimized.cpp` - 主实现文件
- `src/socs_config_optimized.h` - 配置头文件
- `tb/tb_socs_optimized.cpp` - 测试平台

### 配置文件
- `script/config/hls_config_optimized.cfg` - HLS综合配置
- `board_validation/socs_optimized/address_map.json` - 地址映射

### 验证工具
- `board_validation/socs_optimized/run_socs_optimized_validation.tcl` - 主验证脚本
- `board_validation/socs_optimized/verify_socs_optimized_results.py` - 结果验证
- `validation/verify_hls_output.py` - HLS输出验证
- `validation/visualize_aerial_image.py` - 空中像可视化

### 文档
- `README.md` - 项目说明
- `SOCS_TODO.md` - 任务清单
- `DIRECTORY_STRUCTURE.md` - 本文件
- `doc/INDEX.md` - 文档索引

---

## 🔧 工具脚本

### 目录整理
```bash
# 运行目录整理脚本
cd e:\fpga-litho-accel\source\SOCS_HLS\scripts
python organize_directory.py

# 或使用批处理
run_organize.bat
```

### HLS输出验证
```bash
# 验证HLS输出与Golden数据
cd e:\fpga-litho-accel\source\SOCS_HLS\validation
python verify_hls_output.py

# 可视化空中像
python visualize_aerial_image.py
```

### 板级验证
```bash
# 生成验证脚本
cd e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized
python generate_socs_optimized_tcl.py

# 验证TCL语法
python test_tcl_syntax.py

# 运行验证（需要硬件）
# 在Vivado TCL Console中运行:
source run_socs_optimized_validation.tcl
```

---

## 📊 版本目录说明

### 优化版本（当前）
- `socs_optimized_comp/` - 优化版本综合结果
- `socs_optimized_csim/` - 优化版本C仿真

### 历史版本
- `socs_simple_cosim_fixed/` - 固定点版本Co-Simulation
- `socs_simple_cosim_float_lut/` - 浮点LUT版本Co-Simulation
- `socs_simple_cosim_v2/` - 版本2 Co-Simulation
- `socs_simple_cosim_v3/` - 版本3 Co-Simulation
- `socs_simple_cosim_v4/` - 版本4 Co-Simulation
- 其他版本目录

---

## 🚀 快速开始

### 1. 查看项目状态
```bash
# 查看任务清单
cat SOCS_TODO.md

# 查看文档索引
cat doc/INDEX.md
```

### 2. 运行HLS综合
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS
v++ -c --mode hls --config script/config/hls_config_optimized.cfg --work_dir socs_optimized_comp
```

### 3. 验证HLS输出
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS\validation
python verify_hls_output.py
```

### 4. 可视化空中像
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS\validation
python visualize_aerial_image.py
```

---

## 📞 技术支持

### 问题排查
1. **目录混乱**: 运行 `scripts/organize_directory.py`
2. **文档缺失**: 检查 `doc/` 目录结构
3. **工具找不到**: 检查 `validation/` 和 `scripts/` 目录

### 联系信息
- **项目路径**: `e:\fpga-litho-accel\source\SOCS_HLS`
- **文档目录**: `doc/`
- **工具脚本**: `scripts/`

---

**最后更新**: 2026-04-23  
**维护人员**: AI Assistant  
**状态**: ✅ 目录已整理完成