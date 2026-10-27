#pragma once

#include <string>
#include <vector>

// calculateMatch 的返回类型（公共接口文档 V2.0）。
struct MatchResult
{
    double matchRate = 0.0; // 食材满足率，百分比 0.0–100.0，保留两位小数
    std::vector<std::string> availableIngredients; // 库存中已满足需求的食材名称
    std::vector<std::string> missingIngredients;   // 缺少或数量不足的食材名称
};
