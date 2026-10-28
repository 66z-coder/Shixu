// C 模块：IRecipeService + createRecipeService 适配器验证
// 所有数据文件放在系统临时目录，测试结束清理；不触碰仓库内任何文件。
#include <filesystem>
#include <iostream>
#include <string>

#include "contracts/CoreContracts.h"
#include "contracts/ServiceFactories.h"
#include "recipe/RecipeService.h"

using namespace Shixu::Contracts::V1;
namespace fs = std::filesystem;

int g_pass = 0, g_fail = 0;
void check(bool cond, const std::string& desc) {
    if (cond) { ++g_pass; std::cout << "  OK : " << desc << "\n"; }
    else      { ++g_fail; std::cerr << "FAIL : " << desc << "\n"; }
}

int main()
{
    std::cout << "=== IRecipeService + createRecipeService 验证 ===\n";

    // 独立临时目录，避免与仓库根 adapter_test_meal_records.txt 冲突或污染。
    const fs::path tempDir = fs::temp_directory_path() / "_shixu_contract_adapter_test";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    const std::string path = (tempDir / "meal_records.txt").u8string();

    auto service = createRecipeService(path);
    check(service != nullptr, "createRecipeService 返回非空");

    // 1. getAllRecipes 返回 30 道
    {
        auto r = service->getAllRecipes();
        check(r.outcome.status == OperationStatus::Succeeded, "getAllRecipes Succeeded");
        check(r.value.size() == 30, "getAllRecipes 30 道");
        if (r.value.size() == 30) {
            check(r.value.front().id == 1 && r.value.back().id == 30,
                  "菜谱 ID 覆盖 1..30");
        }
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

    // 4. 合法日期 + 空库 → Succeeded + 空列表（空库不是错误）
    {
        auto r = service->getMealsByDate("2026-10-28");
        check(r.outcome.status == OperationStatus::Succeeded, "合法日期空库 Succeeded");
        check(r.value.empty(), "合法日期空库返回空列表");
    }

    // 5. 非法日期格式/不存在的日期 → Failed + InvalidArgument
    {
        const char* badDates[] = {"abc", "2026/10/28", "2026-13-40",
                                  "2026-02-30", "2026-1-1", ""};
        for (const char* d : badDates) {
            auto meals = service->getMealsByDate(d);
            check(meals.outcome.status == OperationStatus::Failed,
                  std::string("getMealsByDate(\"") + d + "\") Failed");
            check(meals.outcome.errorCode == ErrorCode::InvalidArgument,
                  std::string("getMealsByDate(\"") + d + "\") InvalidArgument");
            check(meals.value.empty(), std::string("getMealsByDate(\"") + d + "\") 不返回伪数据");

            auto nut = service->getNutritionByDate(d);
            check(nut.outcome.status == OperationStatus::Failed,
                  std::string("getNutritionByDate(\"") + d + "\") Failed");
            check(nut.outcome.errorCode == ErrorCode::InvalidArgument,
                  std::string("getNutritionByDate(\"") + d + "\") InvalidArgument");
        }
    }

    // 6. 合法日期 + 空库 → 营养汇总为 0 且 Succeeded
    {
        auto r = service->getNutritionByDate("2026-10-28");
        check(r.outcome.status == OperationStatus::Succeeded, "合法日期营养查询 Succeeded");
        check(r.value.calories == 0.0 && r.value.protein == 0.0,
              "合法日期空库营养为 0");
    }

    // 7. 空 eventId → InvalidArgument（契约语义，不是 InvalidState）
    {
        MealWriteRequest req;
        req.eventId = "";
        req.record.recipeId = 1;
        req.record.servings = 1;
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Failed, "空 eventId Failed");
        check(r.outcome.errorCode == ErrorCode::InvalidArgument, "空 eventId InvalidArgument");
        check(!r.savedRecord.has_value(), "空 eventId 不返回 savedRecord");
    }

    // 8. 正常写入
    {
        MealWriteRequest req;
        req.eventId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        req.record.recipeId = 1;
        req.record.recipeName = "番茄炒蛋";
        req.record.servings = 1;
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Succeeded, "正常写入 Succeeded");
        check(r.savedRecord.has_value(), "savedRecord 有值");
        check(r.savedRecord.has_value() && r.savedRecord->eventId == req.eventId,
              "savedRecord.eventId 与请求一致");
    }

    // 9. 同一 eventId + 相同内容 → AlreadyApplied，记录数不增加
    {
        MealWriteRequest req;
        req.eventId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        req.record.recipeId = 1;
        req.record.recipeName = "番茄炒蛋";
        req.record.servings = 1;
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::AlreadyApplied, "重复提交 AlreadyApplied");
        auto meals = service->getMealsByDate("2026-10-28");
        check(meals.value.size() == 1, "重复提交后仍只有 1 条记录");
    }

    // 10. 同一 eventId + 不同内容 → EventConflict，原记录不变
    {
        MealWriteRequest req;
        req.eventId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        req.record.recipeId = 1;
        req.record.recipeName = "番茄炒蛋";
        req.record.servings = 5;  // 与已保存内容不同
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Failed, "冲突提交 Failed");
        check(r.outcome.errorCode == ErrorCode::EventConflict, "冲突提交 EventConflict");
        auto meals = service->getMealsByDate("2026-10-28");
        check(meals.value.size() == 1, "冲突后记录数仍为 1");
        check(meals.value.size() == 1 && meals.value[0].servings == 1,
              "冲突后原记录内容未被覆盖");
    }

    // 11. 重建服务（模拟重启）后仍识别重复事件
    {
        service.reset();
        service = createRecipeService(path);
        MealWriteRequest req;
        req.eventId = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        req.record.recipeId = 1;
        req.record.recipeName = "番茄炒蛋";
        req.record.servings = 1;
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::AlreadyApplied, "重启后仍识别重复事件");
    }

    // 12. 旧自由函数回归
    {
        std::vector<FoodBatch> empty;
        auto r = findRecipesByFoods(empty);
        check(r.empty(), "旧 findRecipesByFoods 空库存返回空");
    }

    // 清理测试文件与临时目录
    service.reset();
    fs::remove_all(tempDir, ec);
    check(!fs::exists(tempDir, ec), "临时测试目录已清理");

    std::cout << "\n=== " << g_pass << " 通过 / " << g_fail << " 失败 ===\n";
    return g_fail == 0 ? 0 : 1;
}
