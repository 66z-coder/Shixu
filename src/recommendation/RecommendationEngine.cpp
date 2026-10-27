#include "recommendation/RecommendationEngine.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <utility>

#include "recipe/RecipeService.h"

namespace Shixu::RecommendationEngine {
namespace {

QString fromUtf8(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

QString normalized(const QString& text)
{
    return text.trimmed().toCaseFolded();
}

bool containsText(const QStringList& values, const QString& text)
{
    return std::any_of(values.cbegin(), values.cend(), [&](const QString& value) {
        return normalized(value) == normalized(text);
    });
}

std::vector<FoodBatch> usableInventory(const std::vector<FoodBatch>& inventory)
{
    std::vector<FoodBatch> usable;
    for (const FoodBatch& food : inventory) {
        if (!fromUtf8(food.foodName).trimmed().isEmpty()
            && std::isfinite(food.amount) && food.amount > 0.0) {
            usable.push_back(food);
        }
    }
    return usable;
}

QStringList toNames(const std::vector<std::string>& names)
{
    QStringList result;
    for (const std::string& name : names) {
        result.append(fromUtf8(name));
    }
    result.removeDuplicates();
    result.sort();
    return result;
}

bool isFilteredOut(const Recipe& recipe, const UserContext& context)
{
    if (context.servings <= 0 || recipe.id <= 0 || fromUtf8(recipe.name).trimmed().isEmpty()) {
        return true;
    }
    if (context.maxPreparationMinutes > 0
        && recipe.cookingTimeMinutes > context.maxPreparationMinutes) {
        return true;
    }
    bool hasIngredient = false;
    for (const RecipeIngredient& ingredient : recipe.ingredients) {
        if (!std::isfinite(ingredient.amountGrams)) {
            return true;
        }
        if (ingredient.amountGrams > 0.0) {
            hasIngredient = true;
            if (containsText(context.excludedIngredients, fromUtf8(ingredient.foodName))) {
                return true;
            }
        }
    }
    return !hasIngredient;
}

bool validNutrition(const NutritionInfo& nutrition)
{
    return std::isfinite(nutrition.calories) && nutrition.calories >= 0.0
        && std::isfinite(nutrition.protein) && nutrition.protein >= 0.0
        && std::isfinite(nutrition.carbohydrates) && nutrition.carbohydrates >= 0.0
        && std::isfinite(nutrition.fat) && nutrition.fat >= 0.0;
}

bool better(const Recommendation& left, const Recommendation& right)
{
    if (left.totalScore != right.totalScore) {
        return left.totalScore > right.totalScore;
    }
    return left.recipeId < right.recipeId;
}

// Using 'better' as the heap comparator leaves the worst retained result at top.
struct WorstAtTop {
    bool operator()(const Recommendation& left, const Recommendation& right) const
    {
        return better(left, right);
    }
};

} // namespace

Recommendation generateRecommendation(const Recipe& recipe, const UserContext& context)
{
    if (isFilteredOut(recipe, context)) {
        return {};
    }

    Recipe demand = recipe;
    for (RecipeIngredient& ingredient : demand.ingredients) {
        ingredient.amountGrams *= context.servings;
        if (!std::isfinite(ingredient.amountGrams)) {
            return {};
        }
    }

    const MatchResult match = calculateMatch(demand, usableInventory(context.inventory));
    Recommendation result;
    result.recipeId = recipe.id;
    result.dishName = fromUtf8(recipe.name);
    result.recipe = recipe;
    result.servings = context.servings;
    result.availableIngredients = toNames(match.availableIngredients);
    result.missingIngredients = toNames(match.missingIngredients);
    result.ingredientMatchScore = std::clamp(match.matchRate, 0.0, 100.0);

    std::set<QString> requiredNames;
    for (const RecipeIngredient& ingredient : recipe.ingredients) {
        if (ingredient.amountGrams > 0.0) {
            requiredNames.insert(normalized(fromUtf8(ingredient.foodName)));
        }
    }
    int expiring = 0;
    for (const QString& name : requiredNames) {
        if (containsText(context.expiringIngredients, name)) {
            ++expiring;
        }
    }
    result.expiryUrgencyScore = requiredNames.empty() ? 0.0
        : 100.0 * expiring / static_cast<double>(requiredNames.size());

    std::set<QString> preferences;
    for (const QString& tag : context.preferredTags) {
        if (!tag.trimmed().isEmpty()) {
            preferences.insert(normalized(tag));
        }
    }
    int preferred = 0;
    for (const QString& tag : preferences) {
        if (std::any_of(recipe.tags.cbegin(), recipe.tags.cend(), [&](const std::string& item) {
                return normalized(fromUtf8(item)) == tag;
            })) {
            ++preferred;
        }
    }
    result.preferenceMatchScore = preferences.empty() ? 0.0
        : 100.0 * preferred / static_cast<double>(preferences.size());

    result.nutrition = calculateNutrition(recipe, context.servings);
    const bool nutritionValid = validNutrition(result.nutrition);
    if (!nutritionValid) {
        result.nutrition = {};
    }
    result.nutritionScoreAvailable = nutritionValid && result.nutrition.calories > 0.0
        && std::isfinite(context.targetCaloriesPerServing)
        && context.targetCaloriesPerServing > 0.0;
    if (result.nutritionScoreAvailable) {
        const double perServing = result.nutrition.calories / context.servings;
        result.nutritionScore = std::clamp(
            100.0 * (1.0 - std::abs(perServing - context.targetCaloriesPerServing)
                              / context.targetCaloriesPerServing), 0.0, 100.0);
    }

    const bool timeScored = context.maxPreparationMinutes > 0 && recipe.cookingTimeMinutes > 0;
    if (timeScored) {
        result.preparationTimeScore = std::clamp(
            100.0 * (1.0 - static_cast<double>(recipe.cookingTimeMinutes)
                              / context.maxPreparationMinutes), 0.0, 100.0);
    }

    // Draft weights. Missing optional preferences/energy target/time are omitted
    // and the active weights are normalized; a missing value is never fabricated.
    double weight = 0.35 + 0.25;
    double weighted = 0.35 * result.ingredientMatchScore + 0.25 * result.expiryUrgencyScore;
    if (!preferences.empty()) {
        weight += 0.20;
        weighted += 0.20 * result.preferenceMatchScore;
    }
    if (result.nutritionScoreAvailable) {
        weight += 0.15;
        weighted += 0.15 * result.nutritionScore;
    }
    if (timeScored) {
        weight += 0.05;
        weighted += 0.05 * result.preparationTimeScore;
    }
    result.totalScore = std::clamp(weighted / weight, 0.0, 100.0);
    return result;
}

std::vector<Recommendation> getTopKRecommendations(
    const std::vector<Recipe>& candidates, const UserContext& context, int k)
{
    if (k <= 0 || context.servings <= 0) {
        return {};
    }
    std::priority_queue<Recommendation, std::vector<Recommendation>, WorstAtTop> top;
    std::set<int> seen;
    for (const Recipe& recipe : candidates) {
        if (isFilteredOut(recipe, context) || !seen.insert(recipe.id).second) {
            continue;
        }
        Recommendation current = generateRecommendation(recipe, context);
        if (current.recipeId <= 0) {
            continue;
        }
        if (top.size() < static_cast<std::size_t>(k)) {
            top.push(std::move(current));
        } else if (better(current, top.top())) {
            top.pop();
            top.push(std::move(current));
        }
    }
    std::vector<Recommendation> results;
    results.reserve(top.size());
    while (!top.empty()) {
        results.push_back(top.top());
        top.pop();
    }
    std::sort(results.begin(), results.end(), better);
    return results;
}

std::vector<Recommendation> recommend(const UserContext& context)
{
    if (context.topK <= 0 || context.servings <= 0) {
        return {};
    }
    const auto inventory = usableInventory(context.inventory);
    if (inventory.empty()) {
        return {};
    }
    return getTopKRecommendations(findRecipesByFoods(inventory), context, context.topK);
}

ExplainResult explainRecommendation(const Recommendation& recommendation)
{
    ExplainResult result;
    result.dishName = recommendation.dishName;
    result.totalScore = recommendation.totalScore;
    result.ingredientMatchScore = recommendation.ingredientMatchScore;
    result.expiryUrgencyScore = recommendation.expiryUrgencyScore;
    result.preferenceMatchScore = recommendation.preferenceMatchScore;
    result.nutritionScore = recommendation.nutritionScore;
    result.preparationTimeScore = recommendation.preparationTimeScore;
    result.nutritionScoreAvailable = recommendation.nutritionScoreAvailable;
    result.missingIngredients = recommendation.missingIngredients;
    if (recommendation.recipeId <= 0) {
        return result;
    }
    result.reasons.append(result.missingIngredients.isEmpty()
        ? QStringLiteral("库存数量满足当前份数所需的全部食材")
        : QStringLiteral("还需补齐食材或数量：%1").arg(result.missingIngredients.join(QStringLiteral("、"))));
    if (result.expiryUrgencyScore > 0.0) {
        result.reasons.append(QStringLiteral("可利用临近到期的库存食材"));
    }
    if (result.preferenceMatchScore > 0.0) {
        result.reasons.append(QStringLiteral("包含你偏好的菜谱标签"));
    }
    if (result.nutritionScoreAvailable) {
        result.reasons.append(QStringLiteral("能量目标匹配分：%1（使用 C 模块的估算营养数据）")
                                  .arg(result.nutritionScore, 0, 'f', 1));
    }
    if (recommendation.recipe.cookingTimeMinutes > 0) {
        result.reasons.append(QStringLiteral("预计制作时间：%1 分钟")
                                  .arg(recommendation.recipe.cookingTimeMinutes));
    }
    return result;
}

UserContext makeDemoUserContext()
{
    UserContext context;
    context.inventory = {{"", "番茄", 300.0}, {"", "鸡蛋", 200.0},
                         {"", "食用油", 50.0}, {"", "胡萝卜", 200.0}};
    context.expiringIngredients = {QStringLiteral("番茄")};
    context.excludedIngredients = {QStringLiteral("花生")};
    context.preferredTags = {QStringLiteral("家常"), QStringLiteral("清淡")};
    context.maxPreparationMinutes = 40;
    context.topK = 3;
    return context;
}

} // namespace Shixu::RecommendationEngine
