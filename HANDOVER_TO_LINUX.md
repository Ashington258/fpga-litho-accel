# FPGA-Litho 项目交接清单


## 🎯 下一步执行优先级

### **P0 - 立即执行**（阻塞后续所有工作）

#### Phase 3: Golden数据生成
```bash
cd /root/project/FPGA-Litho
python validation/golden/run_verification.py \
    --config input/config/config_Nx16_exact.json
```

**⚠️ 关键注意**:
- 必须使用 `config_Nx16_exact.json`（NA=1.0, Lx=1632 → Nx=16）
- ❌ 不要使用 `config_Nx16.json`（误导命名，实际产生Nx=4）

**预期输出**（位于 `output/verification/`）:
- `tmpImgp_pad128.bin`: 65×65 float数组（**HLS验证目标**）
- `mskf_r.bin`, `mskf_i.bin`: 512×512频域数据
- `scales.bin`: 10个特征值
- `kernels/krn_*.bin`: Nx=16 kernel数据

**数据验证**:
```bash
python -c "
import numpy as np
tmpImgp = np.fromfile('output/verification/tmpImgp_pad128.bin', dtype=np.float32)
print(f'Size: {len(tmpImgp)} (expected 4225)')
print(f'Range: [{tmpImgp.min():.6f}, {tmpImgp.max():.6f}]')
"
```

---

### **P1 - Phase 4 HLS验证**（依赖Phase 3完成）

#### 工作目录
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
```

#### 4.1 C Simulation（验证HLS输出与Golden匹配）
```bash
vitis-run --mode hls --csim \
    --config script/config/hls_config_socs_full.cfg \
    --work_dir hls/socs_full_csim
```

#### 4.2 C Synthesis（验证性能指标）
```bash
v++ -c --mode hls \
    --config script/config/hls_config_socs_full.cfg \
    --work_dir socs_full_comp
```
**预期指标**: Fmax > 250 MHz, Latency < 50,000 cycles

#### 4.3 Co-Simulation（RTL行为验证）
```bash
vitis-run --mode hls --cosim \
    --config script/config/hls_config_socs_full.cfg \
    --work_dir socs_full_comp
```

---

## ✅ 已完成工作（已推送到GitHub）

### Phase 2.7: HLS代码迁移
- **状态**: ✅ 完成
- **关键改动**: 128×128 FFT改写
- **文件**: `source/SOCS_HLS/src/socs_fft.cpp/h`, `socs_hls.cpp`

### 验证基础设施
- **状态**: ✅ 完成
- **新增**: 19个验证脚本（DDR加载、可视化、诊断工具）
- **位置**: `validation/board/jtag/`, `validation/board/`

### 配置文件修正
- **状态**: ✅ 完成
- **正确配置**: `input/config/config_Nx16_exact.json`

### 编译版本准备
- **状态**: ✅ 已拉取
- **位置**: `validation/golden/src/litho`（Linux ELF可执行文件）

---

## ⚠️ 已知问题

### 30,653×幅度失配问题
- **现状**: HLS输出与Golden存在巨大幅度差异
- **诊断工具**: `validation/board/trace_30_factor.py`
- **解决方案**: 待Phase 3完成后系统性排查

---

## 📋 Git状态

- **Commit Hash**: 439ebcf
- **Branch**: main
- **Status**: 已推送到 origin/main

---

## 📚 详细进度文档

完整进度记录请查看 `/memories/session/phase3_progress_and_linux_plan.md`

---

## 🚀 快速启动命令

```bash
# 1. 拉取最新代码
cd /root/project/FPGA-Litho
git pull origin main

# 2. Phase 3 Golden生成（最关键）
python validation/golden/run_verification.py \
    --config input/config/config_Nx16_exact.json

# 3. 验证输出
ls -lh output/verification/tmpImgp_pad128.bin

# 4. Phase 4 HLS验证
cd source/SOCS_HLS
vitis-run --mode hls --csim \
    --config script/config/hls_config_socs_full.cfg
```

---

**预计完成时间**: Phase 3 ~10分钟 + Phase 4 ~15分钟 = **总计25分钟**