# C 模块开发与测试清单

> 维护者：**成员 C**（菜谱与营养分析模块）
> 本清单以仓库实际文件为依据整理，用于交付核对。
> 生成时**未修改任何 C++ 源码 / 头文件 / CMake 配置 / 测试代码，未执行 Git 操作**。

---

## 1. C 模块职责范围

C 模块负责《食序》数据链中“现有食材能做什么、吃进去是什么情况”一环：

- 菜谱数据模型与内置菜谱库（30 道中式家常菜）。
- “食材 → 菜谱”倒排索引：根据库存食材反查候选菜谱。
- 菜谱食材满足率计算（已有 / 缺少食材清单）。
- 菜谱营养计算（热量、蛋白质、碳水化合物、脂肪），支持份数调整。
- 用餐记录写入与按日期查询 / 汇总（本地文本持久化）。

职责边界：C **不实现**识别（A）、库存状态（B）、推荐（D）、界面与集成（E）；
跨模块只通过《公共接口文档 V2.0》规定的接口交互。

---

## 2. 四个公共接口及功能

声明位置：[RecipeService.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/recipe/RecipeService.h)（签名与《公共接口文档 V2.0》逐字一致，已冻结）
实现位置：[RecipeService.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/RecipeService.cpp)

| 接口 | 签名 | 功能 |
| --- | --- | --- |
| `findRecipesByFoods` | `std::vector<Recipe> findRecipesByFoods(const std::vector<FoodBatch>& foods)` | 根据库存食材返回候选菜谱：完整匹配优先，其后按食材覆盖率降序、同率按 id 升序。`amount <= 0` 视为不可用；空库存返回空列表。 |
| `calculateMatch` | `MatchResult calculateMatch(const Recipe& recipe, const std::vector<FoodBatch>& inventory)` | 按**食材种类数**计算满足率（百分比 0–100，保留两位小数），并列出 `availableIngredients` / `missingIngredients`。同种食材多行 / 多批次先汇总再判定，避免除零。 |
| `calculateNutrition` | `NutritionInfo calculateNutrition(const Recipe& recipe, int servings)` | 以 `amountGrams`（一人份）与 `nutritionPer100g` 计算营养，再乘 `servings`。`servings <= 0` 返回零；`amountGrams <= 0` 的食材跳过。 |
| `recordMeal` | `void recordMeal(const Recipe& recipe, int servings, const std::string& time)` | 记录一条用餐信息：内部复用 `calculateNutrition`，经 `MealRecordStorage` 追加写入本地文本文件。`servings <= 0` 不产生记录；失败经 `MealRecordStorage::getLastError()` 诊断。 |

---

## 3. 数据模型与数据文件位置

### 3.1 核心数据模型（`include/model/`）

| 文件 | 内容 |
| --- | --- |
| [NutritionInfo.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/NutritionInfo.h) | 营养信息：`calories`、`protein`、`carbohydrates`、`fat`（默认 0.0） |
| [RecipeIngredient.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/RecipeIngredient.h) | 菜谱食材：`foodName`、`amountGrams`（一人份克数）、`nutritionPer100g` |
| [Recipe.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/Recipe.h) | 菜谱：`id`、`name`、`ingredients`、`steps`、`cookingTimeMinutes`、`category`、`tags` |
| [MatchResult.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/MatchResult.h) | 匹配结果：`matchRate`（百分比）、`availableIngredients`、`missingIngredients` |
| [MealRecord.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/MealRecord.h) | 用餐记录：`recipeId`、`recipeName`、`servings`、`time`、`date`、`nutrition` |
| [FoodBatch.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/include/model/FoodBatch.h) | 库存批次（**C 模块临时类型，见第 5 节**）：`foodId`、`foodName`、`amount` |

### 3.2 菜谱库与营养数据（`src/recipe/`，C 模块内部）

| 文件 | 内容 |
| --- | --- |
| [RecipeData.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/RecipeData.h) / [RecipeData.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/RecipeData.cpp) | `loadBuiltinRecipes()` 返回 **30 道内置菜谱**（经核实为 30 条 `dish()` 定义，id 1–30）；`kNutritionPer100g` 为每 100 克食材营养表 |
| [RecipeRepository.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/RecipeRepository.h) / [RecipeRepository.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/RecipeRepository.cpp) | 菜谱库与“食材 → 菜谱”倒排索引，提供 `findByIngredient` |

### 3.3 用餐记录存储（`src/recipe/`，C 模块内部）

| 文件 | 内容 |
| --- | --- |
| [MealRecordStorage.h](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/MealRecordStorage.h) / [MealRecordStorage.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/src/recipe/MealRecordStorage.cpp) | 命名空间 `MealRecordStorage`：`setFilePath` / `filePath` / `append` / `loadAll` / `loadByDate` / `summarizeByDate` / `getLastError` / `clearLastError`。格式为管道分隔行（`recipeId|recipeName|servings|time|calories|protein|carbohydrates|fat`），追加写入，默认 `meal_records.txt` |

> 注：`MealRecordStorage.h` 位于 `src/recipe/`（C 内部头），**不在 `include/` 公共目录**。

### 3.4 构建配置

[CMakeLists.txt](file:///d:/university/2.1homework/C%2B%2B/Shixu/CMakeLists.txt)：C 源码集 `SHIXU_RECIPE_SOURCES`（4 个 cpp）、四个独立测试目标；主目标 `Shixu` 仅在检测到 Qt Widgets 时构建。

---

## 4. 四个独立测试目标

以下断言数量为**此前测试记录**，**不是本次重新运行的结果**（本阶段未修改代码、未重新构建运行）。

| 测试目标 | 源文件 | 断言数（此前记录） |
| --- | --- | --- |
| `RecipeDemo` | [test_findRecipesByFoods.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/tests/recipe/test_findRecipesByFoods.cpp) | 20 |
| `CalculateMatchDemo` | [test_calculateMatch.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/tests/recipe/test_calculateMatch.cpp) | 28 |
| `CalculateNutritionDemo` | [test_calculateNutrition.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/tests/recipe/test_calculateNutrition.cpp) | 54 |
| `RecordMealDemo` | [test_recordMeal.cpp](file:///d:/university/2.1homework/C%2B%2B/Shixu/tests/recipe/test_recordMeal.cpp) | 59 |
| **累计** | | **161** |

> 说明：上表 161 为**历史测试记录**（此前各阶段运行、全部 exit 0）。本阶段未运行测试，
> 因此不构成“本次重新验证”。实际重新运行时，请以当次程序输出的 OK/FAIL 计数与退出码为准。

---

## 5. 当前已知限制

1. **`FoodBatch` 仍为 C 模块临时类型**：`include/model/FoodBatch.h` 现含 `foodId` / `foodName` / `amount`，
   其中 `foodName` 是 C 为按名称匹配而自行加入的，**不在《分工文档》§7.1 建议字段**
   （`foodId, amount, expireDate, source`）之内。**最终接口字段待团队统一确认后替换**。
   相关讨论见 [C模块_B_C接口兼容性说明.md](file:///d:/university/2.1homework/C%2B%2B/Shixu/docs/C%E6%A8%A1%E5%9D%97_B_C%E6%8E%A5%E5%8F%A3%E5%85%BC%E5%AE%B9%E6%80%A7%E8%AF%B4%E6%98%8E.md)。
2. **Qt 主程序尚未实际构建验证**：本机未安装 Qt，`Shixu` 主目标未构建；已构建与通过的是
   四个无 Qt 依赖的命令行测试目标，**不能据此声称 Qt 程序可运行**。
3. **用餐记录默认文件路径依赖运行时工作目录**：默认 `meal_records.txt` 为相对路径，
   从不同工作目录启动可能产生多份文件；集成时应由上层用 `setFilePath()` 指向绝对路径。
4. **内置营养数据存在估算值**：`RecipeData.cpp` 的 `kNutritionPer100g` 为参考常见食物成分表的
   **估算值**，非精确实测或权威数据验证，**不能描述为“全部经过权威数据验证”**。

---

## 6. 后续待办

- [ ] 团队**统一接口兼容性检查**：冻结 `FoodBatch` 等公共数据结构与跨模块接口。
- [ ] 按检查结果做**必要适配**（例如 B 若只提供 `foodId`，则在 C 侧增加 id→名称适配或由 B 提供名称字段/查询接口）。
- [ ] **集成验证**：与 A/B/D/E 联合联调，验证数据链闭环（识别 → 库存 → 菜谱/营养 → 推荐 → 交互）。
- [ ] 在具备 Qt 的环境中**构建并运行主程序**，验证 E 模块对 C 接口的实际调用。
- [ ] **最终交付**：确认四个接口、测试目标、示例数据与本文档随交付物一并归档。

---

## 附：相关文档

- [C模块_B_C接口兼容性说明.md](file:///d:/university/2.1homework/C%2B%2B/Shixu/docs/C%E6%A8%A1%E5%9D%97_B_C%E6%8E%A5%E5%8F%A3%E5%85%BC%E5%AE%B9%E6%80%A7%E8%AF%B4%E6%98%8E.md)（C 模块内部接口说明）
- 《公共接口文档 V2.0》《食光小屋_五人分工文档》（**在仓库上级目录，未纳入仓库**）
