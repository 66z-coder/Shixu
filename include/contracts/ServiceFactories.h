#pragma once
// Shared construction boundary. These are declarations, not production stubs.
#include <memory>
#include "contracts/CoreContracts.h"
class InventoryManager;
class RewardSystem;
namespace Shixu::Contracts::V1 {
// B/E adapters borrow the existing instances; owner must outlive the adapter.
std::unique_ptr<IInventoryService> createInventoryService(InventoryManager& inventory);
// C owns its storage adapter; UTF-8 path, same file as legacy recordMeal.
std::unique_ptr<IRecipeService> createRecipeService(const std::string& mealRecordsPath);
std::unique_ptr<IRewardService> createRewardService(RewardSystem& rewards);
// Coordinator borrows all services; independent journal, not a second inventory connection.
std::unique_ptr<ICookingCompletionService> createCookingCompletionService(
    IInventoryService& inventory, IRecipeService& recipes, IRewardService& rewards,
    const std::string& journalPath);
} // namespace Shixu::Contracts::V1
