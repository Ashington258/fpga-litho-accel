# Copilot自动化使用指南

**创建日期**: 2026-04-25  
**目的**: 提升Copilot自动化程度，减少用户干预

---

## 📋 概述

本指南介绍如何使用新增的自动化机制：

1. **自动任务推进**：完成TODO任务后自动开始下一个
2. **自动经验记录**：自动更新全局约束和skill文档

---

## 🚀 快速开始

### 1. 启用自动化配置

在 `.github/copilot-instructions.md` 中已添加自动化配置：

```yaml
automation:
  auto_progress_tasks: true          # 启用自动任务推进
  max_continuous_tasks: 5            # 最多连续执行5个任务
  pause_on_failure: true             # 失败时暂停
  pause_on_phase_change: true        # 阶段切换时暂停
  log_to_session_memory: true        # 记录日志

experience_recording:
  auto_update_constraints: true      # 自动更新全局约束
  auto_update_skills: true           # 自动更新skill文档
  batch_update_on_phase_complete: true  # Phase完成时批量更新
  log_to_repo_memory: true           # 记录到repo memory
  require_user_confirmation: false   # 无需用户确认
```

### 2. 标准化TODO文档格式

确保 `SOCS_TODO.md` 使用统一的状态标记：

```markdown
### 任务 2.1: HLS FFT IP优化 ⭐ (优先级1)

**状态**: ⏳ 待开始 | 🔄 进行中 | ✅ 完成 | ❌ 失败

**子任务**:
- [ ] 2.1.1: FFT配置优化 (⏳ 待开始)
- [x] 2.1.2: FFT流水线优化 (✅ 完成)
- [ ] 2.1.3: 验证优化效果 (⏳ 待开始)
```

**状态标记说明**：
- ⏳ 待开始：任务尚未开始
- 🔄 进行中：任务正在执行
- ✅ 完成：任务已成功完成
- ❌ 失败：任务执行失败

---

## 📖 使用示例

### 示例1：自动推进Phase 2优化任务

**用户请求**：
```
"开始Phase 2优化工作"
```

**Copilot自动执行**：

#### 步骤1：读取TODO并识别任务
```
读取 SOCS_TODO.md
找到 Phase 2 第一个任务：任务 2.1.1 (⏳ 待开始)
```

#### 步骤2：执行任务2.1.1
```
更新状态：⏳ 待开始 → 🔄 进行中
调用 hls-csynth-verify skill
执行FFT配置优化
验证优化效果
更新状态：🔄 进行中 → ✅ 完成
```

#### 步骤3：自动开始下一个任务
```
识别下一个任务：任务 2.1.2 (⏳ 待开始)
无需用户确认，直接开始执行
更新状态：⏳ 待开始 → 🔄 进行中
执行FFT流水线优化
...
```

#### 步骤4：记录到session memory
```
写入 /memories/session/task_progression.md：
- 14:30 - 任务 2.1.1 完成
- 14:45 - 任务 2.1.2 自动开始
- 15:00 - 任务 2.1.2 完成
- 15:05 - 任务 2.1.3 自动开始
```

**关键改进**：
- ❌ 旧方式：完成任务 → 用户说"继续下一步" → 开始下一个
- ✅ 新方式：完成任务 → 自动开始下一个（无需用户干预）

---

### 示例2：自动记录经验教训

**场景**：Co-Simulation失败

**错误信息**：
```
ERROR: A depth specification is required for interface port 'mskf_r' for cosimulation.
```

**Copilot自动执行**：

#### 步骤1：识别问题类型
```
问题类型：关键约束
影响范围：所有AXI-MM接口
严重程度：高（导致Co-Simulation失败）
```

#### 步骤2：确定更新位置
```
更新位置1：copilot-instructions.md（全局约束）
更新位置2：hls-full-validation/SKILL.md（工作流程）
```

#### 步骤3：生成更新内容
```markdown
# copilot-instructions.md 更新内容：
### ⚠️ AXI-MM接口depth参数要求

**强制要求**：所有AXI-MM接口必须添加 `depth=<size>` 参数。

**正确示例**：
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 depth=1024*1024

**原因**：Co-Simulation需要depth参数生成RTL testbench。

# hls-full-validation/SKILL.md 更新内容：
在"关键经验教训"表格添加新行：
| **AXI-MM depth缺失** | `A depth specification is required...` | 所有AXI-MM接口必须添加 `depth=<size>` 参数 |
```

#### 步骤4：应用更新
```
使用 replace_string_in_file 工具更新文档
```

#### 步骤5：记录更新日志
```markdown
# /memories/repo/experience_log.md

## 2026-04-25 14:30 - AXI-MM depth参数约束

**触发场景**：Co-Simulation失败
**经验类型**：关键约束
**更新位置**：copilot-instructions.md, hls-full-validation/SKILL.md
**验证状态**：✅ 已验证（修复后Co-Simulation通过）
```

**关键改进**：
- ❌ 旧方式：发现问题 → 用户手动记录 → 可能遗忘
- ✅ 新方式：发现问题 → 自动更新文档 → 持续改进

---

## 🎯 最佳实践

### 1. TODO文档维护

**推荐做法**：
- ✅ 使用统一的状态标记（⏳ 🔄 ✅ ❌）
- ✅ 任务粒度适中（15-30分钟完成）
- ✅ 标注任务依赖关系
- ✅ 关键任务后添加验证步骤

**避免**：
- ❌ 状态标记不一致（如"进行中"、"已完成"混用）
- ❌ 任务粒度过大（如"完成整个Phase"）
- ❌ 缺少依赖关系说明

### 2. 经验记录时机

**高优先级（立即记录）**：
- 🚨 导致验证失败的问题
- ⚠️ 文档与实际代码不符
- 🔒 安全相关约束

**中优先级（批量记录）**：
- 📈 性能优化经验
- 🛠️ 工具使用技巧
- 💡 最佳实践

**低优先级（定期记录）**：
- 📝 文档格式改进
- 🎨 代码风格建议

### 3. 自动化边界

**自动执行的任务**：
- ✅ 重复性高的任务（如C综合、验证）
- ✅ 有明确成功标准的任务（如RMSE < 1e-5）
- ✅ 低风险任务（不涉及架构变更）

**需要用户确认的任务**：
- 🚨 验证失败（RMSE超标、资源超限）
- 🔀 阶段切换（Phase 1 → Phase 2）
- 💰 重大决策（如更换架构方案）
- ⏸️ 用户明确要求暂停

---

## 🔧 高级配置

### 自定义自动化行为

**修改 `copilot-instructions.md` 中的配置**：

```yaml
# 调整最大连续任务数
automation:
  max_continuous_tasks: 3  # 改为3个任务后暂停（更保守）

# 禁用自动经验记录
experience_recording:
  require_user_confirmation: true  # 改为需要用户确认

# 禁用自动任务推进
automation:
  auto_progress_tasks: false  # 改为手动推进
```

### 添加自定义触发条件

**在skill中添加触发逻辑**：

```markdown
# 在 hls-csynth-verify/SKILL.md 中添加：

## 自动触发条件

当满足以下条件时，自动调用 experience-recorder：

1. Fmax提升 > 10%
2. Latency降低 > 20%
3. 资源占用降低 > 15%
4. 发现新的优化技巧
```

---

## 📊 效果评估

### 预期改进

| 指标 | 改进前 | 改进后 | 提升 |
|------|--------|--------|------|
| 用户干预次数 | 每个任务1次 | 每5个任务1次 | 80% ↓ |
| 文档更新及时性 | 1-2天后 | 实时 | 100% ↑ |
| 经验遗忘率 | ~30% | <5% | 83% ↓ |
| 任务推进效率 | 手动驱动 | 自动驱动 | 2-3倍 ↑ |

### 监控指标

**在 `/memories/session/automation_metrics.md` 中记录**：

```markdown
# 自动化效果监控

## 2026-04-25 Phase 2执行统计

- 总任务数：15
- 自动执行：12（80%）
- 需要用户确认：3（20%）
- 平均任务完成时间：18分钟
- 文档自动更新次数：7
- 经验记录次数：5

## 问题统计

- 验证失败：2次（自动暂停）
- 资源超限：1次（自动暂停）
- 配置错误：3次（自动修复并记录）
```

---

## 🐛 故障排查

### 问题1：任务未自动推进

**可能原因**：
- TODO文档格式不正确
- 状态标记不一致
- 达到 `max_continuous_tasks` 限制

**解决方案**：
1. 检查TODO文档格式是否符合标准
2. 确认状态标记使用正确（⏳ 🔄 ✅ ❌）
3. 检查session memory中的日志

### 问题2：经验未自动记录

**可能原因**：
- `experience_recording.auto_update_constraints` 设置为false
- 经验类型判断为低优先级
- 更新位置判断错误

**解决方案**：
1. 检查 `copilot-instructions.md` 中的配置
2. 手动触发经验记录：说"记录这个经验"
3. 检查 `/memories/repo/experience_log.md`

### 问题3：自动化过于激进

**调整配置**：
```yaml
automation:
  max_continuous_tasks: 2  # 减少连续任务数
  pause_on_phase_change: true  # 阶段切换时暂停

experience_recording:
  require_user_confirmation: true  # 需要用户确认
```

---

## 📚 相关文档

- **Skill文档**：
  - `.github/skills/auto-task-progression/SKILL.md`
  - `.github/skills/experience-recorder/SKILL.md`
  - `.github/skills/hls-full-validation/SKILL.md`
  - `.github/skills/hls-csynth-verify/SKILL.md`

- **全局约束**：
  - `.github/copilot-instructions.md`

- **任务清单**：
  - `source/SOCS_HLS/SOCS_TODO.md`

- **经验日志**：
  - `/memories/repo/experience_log.md`

---

## 💡 未来改进方向

1. **智能任务优先级**：根据资源占用、风险等级自动调整任务顺序
2. **预测性经验记录**：在问题发生前预测并记录潜在陷阱
3. **跨项目知识共享**：将经验同步到其他项目的copilot-instructions.md
4. **可视化进度追踪**：生成任务执行甘特图和经验积累曲线
