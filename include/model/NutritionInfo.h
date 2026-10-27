#pragma once

// 营养信息（公共接口文档 V2.0：NutritionInfo calculateNutrition(...) 的返回类型）。
// 单位约定：calories 为千卡（kcal）；protein / carbohydrates / fat 为克（g）。
// 菜谱总营养 = 各食材 (amountGrams / 100) * nutritionPer100g 之和（后续阶段实现）。
// 当前示例数据为估算值，参考常见食物成分表的常见数值，非精确实测。
struct NutritionInfo
{
    double calories = 0.0;      // 能量，千卡（kcal）
    double protein = 0.0;       // 蛋白质，克（g）
    double carbohydrates = 0.0; // 碳水化合物，克（g）
    double fat = 0.0;           // 脂肪，克（g）
};
