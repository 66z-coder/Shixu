// Compile-only adapters: no business implementation or persistence is claimed.
#include "contracts/ServiceFactories.h"
#include "ContractFixtures.h"
#include <type_traits>
using namespace Shixu::Contracts::V1;
struct InventoryAdapter final : IInventoryService {
    QueryResult<InventorySnapshot> getInventoryBatches() const override { return {}; }
    QueryResult<std::vector<IngredientAvailability>> evaluateRequirements(const EvaluationRequest&) const override { return {}; }
    QueryResult<ConsumptionResult> getConsumption(const EventId&) const override { return {}; }
    ConsumptionPreview previewConsumption(const ConsumptionRequest&) const override { return {}; }
    ConsumptionResult consumeForCooking(const ConsumptionRequest&, const std::string&) override { return {}; }
};
struct RecipeAdapter final : IRecipeService {
    QueryResult<std::vector<Recipe>> getAllRecipes() const override { return {}; }
    QueryResult<Recipe> findRecipeById(RecipeId) const override { return {}; }
    QueryResult<std::vector<MealRecord>> getMealsByDate(const LocalDate&) const override { return {}; }
    QueryResult<NutritionInfo> getNutritionByDate(const LocalDate&) const override { return {}; }
    MealWriteResult recordMealOnce(const MealWriteRequest&) override { return {}; }
};
struct RewardAdapter final : IRewardService {
    CookingRewardResult applyCookingRewardOnce(const CookingRewardRequest&) override { return {}; }
    QueryResult<GrowthSnapshot> getGrowthSnapshot(const LocalDate&) const override { return {}; }
};
struct CompletionAdapter final : ICookingCompletionService {
    CookingCompletionPreview previewCompletion(const CookingCompletionRequest&) override { return {}; }
    CookingCompletionResult completeCooking(const CookingCompletionRequest&, const std::string&) override { return {}; }
    CookingCompletionResult retryCompletion(const EventId&) override { return {}; }
    QueryResult<CookingCompletionDetails> getCompletion(const EventId&) const override { return {}; }
    CookingCompletionResult cancelPreparedCompletion(const EventId&) override { return {}; }
    QueryResult<std::vector<CookingCompletionResult>> getPendingCompletions() const override { return {}; }
};
static_assert(!std::is_abstract_v<InventoryAdapter> && !std::is_abstract_v<RecipeAdapter>);
static_assert(!std::is_abstract_v<RewardAdapter> && !std::is_abstract_v<CompletionAdapter>);
static_assert(std::is_same_v<decltype(&createInventoryService), std::unique_ptr<IInventoryService>(*)(InventoryManager&)>);
static_assert(std::is_same_v<decltype(&createRecipeService), std::unique_ptr<IRecipeService>(*)(const std::string&)>);
static_assert(std::is_same_v<decltype(&createRewardService), std::unique_ptr<IRewardService>(*)(RewardSystem&)>);
static_assert(std::is_same_v<decltype(&createCookingCompletionService), std::unique_ptr<ICookingCompletionService>(*)(IInventoryService&, IRecipeService&, IRewardService&, const std::string&)>);
int main() {
    // A default result must never be interpreted as success or an empty valid inventory.
    const QueryResult<InventorySnapshot> absent;
    if (absent.outcome.status != OperationStatus::Failed || absent.outcome.errorCode == ErrorCode::None) return 1;
    // The recovery boundary carries the original request, frozen recipe and durable stage.
    CookingCompletionDetails detail;
    detail.snapshot = Shixu::ContractFixtures::cooking();
    detail.progress.eventId = detail.snapshot.request.eventId;
    detail.progress.stage = CompletionStage::InventorySaved;
    detail.progress.inventory = Shixu::ContractFixtures::expectedTomatoResult();
    if (detail.snapshot.request.recipeId != detail.snapshot.recipe.id ||
        detail.progress.inventory->eventId != detail.progress.eventId) return 2;
    // Current growth is a separate query type, not the historical reward result.
    static_assert(!std::is_same_v<GrowthSnapshot, CookingRewardResult>);
    return 0;
}
