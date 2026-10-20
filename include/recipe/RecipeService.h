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

// 【后续阶段实现】计算指定份数的菜谱营养信息。
NutritionInfo calculateNutrition(
    const Recipe& recipe,
    int servings
);

// 【后续阶段实现】记录一条用餐信息，供后续饮食统计使用。
void recordMeal(
    const Recipe& recipe,
    int servings,
    const std::string& time
);
