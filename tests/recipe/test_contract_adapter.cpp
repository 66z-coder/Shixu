// C 模块：IRecipeService + createRecipeService 适配器验证
// 所有数据文件放在系统临时目录，测试结束清理；不触碰仓库内任何文件。
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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
        req.record.nutrition = NutritionInfo{256.0, 21.75, 12.2, 13.6};
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
        req.record.nutrition = NutritionInfo{256.0, 21.75, 12.2, 13.6};
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
        req.record.servings = 1;
        req.record.time = "2026-10-28 12:00:00";
        req.record.date = "2026-10-28";
        req.record.nutrition = NutritionInfo{512.0, 21.75, 12.2, 13.6};  // 营养与已保存不同
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::Failed, "冲突提交 Failed");
        check(r.outcome.errorCode == ErrorCode::EventConflict, "冲突提交 EventConflict");
        auto meals = service->getMealsByDate("2026-10-28");
        check(meals.value.size() == 1, "冲突后记录数仍为 1");
        check(meals.value.size() == 1 && meals.value[0].nutrition.calories == 256.0,
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
        req.record.nutrition = NutritionInfo{256.0, 21.75, 12.2, 13.6};
        auto r = service->recordMealOnce(req);
        check(r.outcome.status == OperationStatus::AlreadyApplied, "重启后仍识别重复事件");
    }

    // 12. 饮食记录输入校验：非法请求必须 Failed + InvalidArgument 且不落盘
    {
        const double nan  = std::numeric_limits<double>::quiet_NaN();
        const double inf  = std::numeric_limits<double>::infinity();

        // 契约规定 eventId 为小写、无花括号 UUID，因此所有"字段非法"用例都必须
        // 携带合法 UUID，否则失败原因会被事件 ID 校验抢先命中，掩盖真正的断言目标。
        auto uuidOf = [](int n) {
            char buf[37] = {0};
            std::snprintf(buf, sizeof(buf), "00000000-0000-4000-8000-%012d", n);
            return std::string(buf);
        };

        auto makeValid = [](const std::string& eventId) {
            MealWriteRequest req;
            req.eventId = eventId;
            req.record.recipeId = 1;
            req.record.recipeName = "番茄炒蛋";
            req.record.servings = 1;
            req.record.time = "2026-10-28 12:00:00";
            req.record.date = "2026-10-28";
            req.record.nutrition = NutritionInfo{100.0, 10.0, 10.0, 10.0};
            return req;
        };

        struct BadCase {
            std::string name;
            MealWriteRequest req;
        };
        std::vector<BadCase> bad;

        { auto q = makeValid(uuidOf(101));  q.record.nutrition.calories = -100.0; bad.push_back({"负 calories", q}); }
        { auto q = makeValid(uuidOf(102));  q.record.nutrition.protein = -1.0;   bad.push_back({"负 protein", q}); }
        { auto q = makeValid(uuidOf(103));  q.record.nutrition.carbohydrates = -0.5; bad.push_back({"负 carbohydrates", q}); }
        { auto q = makeValid(uuidOf(104));  q.record.nutrition.fat = -0.1;      bad.push_back({"负 fat", q}); }
        { auto q = makeValid(uuidOf(105));  q.record.nutrition.calories = nan;  bad.push_back({"NaN calories", q}); }
        { auto q = makeValid(uuidOf(106));  q.record.nutrition.protein = nan;   bad.push_back({"NaN protein", q}); }
        { auto q = makeValid(uuidOf(107));  q.record.nutrition.calories = inf;  bad.push_back({"+Inf calories", q}); }
        { auto q = makeValid(uuidOf(108));  q.record.nutrition.fat = -inf;      bad.push_back({"-Inf fat", q}); }
        { auto q = makeValid(uuidOf(109));  q.record.recipeId = 0;              bad.push_back({"菜谱 ID = 0", q}); }
        { auto q = makeValid(uuidOf(110));  q.record.recipeId = -3;             bad.push_back({"菜谱 ID < 0", q}); }
        { auto q = makeValid(uuidOf(111));  q.record.servings = 0;              bad.push_back({"servings = 0", q}); }
        { auto q = makeValid(uuidOf(112));  q.record.servings = 2;              bad.push_back({"servings = 2", q}); }
        { auto q = makeValid(uuidOf(113));  q.record.time = "2026/10/28 12:00:00"; bad.push_back({"非法 time", q}); }
        { auto q = makeValid(uuidOf(114));  q.record.time = "2026-02-30 12:00:00"; bad.push_back({"不存在的 time", q}); }
        { auto q = makeValid(uuidOf(115));  q.record.date = "2026-13-40";       bad.push_back({"非法 date", q}); }

        // eventId 格式：契约要求小写 UUID、无花括号
        { auto q = makeValid("");                                          bad.push_back({"空 eventId", q}); }
        { auto q = makeValid("   ");                                       bad.push_back({"空白 eventId", q}); }
        { auto q = makeValid(std::string("bad\ncntrl-0000-0000-000000000000")); bad.push_back({"含控制字符的 eventId", q}); }
        { auto q = makeValid("AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA");      bad.push_back({"大写 UUID", q}); }
        { auto q = makeValid("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}");    bad.push_back({"带花括号 UUID", q}); }
        { auto q = makeValid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa");       bad.push_back({"35 位 UUID", q}); }
        { auto q = makeValid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaaa");     bad.push_back({"37 位 UUID", q}); }
        { auto q = makeValid("gggggggg-gggg-gggg-gggg-gggggggggggg");      bad.push_back({"非十六进制 UUID", q}); }
        { auto q = makeValid("aaaaaaaaa-aaa-aaaa-aaaa-aaaaaaaaaaaa");      bad.push_back({"连字符位置错误", q}); }
        { auto q = makeValid("aaaaaaaa_aaaa_aaaa_aaaa_aaaaaaaaaaaa");      bad.push_back({"下划线分隔", q}); }

        // 合法形式必须被接受：小写十六进制、8-4-4-4-12、无花括号
        {
            auto q = makeValid("deadbeef-1234-4abc-8def-0123456789ab");
            q.record.time = "2026-10-26 09:00:00";
            q.record.date = "2026-10-26";
            auto r = service->recordMealOnce(q);
            check(r.outcome.status == OperationStatus::Succeeded, "合法小写 UUID 被接受");
            check(r.savedRecord.has_value()
                  && r.savedRecord->eventId == "deadbeef-1234-4abc-8def-0123456789ab",
                  "合法 UUID 原样持久化并回传");
        }

        for (const auto& bc : bad) {
            auto r = service->recordMealOnce(bc.req);
            check(r.outcome.status == OperationStatus::Failed,
                  bc.name + " → Failed");
            check(r.outcome.errorCode == ErrorCode::InvalidArgument,
                  bc.name + " → InvalidArgument");
            check(!r.savedRecord.has_value(), bc.name + " → 不返回 savedRecord");
        }

        // 非法请求不得污染持久化文件与日统计
        auto meals = service->getMealsByDate("2026-10-28");
        check(meals.value.size() == 1, "非法请求后记录数仍为 1");
        auto sum = service->getNutritionByDate("2026-10-28");
        check(sum.outcome.status == OperationStatus::Succeeded, "非法请求后统计仍可读");
        check(sum.value.calories == 256.0, "非法请求未影响日热量统计（仍为 256）");
        check(sum.value.protein == 21.75, "非法请求未影响蛋白质统计（仍为 21.75）");

        // 同一非法请求重试：必须仍是 InvalidArgument，不能变成 EventConflict
        {
            auto q = makeValid(uuidOf(201));
            q.record.nutrition.calories = nan;
            auto first = service->recordMealOnce(q);
            auto second = service->recordMealOnce(q);
            check(first.outcome.errorCode == ErrorCode::InvalidArgument, "首次 NaN 请求 InvalidArgument");
            check(second.outcome.errorCode == ErrorCode::InvalidArgument,
                  "NaN 请求重试仍 InvalidArgument（不产生 EventConflict）");
        }
        {
            auto q = makeValid(uuidOf(202));
            q.record.nutrition.calories = -100.0;
            auto first = service->recordMealOnce(q);
            auto second = service->recordMealOnce(q);
            check(first.outcome.errorCode == ErrorCode::InvalidArgument, "首次负热量请求 InvalidArgument");
            check(second.outcome.errorCode == ErrorCode::InvalidArgument,
                  "负热量请求重试仍 InvalidArgument");
        }

        // 合法请求仍然可保存（含全 0 营养），且不影响既有记录
        {
            auto q = makeValid(uuidOf(301));
            q.record.time = "2026-10-27 08:00:00";
            q.record.date = "2026-10-27";
            q.record.nutrition = NutritionInfo{0.0, 0.0, 0.0, 0.0};
            auto r = service->recordMealOnce(q);
            check(r.outcome.status == OperationStatus::Succeeded, "合法零营养请求仍可保存");
            check(r.savedRecord.has_value(), "合法零营养请求返回 savedRecord");
            check(service->recordMealOnce(q).outcome.status == OperationStatus::AlreadyApplied,
                  "合法零营养请求重复提交仍 AlreadyApplied");
        }
    }

    // 13. 旧自由函数回归
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
