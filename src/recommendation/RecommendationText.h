#pragma once

#include <QString>
#include <QStringList>
#include "recommendation/RecommendationEngine.h"

inline QString recommendationText(
    const std::vector<Shixu::RecommendationEngine::Recommendation>& recommendations)
{
    using namespace Shixu::RecommendationEngine;
    if (recommendations.empty()) {
        return QStringLiteral("没有符合当前库存、忌口和时间条件的菜谱。\n请先添加可用库存，并检查单位是否能换算为克。");
    }
    QStringList lines;
    for (std::size_t index = 0; index < recommendations.size(); ++index) {
        const Recommendation& item = recommendations[index];
        const ExplainResult explanation = explainRecommendation(item);
        lines << QStringLiteral("%1. %2（菜谱 ID %3，%4 份）  综合得分：%5/100")
                     .arg(index + 1).arg(item.dishName).arg(item.recipeId).arg(item.servings)
                     .arg(item.totalScore, 0, 'f', 1);
        lines << QStringLiteral("食材数量满足率 %1 | 临期利用 %2 | 口味偏好 %3 | 时间 %4")
                     .arg(item.ingredientMatchScore, 0, 'f', 1)
                     .arg(item.expiryUrgencyScore, 0, 'f', 1)
                     .arg(item.preferenceMatchScore, 0, 'f', 1)
                     .arg(item.preparationTimeScore, 0, 'f', 1);
        lines << QStringLiteral("本次份数的估算营养：%1 kcal，蛋白质 %2 g，碳水 %3 g，脂肪 %4 g")
                     .arg(item.nutrition.calories, 0, 'f', 1)
                     .arg(item.nutrition.protein, 0, 'f', 1)
                     .arg(item.nutrition.carbohydrates, 0, 'f', 1)
                     .arg(item.nutrition.fat, 0, 'f', 1);
        if (!item.nutritionScoreAvailable) {
            lines << QStringLiteral("未设定能量目标或营养数据不可用，能量目标匹配项未参与评分。");
        }
        lines << QStringLiteral("推荐理由：%1").arg(explanation.reasons.join(QStringLiteral("；")));
        lines << QString();
    }
    return lines.join(QLatin1Char('\n'));
}
