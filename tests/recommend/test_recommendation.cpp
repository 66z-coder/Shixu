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
        context.mood = Mood::Neutral;
        context.inventory = {{"", "番茄", std::numeric_limits<double>::infinity()}};
        check(recommend(context).empty(), "Infinite pantry amounts must not count as available");

        context = makeDemoUserContext();
        context.maxPreparationMinutes = 0;
        std::vector<Recipe> candidates;
        for (int id = 1; id <= 40; ++id) {
            Recipe recipe;
            recipe.id = id;
            recipe.name = "ranking-case-" + std::to_string(id);
            recipe.cookingTimeMinutes = (id % 4 + 1) * 5;
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
        // Mood is explicitly selected by E; it is never inferred or defaulted.
        context = makeDemoUserContext();
        context.mood = Mood::NotSelected;
        check(recommend(context).empty(), "Missing mood must prevent recommendation");
        check(getTopKRecommendations(candidates, context, 3).empty(), "Missing mood must also prevent direct Top-K calls");
        check(generateRecommendation(candidates.front(), context).recipeId == 0, "Missing mood must prevent direct single-dish scoring");
        context.mood = static_cast<Mood>(999);
        check(effectivePreparationTimeLimit(context) == 0 && recommend(context).empty(), "Invalid enum values must not silently select a time budget");
        check(moodTimeLimitMinutes(Mood::ReadyToCook) == 60, "Ready mood cap must be 60 minutes");
        check(moodTimeLimitMinutes(Mood::Neutral) == 30, "Neutral mood cap must be 30 minutes");
        check(moodTimeLimitMinutes(Mood::Tired) == 15, "Tired mood cap must be 15 minutes");

        std::vector<Recipe> durations;
        int nextId = 100;
        for (int minutes : {10, 15, 16, 20, 30, 31, 60, 61, 0, -1}) {
            Recipe recipe;
            recipe.id = nextId++;
            recipe.name = "duration-case-" + std::to_string(minutes);
            recipe.cookingTimeMinutes = minutes;
            recipe.ingredients = {{"番茄", 100.0, {20.0, 0.9, 4.0, 0.2}}};
            durations.push_back(recipe);
        }
        context = makeDemoUserContext();
        context.maxPreparationMinutes = 0;
        for (const auto& scenario : std::vector<std::pair<Mood, int>>{
                 {Mood::ReadyToCook, 7}, {Mood::Neutral, 5}, {Mood::Tired, 2}}) {
            context.mood = scenario.first;
            const auto selected = getTopKRecommendations(durations, context, 100);
            check(static_cast<int>(selected.size()) == scenario.second, "Mood duration boundary must include the cap and exclude longer/unknown durations");
            for (const auto& item : selected) {
                check(item.recipe.cookingTimeMinutes > 0 && item.recipe.cookingTimeMinutes <= moodTimeLimitMinutes(context.mood),
                      "Every selected recipe must have a known duration within the mood cap");
                const auto reason = explainRecommendation(item);
                check(item.mood == context.mood && reason.mood == context.mood
                      && reason.effectiveMaxPreparationMinutes == moodTimeLimitMinutes(context.mood),
                      "Displayed explanation must retain the mood and actual filtering cap");
            }
        }
        context.mood = Mood::ReadyToCook;
        context.maxPreparationMinutes = 20;
        check(effectivePreparationTimeLimit(context) == 20, "Shorter user cap must win");
        check(getTopKRecommendations(durations, context, 100).size() == 4, "User cap must constrain results as well as mood");
        context.mood = Mood::Tired;
        context.maxPreparationMinutes = 90;
        check(effectivePreparationTimeLimit(context) == 15, "Longer user cap must not relax a tired mood's cap");
        context.maxPreparationMinutes = -1;
        check(effectivePreparationTimeLimit(context) == 15, "Nonpositive user cap must still retain the mood cap");

        context.mood = Mood::Neutral;
        context.maxPreparationMinutes = 0;
        const auto shortDish = generateRecommendation(durations[0], context);
        const auto longerDish = generateRecommendation(durations[1], context);
        check(shortDish.preparationTimeScore > longerDish.preparationTimeScore && shortDish.totalScore > longerDish.totalScore,
              "Within the same mood budget, shorter otherwise-identical dishes should score higher");
        Recipe alternateTags = durations[0];
        alternateTags.tags = {"清淡", "家常"};
        check(std::abs(generateRecommendation(alternateTags, context).totalScore - shortDish.totalScore) < 0.001,
              "Former taste tags must no longer affect the recommendation score");
        context.targetCaloriesPerServing = 20.0;
        const auto weightedDish = generateRecommendation(durations[0], context);
        const double expectedScore = 0.35 * weightedDish.ingredientMatchScore
            + 0.25 * weightedDish.expiryUrgencyScore + 0.20 * weightedDish.moodMatchScore
            + 0.05 * weightedDish.preparationTimeScore
            + 0.15 * weightedDish.nutritionScore;
        check(std::abs(weightedDish.totalScore - expectedScore) < 0.001, "Mood 20 percent and time 5 percent must remain independent weighted factors");
        // Changing mood changes the acceptable candidate set, not the
        // independent speed score for the same dish and user time setting.
        context.targetCaloriesPerServing = 0.0;
        context.maxPreparationMinutes = 0;
        context.mood = Mood::Neutral;
        const auto neutralDish = generateRecommendation(durations[0], context);
        context.mood = Mood::Tired;
        const auto tiredDish = generateRecommendation(durations[0], context);
        check(neutralDish.moodMatchScore == 100.0 && tiredDish.moodMatchScore == 100.0,
              "All retained dishes satisfy their selected mood time cap");
        check(std::abs(neutralDish.preparationTimeScore - tiredDish.preparationTimeScore) < 0.001,
              "Changing mood alone must not change the separate speed score");
        check(tiredDish.preparationTimeReferenceMinutes == DefaultTimeScoringReferenceMinutes,
              "No user time cap must use the independent default speed reference");
        const auto separated = explainRecommendation(tiredDish);
        check(separated.moodMatchScore == tiredDish.moodMatchScore
              && separated.preparationTimeScore == tiredDish.preparationTimeScore
              && separated.preparationTimeReferenceMinutes == tiredDish.preparationTimeReferenceMinutes,
              "Explanation must retain separate mood and speed factors");
        context.maxPreparationMinutes = 20;
        const auto userBudget = generateRecommendation(durations[0], context);
        check(userBudget.effectiveMaxPreparationMinutes == 15
              && userBudget.preparationTimeReferenceMinutes == 20
              && std::abs(userBudget.preparationTimeScore - 50.0) < 0.001,
              "Mood filtering cap and independent user speed reference must remain different");
        std::cout << "Recommendation checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Recommendation test failed: " << error.what() << '\n';
        return 1;
    }
}
