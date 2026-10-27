# 手动录入重量（A 模块）
计数单位显示“每个/盒/袋…重量（g）”，用户可填写单件净重；不预填估计值。
总重量 = quantity × gramsPerUnit。g/kg/斤分别按 1/1000/500 换算。
ml/L 不假定密度，保留重量未知。未填写重量不阻止保存。
更换食材名称或单位会清空旧单件重量；修改数量会重算并取消确认。
列表、重新编辑、批量 foodsConfirmed 输出都保留重量。

FoodCandidate 末尾新增：
- gramsPerUnit：计数单位的用户填写单件净重，0 为未填写。
- totalWeightGrams：总净重，0 为未知，不能解释成库存为零。
原 name/quantity/unit/expiryDate 保留原义。按 g/kg/斤录入时 gramsPerUnit 为 0，
totalWeightGrams 仍有值。旧调用方不填写新增字段时默认未知。

主程序的智能录入页现已接入 B：整批成功后清空列表并刷新首页/库存；失败保留内容。
B 启动时在事务中补充 gramsPerUnit、totalWeightGrams 两列，已有记录默认 0，保留旧数据。
addFoods 批量保存会检查名称、单位、有限正数量及有限非负重量；总重量重新计算，不信任调用方的旧总重量。
g/kg/mg/斤按质量换算；计数单位使用填写的单件重量；体积及未知单位不假定密度或规格。
所有查询返回两个重量字段。未知重量可保存，不代表零库存。

D 优先使用有效 totalWeightGrams；旧数据仍走单位/显式换算表，并支持斤→500g。
C 的 FoodBatch.amount 按当前调用契约继续接收克数，热量公式不变。
数据库位置仍为进程工作目录中的 shixu.db，启动主程序时请保持相同工作目录。

验证：PackagingIntegrationTest 使用临时数据库，覆盖旧库升级、重启、事务回滚、
主窗口批量提交及失败重试、页面切换保留草稿、重量传到 D。不修改用户数据库。
