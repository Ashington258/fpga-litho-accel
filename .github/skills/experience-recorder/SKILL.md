---
name: experience-recorder
description: "WORKFLOW SKILL — Automatically capture and integrate development experiences into project constraints and skills. USE FOR: updating copilot-instructions.md; improving SKILL.md files; recording lessons learned; fixing outdated documentation; maintaining knowledge base. Essential for continuous improvement of AI assistance quality. DO NOT USE FOR: one-time tasks; user-specific preferences; temporary workarounds."
---

# Experience Recorder Skill

## 适用场景

当需要**自动更新**项目知识库时使用此skill：

- **记录经验教训**：将实践中发现的问题和解决方案固化到文档
- **更新全局约束**：修改 `copilot-instructions.md` 中的约束和规范
- **改进Skill文档**：优化现有skill的工作流程和最佳实践
- **维护知识一致性**：确保文档与实际代码状态同步

## 核心工作流程

### 1. 自动触发条件

**在以下情况下自动激活**：

1. ✅ **验证失败**：发现配置错误或工具使用问题
2. 🎯 **性能优化**：找到更好的实现方案
3. 🐛 **Bug修复**：解决了一个常见陷阱
4. 📚 **新知识**：学习到新的API用法或最佳实践
5. ⚠️ **约束冲突**：发现文档与实际不符

### 2. 经验分类与记录

**经验类型**：

| 类型 | 记录位置 | 示例 |
|------|---------|------|
| **关键约束** | `copilot-instructions.md` | AXI-MM接口必须添加depth参数 |
| **工作流程** | `SKILL.md` | C仿真实际执行目录为 `hls/<work_dir>/hls/csim/build/` |
| **常见陷阱** | `SKILL.md` (Pitfalls section) | 配置文件不应包含 `[package]` section |
| **最佳实践** | `SKILL.md` (Best Practices) | 使用编译时常量作为depth值 |
| **性能优化** | `copilot-instructions.md` | HLS FFT IP比Direct DFT快10倍 |

### 3. 自动更新机制

**执行流程**：
```
1. 检测到需要记录的经验
   ↓
2. 分类经验类型
   ↓
3. 确定更新位置（copilot-instructions.md 或 SKILL.md）
   ↓
4. 生成更新内容（遵循文档格式规范）
   ↓
5. 应用更新（使用replace_string_in_file工具）
   ↓
6. 记录更新日志到 /memories/repo/
```

## 实现示例

### 示例1：记录关键约束

**触发场景**：
```
用户执行Co-Simulation失败：
ERROR: A depth specification is required for interface port 'mskf_r'
```

**自动执行**：
```markdown
# 1. 识别经验类型：关键约束
# 2. 确定更新位置：copilot-instructions.md

# 3. 生成更新内容：
### ⚠️ AXI-MM接口depth参数要求

**强制要求**：所有AXI-MM接口必须添加 `depth=<size>` 参数。

**正确示例**：
```cpp
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 depth=1024*1024
```

**原因**：Co-Simulation需要depth参数生成RTL testbench。

# 4. 应用更新到 copilot-instructions.md
# 5. 记录到 /memories/repo/experience_log.md
```

### 示例2：改进Skill工作流程

**触发场景**：
```
用户发现C仿真找不到数据文件：
Cannot open file: data/mskf_r.bin
```

**自动执行**：
```markdown
# 1. 识别经验类型：工作流程问题
# 2. 确定更新位置：hls-full-validation/SKILL.md

# 3. 在"关键经验教训"表格中添加新行：
| **数据路径错误** | `Cannot open file: data/mskf_r.bin` | C Simulation实际执行目录为 `hls/<work_dir>/hls/csim/build/`，正确相对路径为 `../../../../../data/` |

# 4. 应用更新
# 5. 记录到 /memories/repo/experience_log.md
```

### 示例3：记录性能优化经验

**触发场景**：
```
用户测试发现HLS FFT IP比Direct DFT快10倍
```

**自动执行**：
```markdown
# 1. 识别经验类型：性能优化
# 2. 确定更新位置：copilot-instructions.md

# 3. 在"FFT优化方案"部分添加：
#### 方案1：集成HLS FFT IP（已验证）

**实测效果**：
- **C仿真时间**：从454s降至45s（降低10倍）
- **DSP消耗**：从8,064降至1,600（降低80%）
- **Fmax**：从273MHz提升至300MHz+

**验证日期**：2026-04-25

# 4. 应用更新
# 5. 记录到 /memories/repo/experience_log.md
```

## 自动化规则

### 规则1：优先级判断

**高优先级（立即更新）**：
- 🚨 导致验证失败的问题
- ⚠️ 文档与实际代码不符
- 🔒 安全相关约束

**中优先级（批量更新）**：
- 📈 性能优化经验
- 🛠️ 工具使用技巧
- 💡 最佳实践

**低优先级（定期更新）**：
- 📝 文档格式改进
- 🎨 代码风格建议

### 规则2：更新验证

**更新前检查**：
1. ✅ 确认经验已验证（非临时方案）
2. ✅ 检查是否与现有约束冲突
3. ✅ 确保更新位置正确
4. ✅ 遵循文档格式规范

**更新后验证**：
1. ✅ 读取更新后的文档确认格式正确
2. ✅ 记录更新日志
3. ✅ 通知用户更新内容

### 规则3：批量更新策略

**触发批量更新的条件**：
- 完成一个Phase的所有任务
- 发现3个以上相关问题
- 用户明确要求"更新文档"

**批量更新流程**：
```
1. 收集本阶段所有经验
2. 按类型分组（约束、流程、陷阱、最佳实践）
3. 生成统一更新计划
4. 一次性应用所有更新
5. 生成更新摘要报告
```

## 与其他Skill的集成

**调用关系**：
```
hls-full-validation
  └─ 发现问题 → 调用 experience-recorder

hls-csynth-verify
  └─ 发现优化机会 → 调用 experience-recorder

auto-task-progression
  └─ 完成Phase → 调用 experience-recorder (批量更新)
```

## 配置选项

**在copilot-instructions.md中添加**：
```yaml
# 经验记录配置
experience_recording:
  auto_update_constraints: true      # 自动更新全局约束
  auto_update_skills: true           # 自动更新skill文档
  batch_update_on_phase_complete: true  # Phase完成时批量更新
  log_to_repo_memory: true           # 记录到 /memories/repo/
  require_user_confirmation: false   # 无需用户确认（自动更新）
```

## 更新日志格式

**在 `/memories/repo/experience_log.md` 中记录**：
```markdown
# Experience Update Log

## 2026-04-25 14:30 - AXI-MM depth参数约束

**触发场景**：Co-Simulation失败
**经验类型**：关键约束
**更新位置**：copilot-instructions.md
**更新内容**：
- 添加AXI-MM接口depth参数要求
- 提供正确配置示例
- 说明原因（Co-Simulation需要）

**验证状态**：✅ 已验证（修复后Co-Simulation通过）

---

## 2026-04-25 15:00 - C仿真数据路径问题

**触发场景**：C仿真找不到数据文件
**经验类型**：工作流程
**更新位置**：hls-full-validation/SKILL.md
**更新内容**：
- 在"关键经验教训"表格添加新行
- 说明C仿真实际执行目录
- 提供正确相对路径示例

**验证状态**：✅ 已验证（修复后C仿真通过）
```

## 最佳实践

1. **及时记录**：发现问题后立即记录，避免遗忘
2. **验证优先**：只记录已验证的经验，不记录临时方案
3. **格式一致**：遵循现有文档格式和风格
4. **避免重复**：更新前检查是否已存在相同内容
5. **保持简洁**：记录核心要点，避免冗长描述

## 注意事项

1. **不覆盖用户修改**：如果用户手动修改了文档，保留用户版本
2. **版本控制**：重大更新前备份原文档
3. **冲突处理**：如遇冲突，询问用户选择
4. **回滚机制**：支持撤销最近的更新
