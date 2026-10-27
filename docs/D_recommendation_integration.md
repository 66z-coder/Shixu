# D 推荐模块接入说明

D 现在调用 C 的真实菜谱接口，并提供 B 库存到推荐输入的转换。E 可以调用 `readUserContext` 组装输入，再调用 `recommend`。本次没有接入 E 的主窗口，也没有改动 B、C 的公共类型或接口。

## 已连接的数据来源

- C：`findRecipesByFoods` 获取候选，`calculateMatch` 计算按份数放大的食材数量满足率，`calculateNutrition` 计算本次份数的营养。
- B：`InventoryManager::getAllItems()` 读取 SQLite 中的 `QList<FoodCandidate>`。
- D：`RecommendationContextBuilder` 过滤过期、无效数量并换算单位，生成 C 需要的 `FoodBatch`，随后执行多因素评分、PriorityQueue Top-K 和解释。
- C 的菜谱 ID 为整数。`Recommendation.recipeId` 已对齐为 `int`，`Recommendation.recipe` 保留完整 C 菜谱，供 E 展示步骤或调用 C 的 `recordMeal`。
- 原临时 `RecipeCandidate` 已移除；单菜评分和 Top-K 函数接收 C 的 `Recipe`。

## E 最小调用示例

```cpp
#include "inventory.h"
#include "recommendation/RecommendationContextBuilder.h"

using namespace Shixu::RecommendationEngine;

// InventoryManager 由 E 管理生命周期，复用同一个实例。
// 不要在每次刷新时创建第二个使用同名数据库连接的实例。
InventoryManager inventory;
UserContext preferences;
preferences.topK = 5;
preferences.servings = 1;
preferences.maxPreparationMinutes = 30;
preferences.excludedIngredients = {QStringLiteral("花生")};
preferences.preferredTags = {QStringLiteral("清淡")};

auto built = readUserContext(inventory, preferences);
auto results = recommend(built.context);
// 将 built.warnings 显示给用户，不能忽略未能换算单位的食材。
for (const auto& item : results) {
    auto explanation = explainRecommendation(item);
    // 显示 item.recipeId、item.dishName、item.missingIngredients、
    // item.nutrition、explanation.reasons 等。
}
```

如果 E 已经调用 B 得到库存列表，可以使用 `buildUserContext(foods, preferences)`，无需重复查询数据库。为 E 的目标链接 `ShixuRecommendationInventory` 即可取得推荐层、B、C 及 Qt Sql 依赖。

## 单位、日期与评分约定

- C 的 `FoodBatch.amount` 按克传入。`g/克`、`kg/公斤/千克`、`mg/毫克` 可自动换算。
- `个/盒/袋` 的重量未知时，暂不参与数量匹配，并在 `warnings` 中说明。不能把“2 个鸡蛋”直接当成“2 克鸡蛋”。
- 如果已有实测转换，可传入 `FoodUnitConversion{食材名称, 单位, 每单位克数}`。转换只对指定食材和单位生效。
- 已过期（到期日早于参考日期）的批次排除；当天到期仍属于临期；临期默认含未来三天。
- 无效 `QDate` 表示未知，仍可作为库存使用，但没有临期加分。
- `calculateMatch` 以菜谱用量乘 `servings` 后计算；营养也由 C 按 `servings` 计算。
- 候选允许部分匹配，必须同时向用户展示 `missingIngredients`，不能声称这些菜都可立即制作。
- 初始权重：数量满足 35%、临期利用 25%、偏好 20%、能量目标匹配 15%、时间 5%。未提供偏好、能量目标或有效用时的可选因素不参加评分，参与的权重重新归一化。权重仍可由团队调整。
- 能量目标由用户通过 `targetCaloriesPerServing` 提供；默认 0 不评分。该分项只表示与输入目标的接近程度，不是健康评价。C 内置营养数据为课程估算值。
- 忌口目前按食材名称精确匹配（去首尾空白、忽略大小写）；食材别名与过敏原分类需要团队另行定义。

## 两个运行目标

- `RecommendationDemo`：固定示例库存 + C 的真实菜谱模块，方便快速检查 D 算法。
- `RecommendationLiveDemo`：输入食材 → B 保存 SQLite → 重新读取库存 → C 查询/匹配/营养 → D 推荐。首次库存为空。可输入番茄 300 g、鸡蛋 200 g、食用油 50 g，然后修改份数、时间、忌口、标签查看结果。

联调窗口把数据库放在用户本地应用数据目录下的 `Shixu/RecommendationLiveDemo/shixu.db`（以窗口底部显示的绝对路径为准）。它是独立联调库存，不会自动读取 A/E 窗口使用的数据库。正式运行时 E 需统一 B 的数据库工作目录；B 当前接口将数据库名称固定为相对路径 `shixu.db`。

## B 尚未合入 develop 时的依赖

本次检查使用 C 所在 `develop` 的 `b927849`，B 的 `feature/inventory` 为 `c570a33690e246526257690ab297c2b6be3f4399`。

B 文件未复制进 D 的受版本控制目录。当前电脑在已经被 Git 忽略的 `build/dependencies/inventory` 下保留 B 的原样快照用于编译；对应提交记在 `SOURCE_COMMIT.txt`。

CMake 优先使用当前项目中的 `src/inventory/inventory.cpp` 和 `include/model/FoodCandidate.h`；若没有，可以将 `SHIXU_INVENTORY_ROOT` 指向 B 的本地项目文件夹。当前电脑还支持上述本地快照作为后备。新克隆的 D 分支若没有 B 依赖，会跳过联调目标，并保留 C/D 算法和示例目标。

```text
cmake -S . -B build -DSHIXU_INVENTORY_ROOT="B 模块本地项目路径"
```

B 的 PR 合入共享分支后，CMake 自动优先使用共享源文件，无需复制或覆盖 B 的实现。提交 D 的更新前，应向组长注明该依赖。

## 构建与检查

在 Qt/MinGW 的运行时目录可用的环境中配置并构建：

```text
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest 登记 C 的四个现有验证程序，以及 D 的 `RecommendationTests` 和 `RecommendationIntegrationTests`。后者在临时目录创建、关闭、重开 SQLite，检查真实 B/C/D 链路、单位转换、未知/过期日期、份数、缺料提示、Top-K 次序和异常输入。

## 后续需团队确认

1. B/C 最终公共库存类型仍待统一。D 当前适配现有 `FoodCandidate` 和 C 的临时 `FoodBatch`，不将它们视为同一个类型。
2. E 需确定主窗口中偏好输入、刷新时机、缺料提示和解释展示，并调用上述推荐入口。
3. B 当前 `addFood` 返回 `void`、读取失败与空库存都可能返回空列表，D 无法完整诊断库存错误；联调窗口对保存操作通过重新读取数量检查。统一错误处理需 B/E 另行约定。
4. 当前只完成本地接入与验证，GitHub PR 的合并由团队决定。
