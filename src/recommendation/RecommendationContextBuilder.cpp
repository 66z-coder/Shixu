#include "recommendation/RecommendationContextBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include "inventory.h"

namespace Shixu::RecommendationEngine {
namespace {

QString normalized(const QString& text)
{
    return text.trimmed().toCaseFolded();
}

double gramsPerUnit(const FoodCandidate& food, const QList<FoodUnitConversion>& conversions)
{
    const QString unit = normalized(food.unit);
    if (unit == QStringLiteral("g") || unit == QStringLiteral("克")) {
        return 1.0;
    }
    if (unit == QStringLiteral("kg") || unit == QStringLiteral("千克") || unit == QStringLiteral("公斤")) {
        return 1000.0;
    }
    if (unit == QStringLiteral("mg") || unit == QStringLiteral("毫克")) {
        return 0.001;
    }
    for (const FoodUnitConversion& conversion : conversions) {
        if (normalized(conversion.foodName) == normalized(food.name)
            && normalized(conversion.unit) == unit
            && std::isfinite(conversion.gramsPerUnit) && conversion.gramsPerUnit > 0.0) {
            return conversion.gramsPerUnit;
        }
    }
    return 0.0;
}

} // namespace

ContextBuildResult buildUserContext(
    const QList<FoodCandidate>& inventory, const UserContext& preferences,
    const QDate& referenceDate, int expiringDays, const QList<FoodUnitConversion>& conversions)
{
    ContextBuildResult result;
    result.context = preferences;
    result.context.inventory.clear();
    result.context.expiringIngredients.clear();
    if (!referenceDate.isValid()) {
        result.warnings.append(QStringLiteral("当前日期无效，未生成库存快照。"));
        return result;
    }
    const QDate endDate = referenceDate.addDays(std::max(0, expiringDays));
    for (const FoodCandidate& food : inventory) {
        const QString name = food.name.trimmed();
        if (name.isEmpty() || !std::isfinite(food.quantity) || food.quantity <= 0.0) {
            result.warnings.append(QStringLiteral("已忽略名称为空或数量无效的库存记录。"));
            continue;
        }
        if (food.expiryDate.isValid() && food.expiryDate < referenceDate) {
            ++result.expiredBatches;
            continue;
        }
        const double factor = gramsPerUnit(food, conversions);
        const double grams = food.quantity * factor;
        if (factor <= 0.0 || !std::isfinite(grams) || grams <= 0.0) {
            ++result.unconvertedBatches;
            result.warnings.append(QStringLiteral("%1：单位“%2”无法换算为克，暂不参与数量匹配；请提供克数或实测换算。")
                                       .arg(name, food.unit));
            continue;
        }
        FoodBatch batch;
        // B's current getAllItems does not expose a stable ID. C only reads
        // foodName and amount, so no fabricated ID is needed.
        batch.foodName = name.toUtf8().toStdString();
        batch.amount = grams;
        result.context.inventory.push_back(std::move(batch));
        ++result.usableBatches;
        if (food.expiryDate.isValid() && food.expiryDate <= endDate
            && !result.context.expiringIngredients.contains(name)) {
            result.context.expiringIngredients.append(name);
        }
    }
    result.warnings.removeDuplicates();
    return result;
}

ContextBuildResult readUserContext(
    const InventoryManager& inventory, const UserContext& preferences,
    const QDate& referenceDate, int expiringDays, const QList<FoodUnitConversion>& conversions)
{
    return buildUserContext(inventory.getAllItems(), preferences, referenceDate, expiringDays, conversions);
}

} // namespace Shixu::RecommendationEngine
