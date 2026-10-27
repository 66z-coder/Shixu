#pragma once

#include <string>

#include "model/NutritionInfo.h"

// 菜谱中的一种食材及其用量。
// 内部计算统一以克（g）为单位。
struct RecipeIngredient
{
    std::string foodName;           // 食材名称，与库存 FoodBatch::foodName 对齐
    double amountGrams = 0.0;       // 用量，克（g）
    NutritionInfo nutritionPer100g; // 每 100 克的营养含量（估算值）
};
