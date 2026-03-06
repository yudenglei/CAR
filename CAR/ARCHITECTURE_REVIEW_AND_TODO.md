# CAR 架构梳理、优化方案与待办

## 1) 当前版本核心优化

- 统一了基础类型、句柄与几何类型，去掉重复定义和冲突。
- 修复了 `ReuseVector` 的可复用槽位、代数（generation）校验和 `replace`/`restore` 能力。
- 重写了事务系统：基于命令对象（undo/redo lambda）实现，支持 add/remove/replace 的轻量操作。
- 修复并增强了四叉树分裂逻辑（避免重复插入/丢失对象）。
- 扩充了 PCB 对象：`LayerStack/Port/Symbol/Parameter`，并保留 `PadstackDef/Trace/Via/Component/Net/Surface/Board`。
- 在数据库中增加了名称索引（字符串池 + 前缀搜索），示例支持 `find_by_name_prefix`。

## 2) 关于 MiniMax 对比说明

本环境无法访问 GitHub 外网资源（HTTP 403），因此未能直接拉取并逐个对比 `MiniMax` 仓库中
`CAEFrame-XXX-vXXX.md` 版本文档。建议下一步将该仓库镜像到本地或提供压缩包后，我可以继续完成：

1. 按文件名与版本号构建演进图谱。
2. 识别「增量文档」与「替代文档」。
3. 对每个模块形成最终合并策略与冲突解法。
4. 输出最终统一版 `CAEFrame` 总体架构文档。

## 3) 后续待办（高优先级）

- [ ] 加入参数表达式系统（ParamTable + 依赖图 + 增量求值缓存）。
- [ ] 引入 Cap'n Proto schema（board/net/trace/via/component 等）及版本迁移策略。
- [ ] 增加对象关系图索引（net->pins/vias/traces 的反向索引自动维护）。
- [ ] 完成跨层对象管理策略（padstack/bondwire 的 layer-span 索引 + 查询过滤器）。
- [ ] 为 2D/3D 视图设计共享只读快照层（避免渲染线程锁争用）。
- [ ] 增加基准测试（百万 trace/via 场景内存与吞吐量）。

## 4) 推荐结构（下一阶段）

- `core/`: 基础类型、句柄、内存池、字符串池
- `db/`: ReuseVector 容器、关系索引、事务
- `pcb/`: 业务对象与拓扑关系
- `io/`: capnp schema + load/save + 版本迁移
- `view2d/`, `view3d/`: 视图模型与渲染桥接
- `algo/`: DRC、连通性、路径分析

## 5) 开发者示例：模板化 insert/replace/erase

```cpp
// insert 示例
ObjectId trace_id = db.insert<PCBDatabase::EntityKind::TRACE>(trace);

// replace 示例（不需要单独写事务模板代码）
Trace t2 = trace;
t2.name_id = db.strings.intern("CLK_NEW");
db.replace<PCBDatabase::EntityKind::TRACE>(trace_id, t2);

// erase 示例（同样自动进入 undo/redo 栈）
db.erase<PCBDatabase::EntityKind::TRACE>(trace_id);
```

说明：
- `PCBDatabase::insert/replace/erase` 是通用模板核心。
- 业务层只需要给出 Kind 和对象；容器映射与索引维护由统一模板路径处理。
- 新增 shape 或器件类型时，无需重复实现一整套 undo/redo 逻辑。


## 6) 本轮修正：去函数化事务 + 统一模板接口

- Undo/Redo 由 `std::function` 回调改为紧凑变更日志：`Change{op, kind, handle, before, after}`。
- 回放时按 `kind + op` 分发到容器 `restore/remove/replace`，避免每条事务携带两个函数对象的额外开销。
- 业务接口统一为模板：
  - `db.insert<Kind>(obj)`
  - `db.replace<Kind>(handle, obj)`
  - `db.erase<Kind>(handle)`
- 这样新增器件类型时，只需把类型纳入 `EntityKind` + 容器映射 + 可选索引钩子，不再新增 `add_xxx/remove_xxx/replace_xxx` 重复接口。


## 7) 方案对比（结合 KLayout db::Shapes layer_op 思路）

- 更接近 KLayout 的是**方案3（操作日志驱动）**：
  - 记录“哪个容器(kind) + 做了什么(op) + 哪个对象(handle)”，而不是记录函数回调。
  - Undo/Redo 回放时执行固定的数据路径（restore/remove/replace）。
- 本轮已移除 `Snapshot variant` 方案，改为：
  - `LayerOp{op, kind, handle, before_ref, after_ref}`
  - `before_ref/after_ref` 仅是对象归档引用（archive handle），不是整库快照。
- 这样做的好处：
  - 降低事务记录对象的动态分发负担（无 `std::function`）；
  - 保持可恢复性（erase/replace 仍可找回旧对象）；
  - API 维持统一模板：`insert<Kind>/replace<Kind>/erase<Kind>`。


## 8) 最终版本（V2+V3 融合）

- 事务记录采用 `dbshapes::layer_op` 风格的**模板化 typed-op**：
  - `LayerOp<K>{op, handle, before_idx, after_idx}`
  - `Transaction` 内按类型分桶存储 `m_ops<T>`，并用 `order` 记录跨类型执行顺序。
- 不使用 `std::function`。
- 不使用 `variant AnyOp/ObjectData`。
- `before_idx/after_idx` 仅是归档数组下标（用于 erase/replace 可逆恢复），不是对象快照树。
- 支持 `begin/commit` 批量记录（一次事务多条 layer_op）。
