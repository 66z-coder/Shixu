#pragma once

#include <string>

#include "model/NutritionInfo.h"

// ============================================================================
// 用餐记录（MealRecord）。
// 仅供 C 模块内部及其下游（例如 E 的统计模块）使用。
// 不是团队公共接口文档 V2.0 冻结的类型。
// ============================================================================
struct MealRecord
{
    int recipeId = 0;
    std::string recipeName;
    int servings = 0;
    // 完整的用餐时间字符串（传入 recordMeal 时原样保存）。
    std::string time;
    // 从 time 中提取的日期（前 10 字符，格式 YYYY-MM-DD），用于按日期查询。
    std::string date;
    // 本次用餐对应的总营养（复用 NutritionInfo）。
    NutritionInfo nutrition;
};
