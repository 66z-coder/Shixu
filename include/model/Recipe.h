#pragma once

#include <string>
#include <vector>

#include "model/RecipeIngredient.h"

// 菜谱（公共接口文档 V2.0：C 模块接口的核心类型）。
// id 使用整型，与 E 模块 completeCooking(int recipeId, int servings) 的约定对齐。
struct Recipe
{
    int id = 0;
    std::string name;
    // 主料与辅料；盐、糖、酱油等家常调味品不计入食材匹配（约定，见阶段报告）。
    std::vector<RecipeIngredient> ingredients;
    std::vector<std::string> steps; // 制作步骤
    int cookingTimeMinutes = 0;     // 预计制作时间，分钟
    std::string category;           // 分类，如 荤菜 / 素菜 / 汤类 / 主食 / 水产
    std::vector<std::string> tags;  // 标签，如 家常 / 快手 / 清淡 / 高蛋白
};
