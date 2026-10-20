# C 模块（菜谱与营养）接口说明 · B/C 兼容性备忘

> 本文件由 **成员 C** 维护，用于说明 C 模块对外接口现状，以及 C 与 B 模块
> 关于 `FoodBatch` 的对接边界。**不是团队公共接口文档的替代品**，最终以
> 《公共接口文档 V2.0》为准。

---

## 1. C 模块四个正式接口（签名已冻结，与《公共接口文档 V2.0》逐字一致）

| 接口 | 声明位置 |
| --- | --- |
| `std::vector<Recipe> findRecipesByFoods(const std::vector<FoodBatch>& foods)` | `include/recipe/RecipeService.h` |
| `MatchResult calculateMatch(const Recipe& recipe, const std::vector<FoodBatch>& inventory)` | 同上 |
| `NutritionInfo calculateNutrition(const Recipe& recipe, int servings)` | 同上 |
| `void recordMeal(const Recipe& recipe, int servings, const std::string& time)` | 同上 |

以上四个签名**不得修改**。其他模块调用 C 能力时只能使用这四个接口。

---

## 2. `FoodBatch` 现状：**临时类型，非最终公共类型**

- 定义位置：`include/model/FoodBatch.h`。
- 当前字段（C 为通过编译而设的最小可行方案）：
  - `std::string foodId`
  - `std::string foodName`
  - `double amount`
- **B 模块尚未提供正式定义**：`src/inventory/` 当前仅有 `.gitkeep`；
  远端 `origin/feature/inventory` 的 commit 与 `origin/develop` 相同。
- 《分工文档》§7.1 对 `FoodBatch` 的建议字段为
  **`foodId, amount, expireDate, source`（使用者：B / C / D）**，其中
  **没有食材名称字段**，`foodName` 是 C 模块为满足“按名称匹配”而临时加入的。

> ⚠️ 其他成员请勿把带 `foodName` 的当前 `FoodBatch` 当作最终公共类型。
> B 冻结正式定义后，本文件与 `FoodBatch.h` 的临时字段将同步调整。

---

## 3. C 模块对 `FoodBatch` 的实际依赖

C 只读取两个字段，**从不读取 `foodId`**：

| 接口 | 依赖字段 | 代码位置 |
| --- | --- | --- |
| `findRecipesByFoods` | `amount`（>0 过滤）、`foodName`（匹配键） | `src/recipe/RecipeService.cpp` |
| `calculateMatch` | `amount`（>0 聚合）、`foodName`（匹配键） | 同上 |

`foodId` 仅在测试构造对象时被赋值，产品逻辑中未被解释或使用。

---

## 4. 若 B 只提供 `foodId`（不含名称）的不兼容点

| 位置 | 现状 | 仅有 `foodId` 时的后果 |
| --- | --- | --- |
| `RecipeService.cpp` `findRecipesByFoods` | `normalizeName(batch.foodName)` 收集可用食材名 | 名称为空 → 倒排索引命中为空 → 全部菜谱无法匹配，返回空列表 |
| `RecipeService.cpp` `calculateMatch` | `inventoryByNorm[normalizeName(batch.foodName)] += batch.amount` | 库存键为空串 → 与菜谱食材名（中文）无法对应 → 所有食材被计入 `missing`，`matchRate` 恒为 0 |
| `tests/recipe/test_findRecipesByFoods.cpp` / `test_calculateMatch.cpp` | `makeBatch()` 里设置 `b.foodName` | 正式类型若无 `foodName` 字段，测试无法编译 |

**当前项目不存在任何 `foodId → 食材名称` 的查询接口**：
- B 侧建议接口（`addFood` / `consumeFood` / `getExpiringFoods` / `applyInventoryEvent` / `save` / `load`）
  均不返回“按 id 取名称”。
- C 侧亦无 id→名称解析能力（C 的匹配以名称为依据）。

---

## 5. 后续适配所需的最少内容（择一，由团队与 B 确认）

**方案 A（C 最省事，推荐）**：B 的正式 `FoodBatch` 保留一个食材名称字段
（或提供等价可读名称的字段），C 无需改动即可匹配。

**方案 B（B 只给 id）**：由 B 提供一个最小查询接口，例如
```
std::string getFoodNameById(const std::string& foodId);
```
C 内部加一层“id → 名称”适配（不修改 B 代码，仅在 C 侧调用该接口），
再复用现有按名称匹配的逻辑。

> 除上述二选一外，C 不需要 `FoodBatch` 的其它字段（`expireDate` / `source`
> 等由 B/D 使用，C 不读取）。

---

## 6. 兼容性红线

- C **不修改** B 模块代码，也不猜测 B 的最终字段。
- 在 B 冻结正式定义前，C 维持 `FoodBatch.h` 的临时定义以保持可编译、可测试。
- 若接口不足，按《公共接口文档 V2.0》规则“先提出需要修改的接口”，不擅自改他人模块。
