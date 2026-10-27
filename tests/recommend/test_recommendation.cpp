#include "recommendation/RecommendationEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "recipe/RecipeService.h"

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
const Recommendation& findId(const std::vector<Recommendation>& results, int id)
{
    const auto found = std::find_if(results.begin(), results.end(),
                                    [id](const Recommendation& value) { return value.recipeId == id; });
    if (found == results.end()) {
        throw std::runtime_error("Expected C recipe ID was not returned");
    }
    return *found;
}
}

int main()
{
    try {
        UserContext context = makeDemoUserContext();
        context.topK = 30;
        auto results = recommend(context);
        check(!results.empty(), "C's recipe service should return candidates for pantry data");
        const auto dish = findId(results, 1);
        check(dish.recipe.id == 1 && dish.recipe.name == "番茄炒蛋", "C's complete recipe and integer ID must be retained");
        check(dish.ingredientMatchScore == 100.0, "Sufficient grams should match the full recipe");
        check(std::abs(dish.nutrition.calories - 390.85) < 0.001, "Nutrition must use C's real ingredient calculation");
        check(!dish.nutritionScoreAvailable, "Missing energy target must not create a fake nutrition score");
        check(dish.expiryUrgencyScore > 0.0, "Known expiring ingredients should receive a bonus");
        check(explainRecommendation(dish).missingIngredients.isEmpty(), "Full-match explanation must not list missing foods");

        context.servings = 2;
        const auto doubled = findId(recommend(context), 1);
        check(std::abs(doubled.ingredientMatchScore - 33.33) < 0.001, "Two servings must require twice the ingredient quantities");
        check(std::abs(doubled.nutrition.calories - 781.7) < 0.001, "Two servings must double nutrition");
        check(doubled.missingIngredients.contains(QStringLiteral("番茄")), "Quantity shortage must appear in the missing list");
        check(explainRecommendation(doubled).missingIngredients == doubled.missingIngredients, "Explanation must preserve shortage information");

        context.servings = 1;
        context.targetCaloriesPerServing = 390.85;
        const auto targeted = findId(recommend(context), 1);
        check(targeted.nutritionScoreAvailable && std::abs(targeted.nutritionScore - 100.0) < 0.001,
              "An explicit matching energy target should score 100");
        context.targetCaloriesPerServing = std::numeric_limits<double>::quiet_NaN();
        check(!findId(recommend(context), 1).nutritionScoreAvailable, "NaN target must be disabled");

        context.excludedIngredients.append(QStringLiteral(" 番茄 "));
        results = recommend(context);
        check(std::none_of(results.begin(), results.end(), [](const Recommendation& value) { return value.recipeId == 1; }),
              "Excluded ingredients must filter the entire dish");
        context.excludedIngredients.clear();
        context.maxPreparationMinutes = 10;
        results = recommend(context);
        check(!results.empty() && std::all_of(results.begin(), results.end(), [](const Recommendation& value) {
            return value.recipe.cookingTimeMinutes <= 10;
        }), "Time limit must filter slower recipes");
        context.topK = 0;
        check(recommend(context).empty(), "Zero K must return no results");
        context.topK = -1;
        check(recommend(context).empty(), "Negative K must return no results");
        context.topK = 5;
        context.servings = 0;
        check(recommend(context).empty(), "Invalid servings must not produce recommendations");
        context = {};
        check(recommend(context).empty(), "Empty pantry must not invent candidates");
        context.inventory = {{"", "番茄", std::numeric_limits<double>::infinity()}};
        check(recommend(context).empty(), "Infinite pantry amounts must not count as available");

        context = makeDemoUserContext();
        context.maxPreparationMinutes = 0;
        std::vector<Recipe> candidates;
        for (int id = 1; id <= 40; ++id) {
            Recipe recipe;
            recipe.id = id;
            recipe.name = "ranking-case-" + std::to_string(id);
            recipe.ingredients = {{"番茄", id % 2 ? 100.0 : 1000.0, {20.0, 0.9, 4.0, 0.2}}};
            recipe.tags = id % 3 ? std::vector<std::string>{"家常"} : std::vector<std::string>{};
            candidates.push_back(recipe);
        }
        std::reverse(candidates.begin(), candidates.end());
        std::vector<Recommendation> full;
        for (const auto& candidate : candidates) {
            full.push_back(generateRecommendation(candidate, context));
        }
        std::sort(full.begin(), full.end(), [](const Recommendation& left, const Recommendation& right) {
            return left.totalScore == right.totalScore ? left.recipeId < right.recipeId : left.totalScore > right.totalScore;
        });
        for (int k : {1, 3, 7, 40, 100}) {
            const auto top = getTopKRecommendations(candidates, context, k);
            check(top.size() == std::min(full.size(), static_cast<std::size_t>(k)), "Heap size must be min(K, valid candidate count)");
            for (std::size_t index = 0; index < top.size(); ++index) {
                check(top[index].recipeId == full[index].recipeId, "PriorityQueue Top-K must agree with a full sort, including ties");
                check(std::isfinite(top[index].totalScore) && top[index].totalScore >= 0.0 && top[index].totalScore <= 100.0,
                      "All scores must be finite and in range");
            }
        }
        candidates.push_back(candidates.front());
        check(getTopKRecommendations(candidates, context, 100).size() == 40, "Duplicate recipe IDs must not produce duplicate recommendations");
        std::cout << "Recommendation checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Recommendation test failed: " << error.what() << '\n';
        return 1;
    }
}
