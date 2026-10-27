#include "recommendation/RecommendationContextBuilder.h"
#include "inventory.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Shixu::RecommendationEngine;

namespace {
int checks = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}
FoodCandidate food(const QString& name, double quantity, const QString& unit, QDate date = {})
{
    FoodCandidate value;
    value.name = name;
    value.quantity = quantity;
    value.unit = unit;
    value.expiryDate = date;
    return value;
}
double gramsOf(const UserContext& context, const std::string& name)
{
    double total = 0.0;
    for (const auto& batch : context.inventory) {
        if (batch.foodName == name) {
            total += batch.amount;
        }
    }
    return total;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString oldDirectory = QDir::currentPath();
    QTemporaryDir temporary;
    try {
        check(temporary.isValid() && QDir::setCurrent(temporary.path()), "Test database must be isolated from the user's data");
        const QDate today(2026, 10, 27);
        {
            InventoryManager inventory;
            check(inventory.getAllItems().isEmpty(), "New B SQLite inventory must be empty");
            check(recommend(readUserContext(inventory, {}, today).context).empty(), "Empty B inventory must yield no candidates");
            inventory.addFood(food(QStringLiteral("番茄"), 0.3, QStringLiteral("kg"), today.addDays(1)));
            inventory.addFood(food(QStringLiteral("鸡蛋"), 200, QStringLiteral("g")));
            inventory.addFood(food(QStringLiteral("食用油"), 50000, QStringLiteral("mg"), today.addDays(10)));
            inventory.addFood(food(QStringLiteral("鸡胸肉"), 300, QStringLiteral("g"), today.addDays(-1)));
            inventory.addFood(food(QStringLiteral("米饭"), 7, QStringLiteral("个")));
            check(inventory.getAllItems().size() == 5, "All five valid inputs must round-trip through B's SQLite implementation");
            check(inventory.getExpiredItems(today).size() == 1, "B's expired query must identify the old batch");
            check(inventory.getExpiringItems(today, 3).size() == 1, "B's expiring query must agree with adapter boundaries");

            const auto built = readUserContext(inventory, {}, today);
            check(built.usableBatches == 3 && built.expiredBatches == 1 && built.unconvertedBatches == 1,
                  "Adapter must distinguish usable, expired and unconvertible batches");
            check(std::abs(gramsOf(built.context, "番茄") - 300.0) < 0.001, "kg must convert to grams");
            check(std::abs(gramsOf(built.context, "食用油") - 50.0) < 0.001, "mg must convert to grams");
            check(gramsOf(built.context, "鸡胸肉") == 0.0, "Expired food must not be available for C matching");
            check(gramsOf(built.context, "米饭") == 0.0 && !built.warnings.isEmpty(), "Unknown count units must not silently become grams");
            check(built.context.expiringIngredients == QStringList{QStringLiteral("番茄")}, "Unknown expiry must remain usable without an urgency bonus");
            const auto recommendations = recommend(built.context);
            bool matched = false;
            for (const auto& item : recommendations) {
                if (item.recipeId == 1) {
                    matched = item.ingredientMatchScore == 100.0 && item.nutrition.calories > 0.0;
                }
            }
            check(matched, "Actual B SQLite -> FoodBatch -> C matching/nutrition -> D must return the expected dish");

            const auto converted = readUserContext(inventory, {}, today, 3,
                {{QStringLiteral("米饭"), QStringLiteral("个"), 15.0}});
            check(converted.unconvertedBatches == 0 && gramsOf(converted.context, "米饭") == 105.0,
                  "An explicit measured food/unit conversion should enable the count unit");
        }
        {
            InventoryManager reopened;
            check(reopened.getAllItems().size() == 5, "B records must persist after close and reopen");
        }

        const QList<FoodCandidate> dates{
            food(QStringLiteral("today"), 1, QStringLiteral("g"), today),
            food(QStringLiteral("edge"), 1, QStringLiteral("g"), today.addDays(3)),
            food(QStringLiteral("later"), 1, QStringLiteral("g"), today.addDays(4)),
            food(QStringLiteral("unknown"), 1, QStringLiteral("g")),
            food(QStringLiteral("old"), 1, QStringLiteral("g"), today.addDays(-1))};
        const auto boundaries = buildUserContext(dates, {}, today);
        check(boundaries.context.expiringIngredients.size() == 2, "Expiry range must include today and the threshold date only");
        check(boundaries.usableBatches == 4 && boundaries.expiredBatches == 1, "Unknown dates must not be considered expired");
        check(buildUserContext(dates, {}, QDate()).context.inventory.empty(), "An invalid reference date must not produce misleading inventory");

        const auto invalid = buildUserContext({food(QStringLiteral("bad"), std::numeric_limits<double>::quiet_NaN(), QStringLiteral("g")),
                                               food(QStringLiteral("zero"), 0, QStringLiteral("g"))}, {}, today);
        check(invalid.context.inventory.empty() && !invalid.warnings.isEmpty(), "Invalid amounts must be excluded and reported");
        const auto batches = buildUserContext({food(QStringLiteral("番茄"), 150, QStringLiteral("g")),
                                               food(QStringLiteral("番茄"), 150, QStringLiteral("g")),
                                               food(QStringLiteral("鸡蛋"), 150, QStringLiteral("g")),
                                               food(QStringLiteral("食用油"), 15, QStringLiteral("g"))}, {}, today);
        check(gramsOf(batches.context, "番茄") == 300.0, "Multiple B batches must remain available for C quantity aggregation");
        UserContext preferences;
        preferences.topK = 2;
        preferences.servings = 3;
        preferences.excludedIngredients = {QStringLiteral("花生")};
        const auto kept = buildUserContext(dates, preferences, today);
        check(kept.context.topK == 2 && kept.context.servings == 3
              && kept.context.excludedIngredients == preferences.excludedIngredients, "E's settings must survive inventory conversion");

        QDir::setCurrent(oldDirectory);
        std::cout << "Inventory/C/D integration checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        QDir::setCurrent(oldDirectory);
        std::cerr << "Inventory integration test failed: " << error.what() << '\n';
        return 1;
    }
}
