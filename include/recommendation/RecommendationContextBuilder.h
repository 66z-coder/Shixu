#pragma once

#include <QDate>
#include <QList>
#include "model/FoodCandidate.h"
#include "recommendation/RecommendationEngine.h"

class InventoryManager;

namespace Shixu::RecommendationEngine {

// Optional measured conversion for a specific food and unit, e.g. one egg.
// D never invents a weight for "个", "盒" or "袋".
struct FoodUnitConversion {
    QString foodName;
    QString unit;
    double gramsPerUnit = 0.0;
};

struct ContextBuildResult {
    UserContext context;
    QStringList warnings;
    int usableBatches = 0;
    int expiredBatches = 0;
    int unconvertedBatches = 0;
};

// Retains preferences but replaces inventory and expiringIngredients.
// Expired/nonpositive/invalid quantities are excluded. Invalid expiryDate means
// unknown: the batch remains usable but receives no expiry-priority bonus.
ContextBuildResult buildUserContext(
    const QList<FoodCandidate>& inventory,
    const UserContext& preferences = {},
    const QDate& referenceDate = QDate::currentDate(),
    int expiringDays = 3,
    const QList<FoodUnitConversion>& conversions = {});

// E can own one InventoryManager and use this helper before calling recommend.
ContextBuildResult readUserContext(
    const InventoryManager& inventory,
    const UserContext& preferences = {},
    const QDate& referenceDate = QDate::currentDate(),
    int expiringDays = 3,
    const QList<FoodUnitConversion>& conversions = {});

} // namespace Shixu::RecommendationEngine
