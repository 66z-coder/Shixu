#include "recommendation/RecommendationEngine.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <utility>

namespace Shixu::RecommendationEngine {
namespace {

QString normalized(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

bool containsText(const QStringList& values, const QString& target)
{
    const QString key = normalized(target);
    return std::any_of(values.cbegin(), values.cend(), [&](const QString& value) {
        return normalized(value) == key;
    });
}

double ratioScore(int matches, int total)
{
    if (total <= 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(matches) / static_cast<double>(total);
}

bool isFilteredOut(const RecipeCandidate& candidate, const UserContext& context)
{
    if (candidate.name.trimmed().isEmpty()) {
        return true;
    }
    if (context.maxPreparationMinutes > 0 && candidate.preparationMinutes > 0
        && candidate.preparationMinutes > context.maxPreparationMinutes) {
        return true;
    }
    return std::any_of(candidate.ingredients.cbegin(), candidate.ingredients.cend(),
                       [&](const QString& ingredient) {
        return containsText(context.excludedIngredients, ingredient);
    });
}

// The priority queue keeps the currently lowest-ranked retained item at top,
// so a better item can replace it in O(log k).
struct LowerScoreAtTop {
    bool operator()(const Recommendation& left, const Recommendation& right) const
    {
        if (left.totalScore != right.totalScore) {
            return left.totalScore > right.totalScore;
        }
        return left.dishName < right.dishName;
    }
};

} // namespace

Recommendation generateRecommendation(const RecipeCandidate& candidate,
                                      const UserContext& context)
{
    Recommendation result;
    result.recipeId = candidate.id;
    result.dishName = candidate.name.trimmed();

    int availableMatches = 0;
    int expiringMatches = 0;
    for (const QString& ingredient : candidate.ingredients) {
        if (containsText(context.availableIngredients, ingredient)) {
            ++availableMatches;
        }
        if (containsText(context.expiringIngredients, ingredient)) {
            ++expiringMatches;
        }
    }

    int preferredMatches = 0;
    for (const QString& tag : candidate.tags) {
        if (containsText(context.preferredTags, tag)) {
            ++preferredMatches;
        }
    }

    result.ingredientMatchScore = ratioScore(availableMatches, candidate.ingredients.size());
    result.expiryUrgencyScore = ratioScore(expiringMatches, candidate.ingredients.size());
    result.preferenceMatchScore = ratioScore(preferredMatches, candidate.tags.size());
    result.nutritionScore = std::isfinite(candidate.nutritionScore)
        ? std::clamp(candidate.nutritionScore, 0.0, 100.0)
        : 0.0;

    if (context.maxPreparationMinutes > 0 && candidate.preparationMinutes > 0) {
        const double timeRatio = static_cast<double>(candidate.preparationMinutes)
            / static_cast<double>(context.maxPreparationMinutes);
        result.preparationTimeScore = std::clamp(100.0 * (1.0 - timeRatio), 0.0, 100.0);
    } else {
        result.preparationTimeScore = 50.0;
    }

    // Initial tunable weights: pantry match 35%, expiry 25%, preferences 20%,
    // nutrition 15%, and preparation time 5%.
    result.totalScore = 0.35 * result.ingredientMatchScore
        + 0.25 * result.expiryUrgencyScore
        + 0.20 * result.preferenceMatchScore
        + 0.15 * result.nutritionScore
        + 0.05 * result.preparationTimeScore;
    return result;
}

std::vector<Recommendation> getTopKRecommendations(
    const std::vector<RecipeCandidate>& candidates,
    const UserContext& context,
    int k)
{
    if (k <= 0) {
        return {};
    }

    std::priority_queue<Recommendation, std::vector<Recommendation>, LowerScoreAtTop> top;
    for (const RecipeCandidate& candidate : candidates) {
        if (isFilteredOut(candidate, context)) {
            continue;
        }

        Recommendation current = generateRecommendation(candidate, context);
        if (static_cast<int>(top.size()) < k) {
            top.push(std::move(current));
        } else if (current.totalScore > top.top().totalScore
                   || (current.totalScore == top.top().totalScore
                       && current.dishName < top.top().dishName)) {
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
    std::sort(results.begin(), results.end(), [](const Recommendation& left,
                                                  const Recommendation& right) {
        if (left.totalScore != right.totalScore) {
            return left.totalScore > right.totalScore;
        }
        return left.dishName < right.dishName;
    });
    return results;
}

std::vector<Recommendation> recommend(const UserContext& context)
{
    return getTopKRecommendations(context.recipeCandidates, context, context.topK);
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

    if (result.ingredientMatchScore >= 50.0) {
        result.reasons.append(QStringLiteral("现有食材匹配度较高"));
    }
    if (result.expiryUrgencyScore > 0.0) {
        result.reasons.append(QStringLiteral("可以优先利用临近到期的食材"));
    }
    if (result.preferenceMatchScore > 0.0) {
        result.reasons.append(QStringLiteral("符合你的口味偏好"));
    }
    if (result.nutritionScore >= 70.0) {
        result.reasons.append(QStringLiteral("营养评分较高"));
    }
    if (result.preparationTimeScore >= 50.0) {
        result.reasons.append(QStringLiteral("所需准备时间符合当前设置"));
    }
    if (result.reasons.isEmpty()) {
        result.reasons.append(QStringLiteral("综合评分较高，已进入推荐列表"));
    }
    return result;
}

UserContext makeDemoUserContext()
{
    UserContext context;
    context.availableIngredients = {QStringLiteral("鸡蛋"), QStringLiteral("番茄"),
                                    QStringLiteral("胡萝卜"), QStringLiteral("米饭")};
    context.expiringIngredients = {QStringLiteral("番茄")};
    context.excludedIngredients = {QStringLiteral("花生")};
    context.preferredTags = {QStringLiteral("家常"), QStringLiteral("清淡")};
    context.maxPreparationMinutes = 40;
    context.topK = 3;
    context.recipeCandidates = {
        {QStringLiteral("dish-001"), QStringLiteral("番茄炒蛋"),
         {QStringLiteral("番茄"), QStringLiteral("鸡蛋")},
         {QStringLiteral("家常"), QStringLiteral("快手")}, 15, 82.0},
        {QStringLiteral("dish-002"), QStringLiteral("胡萝卜炒饭"),
         {QStringLiteral("胡萝卜"), QStringLiteral("米饭"), QStringLiteral("鸡蛋")},
         {QStringLiteral("家常"), QStringLiteral("清淡")}, 25, 76.0},
        {QStringLiteral("dish-003"), QStringLiteral("花生拌面"),
         {QStringLiteral("面条"), QStringLiteral("花生")},
         {QStringLiteral("家常")}, 20, 68.0},
        {QStringLiteral("dish-004"), QStringLiteral("慢炖蔬菜汤"),
         {QStringLiteral("胡萝卜"), QStringLiteral("土豆")},
         {QStringLiteral("清淡")}, 70, 90.0}
    };
    return context;
}

} // namespace Shixu::RecommendationEngine
