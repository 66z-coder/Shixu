#include "ContractFixtures.h"
#include "model/FoodBatch.h"
#include "recipe/RecipeService.h"
#include <cmath>
#include <iostream>
#include <type_traits>
using namespace Shixu::Contracts::V1;
namespace F = Shixu::ContractFixtures;
static_assert(std::is_same_v<decltype(Recipe::id), RecipeId>);
static_assert(std::is_same_v<decltype(RecommendationPreferences::excludedRecipeIds), std::vector<int>>);
static_assert(std::is_abstract_v<IInventoryService> && std::is_abstract_v<IRecipeService>);
static_assert(std::is_abstract_v<IRewardService> && std::is_abstract_v<ICookingCompletionService>);
static_assert(std::is_same_v<decltype(&IInventoryService::consumeForCooking),
    ConsumptionResult (IInventoryService::*)(const ConsumptionRequest&, const std::string&)>);
static_assert(std::is_same_v<decltype(&IRecipeService::recordMealOnce),
    MealWriteResult (IRecipeService::*)(const MealWriteRequest&)>);
static_assert(std::is_same_v<decltype(&ICookingCompletionService::retryCompletion),
    CookingCompletionResult (ICookingCompletionService::*)(const EventId&)>);
int main() {
    const auto batch = F::tomatoes().batches.front();
    const auto result = F::expectedTomatoResult();
    const auto used = result.consumed.front();
    const auto snapshot = F::cooking();
    const auto recorded = F::meal();
    auto require = [](bool condition, const char* message) {
        if (!condition) std::cerr << message << '\n';
        return condition;
    };
    bool ok = require(batch.quantity * batch.gramsPerUnit == batch.totalWeightGrams, "fixture weight");
    ok &= require(used.weightBeforeGrams - used.consumedGrams == used.weightAfterGrams, "fixture deduction");
    ok &= require(used.quantityAfter * batch.gramsPerUnit == used.weightAfterGrams, "fixture count");
    ok &= require(recorded.eventId == snapshot.request.eventId && recorded.record.recipeId == snapshot.recipe.id,
                  "cross-module event/id");
    ok &= require(recorded.record.date == recorded.record.time.substr(0, 10), "frozen date");
    const auto calculated = calculateNutrition(snapshot.recipe, 1); // Existing real C formula.
    ok &= require(std::abs(calculated.calories - snapshot.nutrition.calories) < 1e-6 &&
                  std::abs(calculated.protein - snapshot.nutrition.protein) < 1e-6 &&
                  std::abs(calculated.carbohydrates - snapshot.nutrition.carbohydrates) < 1e-6 &&
                  std::abs(calculated.fat - snapshot.nutrition.fat) < 1e-6, "C nutrition compatibility");
    ok &= require(F::unknownEggs().state == AvailabilityState::Unknown && F::unknownEggs().knownGapGrams == 50.0,
                  "fixture unknown vs missing");
    ok &= require(F::expectedFirstReward().pointsAdded == 5 + 10 + 15, "fixture reward");
    if (ok) std::cout << "Contract signatures and fixtures pass (not business integration).\n";
    return ok ? 0 : 1;
}
