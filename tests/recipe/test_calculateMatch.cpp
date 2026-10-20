// ============================================================================
// C 模块阶段二：calculateMatch 验证程序。
// 覆盖 10 类场景，全部通过时返回 0。
// ============================================================================

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "model/FoodBatch.h"
#include "model/Recipe.h"
#include "recipe/RecipeService.h"

namespace {

FoodBatch makeBatch(const std::string& name, double amount)
{
    FoodBatch b;
    b.foodId = "test-" + name;
    b.foodName = name;
    b.amount = amount;
    return b;
}

Recipe makeRecipe(int id, std::vector<RecipeIngredient> ingredients)
{
    Recipe r;
    r.id = id;
    r.name = "测试菜谱-" + std::to_string(id);
    r.ingredients = std::move(ingredients);
    return r;
}

RecipeIngredient makeIng(const std::string& name, double grams)
{
    RecipeIngredient ing;
    ing.foodName = name;
    ing.amountGrams = grams;
    return ing;
}

bool containsName(const std::vector<std::string>& list, const std::string& name)
{
    for (const auto& n : list) {
        if (n == name) return true;
    }
    return false;
}

bool approxEq(double a, double b)
{
    return std::abs(a - b) < 0.01; // 匹配率保留两位小数，容差设为 0.01
}

int g_pass = 0;
int g_fail = 0;

void check(bool condition, const std::string& description)
{
    if (condition) {
        ++g_pass;
        std::cout << "  OK : " << description << std::endl;
    } else {
        ++g_fail;
        std::cerr << "FAIL : " << description << std::endl;
    }
}

} // namespace

int main()
{
    std::cout << "=== C 模块阶段二验证：calculateMatch ===" << std::endl;

    // --- Case 1: 所有食材充足，匹配率 100% ---
    {
        std::cout << "[Case 1] 所有食材充足 → 100%" << std::endl;
        Recipe recipe = makeRecipe(101, {
            makeIng("番茄", 200),
            makeIng("鸡蛋", 150),
            makeIng("食用油", 15),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 300),
            makeBatch("鸡蛋", 200),
            makeBatch("食用油", 50),
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 100.0), "matchRate == 100.0");
        check(r.availableIngredients.size() == 3, "available 应含 3 项");
        check(r.missingIngredients.empty(), "missing 应为空");
    }

    // --- Case 2: 部分食材缺失 ---
    {
        std::cout << "[Case 2] 部分食材缺失" << std::endl;
        Recipe recipe = makeRecipe(102, {
            makeIng("番茄", 200),
            makeIng("鸡蛋", 150),
            makeIng("食用油", 15),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 300),
            makeBatch("食用油", 50),
            // 缺鸡蛋
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 66.67), "matchRate == 66.67 (2/3 * 100)");
        check(containsName(r.availableIngredients, "番茄"), "available 含番茄");
        check(containsName(r.availableIngredients, "食用油"), "available 含食用油");
        check(containsName(r.missingIngredients, "鸡蛋"), "missing 含鸡蛋");
    }

    // --- Case 3: 食材名称存在但数量不足 ---
    {
        std::cout << "[Case 3] 数量不足 → 算缺失" << std::endl;
        Recipe recipe = makeRecipe(103, {
            makeIng("番茄", 200),
            makeIng("鸡蛋", 150),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 300),
            makeBatch("鸡蛋", 50), // 只有 50，不够 150
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 50.0), "matchRate == 50.0");
        check(containsName(r.availableIngredients, "番茄"), "available 含番茄");
        check(containsName(r.missingIngredients, "鸡蛋"), "missing 含鸡蛋（数量不足）");
    }

    // --- Case 4: 库存同名食材分布在多批次，需要汇总 ---
    {
        std::cout << "[Case 4] 库存多批次汇总" << std::endl;
        Recipe recipe = makeRecipe(104, {
            makeIng("番茄", 250),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 100),
            makeBatch("番茄", 100),
            makeBatch("番茄", 60), // 合计 260 ≥ 250
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 100.0), "多批次汇总后匹配 100%");
        check(r.missingIngredients.empty(), "不应出现在 missing 中");
    }

    // --- Case 5: 菜谱同名食材重复出现，需要汇总需求 ---
    {
        std::cout << "[Case 5] 菜谱同名多条目汇总需求" << std::endl;
        Recipe recipe = makeRecipe(105, {
            makeIng("番茄", 120),
            makeIng("番茄", 90),   // 番茄合计需求 210
            makeIng("鸡蛋", 150),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 250), // 250 ≥ 210
            makeBatch("鸡蛋", 200),
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 100.0), "菜谱同名多条目汇总后匹配 100%");
        check(r.availableIngredients.size() == 2, "available 应含 2 项（种类不是条目）");
        check(r.missingIngredients.empty(), "missing 应为空");
    }

    // --- Case 6: 库存数量为 0 或负数 → 不计入可用 ---
    {
        std::cout << "[Case 6] 库存 amount <= 0 视为不可用" << std::endl;
        Recipe recipe = makeRecipe(106, {
            makeIng("番茄", 100),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 0),
            makeBatch("番茄", -50),
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 0.0), "零/负库存 → 不匹配");
        check(r.missingIngredients.size() == 1, "missing 包含番茄");
    }

    // --- Case 7: 空库存 ---
    {
        std::cout << "[Case 7] 空库存" << std::endl;
        Recipe recipe = makeRecipe(107, {
            makeIng("番茄", 100),
            makeIng("鸡蛋", 50),
        });
        MatchResult r = calculateMatch(recipe, {});
        check(approxEq(r.matchRate, 0.0), "空库存 → 0%");
        check(r.missingIngredients.size() == 2, "missing 应含 2 项");
        check(r.availableIngredients.empty(), "available 应为空");
    }

    // --- Case 8: 空菜谱 / 仅 amountGrams <= 0 的食材 → 100% 且不崩溃 ---
    {
        std::cout << "[Case 8] 空菜谱（无有效需求）" << std::endl;
        // 8a. 真·空菜谱
        MatchResult r1 = calculateMatch(Recipe{}, {});
        check(approxEq(r1.matchRate, 100.0), "真·空菜谱 → 100%（避免除零）");

        // 8b. 菜谱存在但所有食材 amountGrams <= 0
        Recipe recipe = makeRecipe(108, {
            makeIng("番茄", 0),
            makeIng("鸡蛋", -10),
        });
        MatchResult r2 = calculateMatch(recipe, {makeBatch("番茄", 500)});
        check(approxEq(r2.matchRate, 100.0), "仅非正需求 → 100%");
        check(r2.availableIngredients.empty(), "available 应为空（没有有效食材）");
        check(r2.missingIngredients.empty(), "missing 应为空");
    }

    // --- Case 9: 食材名称首尾空白 → 应规范化后匹配 ---
    {
        std::cout << "[Case 9] 名称规范化（trim whitespace）" << std::endl;
        Recipe recipe = makeRecipe(109, {
            makeIng("  番茄  ", 100),
            makeIng("鸡蛋", 100),
        });
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 200),
            makeBatch(" 鸡蛋 ", 200),
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 100.0), "trim 后应匹配 100%");
    }

    // --- Case 10: matchRate 计算与保留两位小数 ---
    {
        std::cout << "[Case 10] matchRate 保留两位小数" << std::endl;
        Recipe recipe = makeRecipe(110, {
            makeIng("A", 100),
            makeIng("B", 100),
            makeIng("C", 100),
        });
        // 只有 A 和 B 匹配 → 2/3 * 100 = 66.666... → 66.67
        std::vector<FoodBatch> inv = {
            makeBatch("A", 200),
            makeBatch("B", 200),
        };
        MatchResult r = calculateMatch(recipe, inv);
        check(approxEq(r.matchRate, 66.67), "2/3 → 66.67");

        // 额外验证：全部缺失 → 0.0
        Recipe recipe2 = makeRecipe(111, {makeIng("X", 100), makeIng("Y", 100), makeIng("Z", 100)});
        MatchResult r2 = calculateMatch(recipe2, {});
        check(approxEq(r2.matchRate, 0.0), "全部缺失 → 0.0");

        // 验证：边界 1/3 → 33.33
        Recipe recipe3 = makeRecipe(112, {makeIng("A", 100), makeIng("B", 100), makeIng("C", 100)});
        std::vector<FoodBatch> inv3 = {makeBatch("A", 200)};
        MatchResult r3 = calculateMatch(recipe3, inv3);
        check(approxEq(r3.matchRate, 33.33), "1/3 → 33.33");
    }

    std::cout << std::endl << "=== 阶段二验证完成："
              << g_pass << " 通过 / " << g_fail << " 失败 ===" << std::endl;
    return g_fail == 0 ? 0 : 1;
}
