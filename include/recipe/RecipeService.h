#pragma once

#include <string>
#include <vector>

#include "model/FoodBatch.h"
#include "model/MatchResult.h"
#include "model/NutritionInfo.h"
#include "model/Recipe.h"

// ============================================================================
// C 模块（菜谱与营养分析）正式接口，来自《公共接口文档 V2.0》。
// 函数名、参数类型、返回类型冻结，不得修改。
//
// 阶段一已实现：findRecipesByFoods。
// 以下三个接口已声明（冻结签名），在后续阶段实现：
//   calculateMatch / calculateNutrition / recordMeal
// ============================================================================

// 根据库存食材查询可考虑制作的菜谱。
// 返回完整匹配与部分匹配的菜谱：完整匹配（食材齐全）排在前面，
// 其后按食材覆盖率从高到低排列；覆盖率相同按菜谱 id 升序。
// 库存中 amount <= 0 的食材视为不可用；空库存返回空列表。
std::vector<Recipe> findRecipesByFoods(
    const std::vector<FoodBatch>& foods
);

// 【后续阶段实现】计算菜谱的食材满足率，并列出已有和缺少的食材。
MatchResult calculateMatch(
    const Recipe& recipe,
    const std::vector<FoodBatch>& inventory
);

// 计算指定份数的菜谱营养信息。
// 约定：RecipeIngredient.amountGrams 表示一人份的食材重量；
//       calculateNutrition(recipe, servings) = 一人份营养 × servings。
//       servings <= 0 视为非法，返回零营养值；
//       amountGrams <= 0 的食材不参与计算（避免负营养）。
// 每 100 克营养数据取自 ingredient.nutritionPer100g。
NutritionInfo calculateNutrition(
    const Recipe& recipe,
    int servings
);

// 记录一条用餐信息，供后续饮食统计使用。
// 内部复用 calculateNutrition 计算本次用餐总营养，
// 并通过 MealRecordStorage 追加写入本地文本文件（管道分隔）。
// time 格式约定为 YYYY-MM-DD HH:mm:ss（前 10 字符作为日期查询键）。
// servings <= 0 视为非法，不产生记录。
// 返回 void：写入失败可通过 MealRecordStorage::getLastError() 诊断。
void recordMeal(
    const Recipe& recipe,
    int servings,
    const std::string& time
);
