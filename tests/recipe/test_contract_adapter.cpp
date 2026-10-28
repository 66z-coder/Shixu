// 阶段二过渡验证：RecipeContractAdapter + createRecipeService
#include <iostream>
#include <string>

#include "contracts/CoreContracts.h"
#include "contracts/ServiceFactories.h"
#include "recipe/RecipeService.h"

using namespace Shixu::Contracts::V1;

int g_pass = 0, g_fail = 0;
void check(bool cond, const std::string& desc) {
    if (cond) { ++g_pass; std::cout << "  OK : " << desc << "\n"; }
    else      { ++g_fail; std::cerr << "FAIL : " << desc << "\n"; }
}

int main()
{
    std::cout << "=== 阶段二：IRecipeService + createRecipeService 验证 ===\n";

    const std::string path = "adapter_test_meal_records.txt";
    auto service = createRecipeService(path);
    check(service != nullptr, "createRecipeService 返回非空");

    // 1. getAllRecipes 返回 30 道
    {
        auto r = service->getAllRecipes();
        check(r.outcome.status == OperationStatus::Succeeded, "getAllRecipes Succeeded");
        check(r.value.size() == 30, "getAllRecipes 30 道");
    }

    // 2. findRecipeById 合法 id
    {
        auto r = service->findRecipeById(1);
        check(r.outcome.status == OperationStatus::Succeeded, "findRecipeById(1) Succeeded");
        check(r.value.id == 1, "id=1");
        check(r.value.name == "番茄炒蛋", "name=番茄炒蛋");
    }

    // 3. 不存在 id → NotFound
    {
        auto r = service->findRecipeById(9999);
        check(r.outcome.status == OperationStatus::Failed, "9999 Failed");
        check(r.outcome.errorCode == ErrorCode::NotFound, "9999 NotFound");
    }

    // 4. 空库查询
    {
        auto r = service->getMealsByDate("1999-01-01");
        check(r.outcome.status == OperationStatus::Succeeded, "空库 Succeeded");
        check(r.value.empty(), "空库空列表");
    }

    // 5. recordMealOnce 空 eventId → InvalidState
    {
        MealWriteRequest req;
        req.eventId = "";
        req.record.recipeId = 1; req.record.servings = 1; req.record.time = "t"; req.record.date = "d";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Failed, "空 eventId Failed");
        check(r.outcome.errorCode == ErrorCode::InvalidState, "空 eventId InvalidState");
    }

    // 6. 正常写入
    {
        MealWriteRequest req;
        req.eventId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        req.record.recipeId = 1; req.record.recipeName = "番茄炒蛋";
        req.record.servings = 1; req.record.time = "2026-10-28 12:00:00"; req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Succeeded, "正常写入 Succeeded");
        check(r.savedRecord.has_value(), "savedRecord 有值");
    }

    // 7. 旧自由函数回归
    {
        std::vector<FoodBatch> empty;
        auto r = findRecipesByFoods(empty);
        check(r.empty(), "旧 findRecipesByFoods 空库存返回空");
    }

    std::cout << "\n=== " << g_pass << " 通过 / " << g_fail << " 失败 ===\n";
    return g_fail == 0 ? 0 : 1;
}
