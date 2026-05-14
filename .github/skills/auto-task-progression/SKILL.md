---
name: auto-task-progression
description: "WORKFLOW SKILL — Automatically progress through TODO tasks without user intervention. USE FOR: reading TODO documents; identifying current task status; executing next steps; updating task progress; chaining multiple tasks. Essential for autonomous development workflows. DO NOT USE FOR: single-step tasks; user-initiated specific operations."
---

# Auto Task Progression Skill

## 适用场景

当需要**自动推进**开发任务时使用此skill：

- **读取TODO文档**：解析任务清单和当前状态
- **识别下一步**：自动判断应该执行哪个任务
- **链式执行**：完成一个任务后自动开始下一个
- **进度更新**：自动更新TODO文档中的任务状态

## 核心工作流程

### 1. 读取TODO文档并解析状态

**自动执行**：
```
1. 读取项目TODO文档（如 SOCS_TODO.md）
2. 解析当前阶段和任务状态
3. 识别"⏳ 待开始"或"🔄 进行中"的任务
4. 确定下一个应该执行的任务
```

### 2. 执行任务并自动推进

**自动化规则**：
- ✅ 完成任务后，**立即**更新TODO状态为"✅ 完成"
- 🔄 自动识别下一个"⏳ 待开始"任务
- 🚀 **无需用户确认**，直接开始执行下一个任务
- 📝 记录执行日志到session memory

### 3. 任务链式执行逻辑

**示例流程**：
```
Phase 2.1: HLS FFT IP优化
  ├─ 任务 2.1.1: FFT配置优化 [⏳ 待开始]
  │   → 自动执行 → 完成后更新状态 → 自动开始下一个
  ├─ 任务 2.1.2: FFT流水线优化 [⏳ 待开始]
  │   → 自动执行 → 完成后更新状态 → 自动开始下一个
  └─ 任务 2.1.3: 验证优化效果 [⏳ 待开始]
      → 自动执行 → 完成后更新状态 → 自动开始下一个
```

## 自动化触发条件

**自动开始下一个任务的条件**：

1. ✅ 当前任务状态已更新为"完成"
2. 📋 TODO文档中存在"⏳ 待开始"任务
3. 🔗 下一个任务与当前任务属于同一阶段
4. ⚠️ 无阻塞问题（如资源超限、验证失败）

**需要用户确认的情况**：

1. 🚨 验证失败（RMSE超标、资源超限）
2. 🔀 阶段切换（Phase 1 → Phase 2）
3. 💰 重大决策（如更换架构方案）
4. ⏸️ 用户明确要求暂停

## 实现机制

### 机制1：TODO文档状态标记

**标准格式**：
```markdown
### 任务 2.1: HLS FFT IP优化 ⭐ (优先级1)

**状态**: ⏳ 待开始 | 🔄 进行中 | ✅ 完成 | ❌ 失败

**子任务**:
- [ ] 2.1.1: FFT配置优化 (⏳ 待开始)
- [x] 2.1.2: FFT流水线优化 (✅ 完成)
- [ ] 2.1.3: 验证优化效果 (⏳ 待开始)
```

### 机制2：自动状态更新

**执行流程**：
```python
# 伪代码示例
def auto_progress_tasks():
    todo_doc = read_file("SOCS_TODO.md")
    current_task = find_task_by_status("🔄 进行中")
    
    if current_task:
        execute_task(current_task)
        update_status(current_task, "✅ 完成")
        
        next_task = find_next_task(current_task)
        if next_task and should_auto_start(next_task):
            update_status(next_task, "🔄 进行中")
            execute_task(next_task)  # 自动开始下一个
```

### 机制3：Session Memory记录

**自动记录执行日志**：
```markdown
# /memories/session/task_progression.md

## 2026-04-25 Task Progression Log

### 14:30 - 任务 2.1.1: FFT配置优化
- 状态: ⏳ 待开始 → 🔄 进行中
- 操作: 修改FFT配置参数
- 结果: Latency降低15%
- 状态: 🔄 进行中 → ✅ 完成

### 14:45 - 任务 2.1.2: FFT流水线优化 (自动开始)
- 状态: ⏳ 待开始 → 🔄 进行中
- 操作: 添加DATAFLOW pragma
- 结果: 吞吐量提升2倍
- 状态: 🔄 进行中 → ✅ 完成
```

## 与其他Skill的集成

**调用链**：
```
auto-task-progression
  ├─ 调用 hls-csynth-verify (执行C综合)
  ├─ 调用 hls-full-validation (执行完整验证)
  └─ 调用 experience-recorder (记录经验教训)
```

## 使用示例

**用户请求**：
```
"开始Phase 2优化工作"
```

**Copilot自动执行**：
1. 读取 `SOCS_TODO.md`，识别Phase 2任务
2. 找到第一个"⏳ 待开始"任务：任务 2.1.1
3. 更新状态为"🔄 进行中"
4. 执行任务 2.1.1（调用hls-csynth-verify skill）
5. 完成后更新状态为"✅ 完成"
6. **自动识别**下一个任务 2.1.2
7. **无需用户确认**，直接开始执行任务 2.1.2
8. 重复步骤3-7，直到Phase 2所有任务完成

**关键改进**：
- ❌ 旧方式：完成任务 → 等待用户说"继续下一步"
- ✅ 新方式：完成任务 → 自动开始下一个任务

## 配置选项

**在copilot-instructions.md中添加**：
```yaml
# 自动化配置
automation:
  auto_progress_tasks: true          # 启用自动任务推进
  max_continuous_tasks: 5            # 最多连续执行5个任务后暂停
  pause_on_failure: true             # 失败时暂停
  pause_on_phase_change: true        # 阶段切换时暂停
  log_to_session_memory: true        # 记录到session memory
```

## 注意事项

1. **安全机制**：遇到错误或验证失败时自动暂停
2. **用户控制**：用户可随时说"暂停"或"跳过此任务"
3. **透明度**：每个任务执行前会告知用户当前进度
4. **可追溯**：所有操作记录到session memory

## 最佳实践

1. **TODO文档格式标准化**：使用统一的状态标记（⏳ 🔄 ✅ ❌）
2. **任务粒度适中**：每个任务应在15-30分钟内完成
3. **依赖关系明确**：标注任务之间的依赖关系
4. **验证检查点**：关键任务后添加验证步骤
