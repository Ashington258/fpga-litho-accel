# HLS SOCS Golden Data Extraction & Skill修正总结

## 任务完成情况

### ✅ 任务1: Golden数据分析与提取计划

#### 1.1 当前配置分析（已确认）

通过分析 `input/config/config.json` 和 `validation/golden/src/litho.cpp`：

**关键参数**:
- **Lx, Ly**: 512×512
- **NA**: 0.8
- **wavelength**: 193 nm
- **outerSigma**: 0.9 (Annular source)
- **Nx, Ny**: **4×4**（动态计算）
  - 公式: $N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$

**尺寸推导**:
| 尺寸类型     | 值        | 说明               |
| ------------ | --------- | ------------------ |
| 物理卷积尺寸 | **17×17** | convX = 4×Nx+1     |
| IFFT执行尺寸 | **32×32** | nextPowerOfTwo(17) |
| SOCS核尺寸   | **9×9**   | kerX = 2×Nx+1      |

**关键发现**: ✅ litho.cpp已满足2^N标准（17→32 zero-padded），无需额外改写！

#### 1.2 已存在Golden数据（已验证）

✅ **输入数据**:
- `mskf_r.bin`, `mskf_i.bin`: mask频域（512×512, 各1MB）
- `scales.bin`: 特征值权重（10个float, 40 bytes）
- `kernels/krn_k_r/i.bin`: 10个SOCS核（每个9×9, 324 bytes）

✅ **最终输出**:
- `aerial_image_socs_kernel.bin`: SOCS完整输出（512×512, 1MB）

#### 1.3 缺失Golden数据（关键）

⚠️ **tmpImgp_pad32.bin** (17×17 float)
- 这是HLS IP的直接输出目标
- 当前litho.cpp输出完整512×512 image
- 需要提取calcSOCS中间步骤

### ✅ 任务2: 验证调试方案正确性并补充修正Skill

#### 2.1 已创建4个HLS Skill

**Skill 1: hls-csynth-verify** ✅
- 功能: C综合验证与性能优化
- 修正点: 
  - 添加当前配置实际值（Nx=4, Ny=4）
  - 明确方案A/B两种配置路径
  - 强调litho.cpp已满足2^N标准

**Skill 2: hls-full-validation** ✅
- 功能: 端到端完整验证流程
- 修正点:
  - 添加步骤0：Golden数据准备前置步骤
  - 明确tmpImgp_pad32.bin作为唯一golden
  - 区分中间输出验证与端到端验证

**Skill 3: hls-board-validation** ✅
- 功能: JTAG-to-AXI板级硬件验证
- 修正点:
  - 明确方案A/B输出尺寸差异
  - 调整输出buffer长度参数
  - 添加配置选择指南

**Skill 4: hls-golden-generation** ✅ **新增**
- 功能: Golden数据生成流程
- 内容:
  - 完整的golden数据提取流程
  - calcSOCS_reference编写指南
  - 补零布局、IFFT缩放等关键细节文档化
  - 自动化验证检查点

#### 2.2 创建的支持文档

**详细计划文档**: `source/SOCS_HLS/data/GOLDEN_EXTRACTION_PLAN.md`
- 当前配置分析和golden状态
- 缺失数据识别和解决方案
- calcSOCS_reference设计要点
- 关键算法细节文档化
- 验收标准和执行计划

**分析脚本**: `source/SOCS_HLS/scripts/extract_hls_golden_simple.py`
- 自动分析当前配置和Nx计算
- 检查已存在golden数据
- 识别缺失数据并提出解决方案

## 关键技术发现

### 1. Nx/Ny动态计算机制

**公式**: $N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$

**当前值**: Nx=4, Ny=4（不是配置文件固定值！）

**影响**: 
- 确定IFFT尺寸（32×32）
- 确定输出尺寸（17×17）
- 影响HLS IP设计参数

### 2. litho.cpp已满足HLS要求

**发现**: litho.cpp已使用 `nextPowerOfTwo()` 自动填充
```cpp
int fftConvX = nextPowerOfTwo(convX);  // 17 → 32
```

**意义**: 无需算法改写，可直接使用litho.cpp作为参考

### 3. 缺失关键Golden数据

**问题**: litho.cpp输出完整512×512，但HLS需要17×17中间输出

**解决方案**: 编写calcSOCS_reference.cpp

**关键细节**:
1. **补零布局**: dif偏移 vs off偏移
2. **IFFT缩放**: 手动除以 (fftSizeX × fftSizeY)
3. **提取规则**: 从32×32中心提取17×17

## 下一步行动建议

### 立即执行（优先级：高）

1. **编写calcSOCS_reference.cpp**（4-6小时）
   - 参考 `GOLDEN_EXTRACTION_PLAN.md` 第4节
   - 实现两种补零布局（dif和off）
   - 输出tmpImgp_pad32.bin（17×17）

2. **生成HLS Golden数据**
   ```bash
   cd validation/golden/src
   make calcSOCS_reference
   ./calcSOCS_reference
   ```

3. **验证Golden正确性**
   - 检查文件大小（289 bytes）
   - 验证数值范围
   - 对比两种补零布局偏差

### 后续HLS开发（优先级：中）

1. **阶段0**: 准备基础设施和Golden数据
2. **阶段1**: 验证32点FFT可用性
3. **阶段2**: 实现32×32 2D IFFT
4. **阶段3**: 完整calcSOCS核心实现
5. **阶段4**: Top接口和testbench
6. **阶段5**: 优化和综合

## 文件清单

### 已创建文件

```
source/SOCS_HLS/
├── data/
│   └── GOLDEN_EXTRACTION_PLAN.md          # 详细golden提取计划
├── scripts/
│   ├── extract_hls_golden.py              # Golden分析脚本（有语法错误，已弃用）
│   └── extract_hls_golden_simple.py       # 简化版分析脚本（推荐使用）
└── SOCS_TODO.md                           # 项目任务清单（已更新）

.github/skills/
├── vitis-hls-workflow/SKILL.md            # 基础HLS workflow skill
├── hls-csynth-verify/SKILL.md             # C综合验证skill（已修正）
├── hls-full-validation/SKILL.md           # 完整验证skill（已修正）
├── hls-board-validation/SKILL.md          # 板级验证skill（已修正）
└── hls-golden-generation/SKILL.md         # Golden生成skill（新增）
```

### 待创建文件

```
validation/golden/src/
└── calcSOCS_reference.cpp                 # TODO: 独立golden生成程序
```

## Skill使用指南

### 示例调用场景

1. **生成Golden数据**:
   ```
   用户: "生成HLS SOCS验证所需的golden数据"
   → 自动调用 hls-golden-generation skill
   ```

2. **完整验证流程**:
   ```
   用户: "对HLS SOCS IP进行完整验证"
   → 自动调用 hls-full-validation skill
   ```

3. **性能优化**:
   ```
   用户: "优化HLS综合的Fmax和Latency"
   → 自动调用 hls-csynth-verify skill
   ```

4. **板级测试**:
   ```
   用户: "在FPGA板上验证HLS IP"
   → 自动调用 hls-board-validation skill
   ```

## 总结

✅ **已完成**:
- 当前配置分析（Nx=4, IFFT 32×32）
- Golden数据状态检查（缺失tmpImgp_pad32.bin）
- 4个HLS skill创建和修正
- 详细golden提取计划文档

⚠️ **待执行**:
- 编写calcSOCS_reference.cpp生成golden
- 运行HLS验证流程

🎯 **关键发现**:
- litho.cpp已满足2^N标准，无需算法改写
- 当前配置Nx=4（非Nx=16），适配32×32 IFFT
- 需要编写独立reference生成中间输出golden

**预计下一步时间**: 4-6小时（编写reference + 验证）
