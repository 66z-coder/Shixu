#include "contracts/RecommendationContract.h"
#include "model/FoodCandidate.h"
#include "ContractFixtures.h"
#include <type_traits>
#include <set>
#include <cmath>
#include <iostream>
#include "recognition/RecognitionService.h"
#include "RecipeData.h"
using namespace Shixu::Contracts::V1;
using Shixu::RecommendationEngine::UserContext;
using Shixu::RecommendationEngine::Recommendation;
static_assert(std::is_abstract_v<IRecommendationService>);
static_assert(std::is_same_v<decltype(&IRecommendationService::recommend),
    RecommendationResult (IRecommendationService::*)(const UserContext&) const>);
static_assert(std::is_same_v<decltype(FoodCandidate::gramsPerUnit), double>);
static_assert(std::is_same_v<decltype(FoodCandidate::totalWeightGrams), double>);
struct RecommendationAdapter final : IRecommendationService {
    RecommendationResult recommend(const UserContext&) const override { return {}; }
};
static_assert(!std::is_abstract_v<RecommendationAdapter>);
static_assert(std::is_same_v<decltype(&createRecommendationService),
    std::unique_ptr<IRecommendationService>(*)(IRecipeService&, IInventoryService&)>);
int main() {
    RecognitionService normalization;
    std::set<int> ids;
    const auto recipes = loadBuiltinRecipes();
    if (recipes.empty()) return 2;
    for (const auto& recipe : recipes) {
        if (recipe.id <= 0 || !ids.insert(recipe.id).second || recipe.cookingTimeMinutes <= 0 || recipe.ingredients.empty()) return 3;
        for (const auto& ingredient : recipe.ingredients) {
            const auto name = QString::fromUtf8(ingredient.foodName.c_str());
            if (name.isEmpty() || normalization.normalizeFoodName(name) != name) {
                std::cerr << "A/C canonical name mismatch: " << ingredient.foodName << '\n';
                return 4;
            }
            if (!std::isfinite(ingredient.amountGrams) || ingredient.amountGrams <= 0) return 5;
            const auto& n = ingredient.nutritionPer100g;
            for (double value : {n.calories, n.protein, n.carbohydrates, n.fat})
                if (!std::isfinite(value) || value < 0) return 6;
        }
    }
    if (normalization.normalizeFoodName(QString::fromUtf8("西红柿")) != QString::fromUtf8("番茄")) return 7;

    UserContext context;
    context.preferences = RecommendationPreferences{Mood::Tired, 30, InventoryPolicy::OnlyInventory,
        Refinement::NoNoodles, {9002}, 1, "2026-10-28"};
    context.inventorySnapshot = Shixu::ContractFixtures::tomatoes();
    Recommendation card;
    card.recipe = Shixu::ContractFixtures::cooking().recipe;
    card.recipeId = card.recipe.id;
    card.role = CardRole::Primary;
    card.availability = Shixu::ContractFixtures::expectedTomatoResult().availability;
    card.reasons = {QString::fromUtf8("已知食材充足")};
    RecommendationResult result{{OperationStatus::Succeeded, ErrorCode::None, {}}, {card}, false};
    FoodCandidate input;
    input.name = QString::fromUtf8("番茄"); input.quantity = 10; input.gramsPerUnit = 150;
    input.totalWeightGrams = 1500;
    return (context.preferences->excludedRecipeIds.front() == 9002 &&
            result.cards.front().recipeId == 9001 &&
            input.totalWeightGrams == context.inventorySnapshot.batches.front().totalWeightGrams) ? 0 : 1;
}
