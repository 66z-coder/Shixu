#pragma once
#include "contracts/CoreContracts.h"
namespace Shixu::ContractFixtures {
using namespace Shixu::Contracts::V1;
// Synthetic fixtures, NOT production recipes/nutrition or an inventory implementation.
inline InventorySnapshot tomatoes() {
    return {"fixture-r1", {{101, "番茄", 10.0, "个", "2026-10-30", 150.0, 1500.0}}};
}
inline ConsumptionRequest tomatoUse() {
    return {"5f778a96-91c7-4e42-a5ae-d7ad189d6101", "2026-10-28 18:00:00",
            {{"番茄", 300.0}}, {}};
}
inline ConsumptionResult expectedTomatoResult() {
    ConsumptionResult r;
    r.outcome = {OperationStatus::Succeeded, ErrorCode::None, "已扣减"};
    r.eventId = tomatoUse().eventId;
    r.availability = {{"番茄", 300.0, 1500.0, 0.0, false, true, AvailabilityState::Enough}};
    r.consumed = {{101, "番茄", 300.0, 10.0, 8.0, 1500.0, 1200.0, true}};
    r.consumedExpiring = true;
    return r;
}
inline InventorySnapshot eggs() {
    return {"fixture-eggs", {{201, "鸡蛋", 6.0, "个", "2026-11-01", 50.0, 300.0}}};
}
inline IngredientAvailability unknownEggs() {
    return {"鸡蛋", 100.0, 50.0, 50.0, true, false, AvailabilityState::Unknown};
}
inline CookingSnapshot cooking() {
    CookingSnapshot s;
    const auto use = tomatoUse();
    s.request = {use.eventId, 9001, 1, use.completedAt, {}};
    s.recipe.id = 9001;
    s.recipe.name = "接口样例番茄菜（非正式菜谱）";
    s.recipe.ingredients = {{"番茄", 300.0, {20.0, 1.0, 3.0, 0.2}}};
    s.recipe.steps = {"仅用于接口样例"};
    s.recipe.cookingTimeMinutes = 15;
    s.recipe.tags = {Tags::Quick, Tags::Easy};
    s.requirements = use.requirements;
    s.nutrition = {60.0, 3.0, 9.0, 0.6};
    return s;
}
inline MealWriteRequest meal() {
    const auto s = cooking();
    return {s.request.eventId, {s.recipe.id, s.recipe.name, 1, s.request.completedAt,
                               "2026-10-28", s.nutrition}};
}
inline CookingRewardResult expectedFirstReward() {
    return {{OperationStatus::Succeeded, ErrorCode::None, "已保存奖励"},
            tomatoUse().eventId, 30, true, 1, 30, {"first_checkin"}};
}
}
