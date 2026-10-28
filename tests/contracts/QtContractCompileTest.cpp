#include "contracts/RecommendationContract.h"
#include "model/FoodCandidate.h"
#include "ContractFixtures.h"
#include <type_traits>
using namespace Shixu::Contracts::V1;
using Shixu::RecommendationEngine::UserContext;
using Shixu::RecommendationEngine::Recommendation;
static_assert(std::is_abstract_v<IRecommendationService>);
static_assert(std::is_same_v<decltype(&IRecommendationService::recommend),
    RecommendationResult (IRecommendationService::*)(const UserContext&) const>);
static_assert(std::is_same_v<decltype(FoodCandidate::gramsPerUnit), double>);
static_assert(std::is_same_v<decltype(FoodCandidate::totalWeightGrams), double>);
int main() {
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
