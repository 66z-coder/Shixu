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

本分支当前为 A 演示程序，尚未接入 B 数据库。
B 后续需持久化这两个字段；修改数量/单位时保持总重量同步。
D 后续可优先使用有效 totalWeightGrams，未知时保留原有转换/提示逻辑。
C 的 FoodBatch.amount 按当前调用契约继续接收克数，不在此处更改。
这些下游修改尚未在本次实现，不代表已完成 B/C/D 联调。
