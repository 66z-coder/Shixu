// ============================================================================
// C 模块阶段三：calculateNutrition 验证程序。
// 所有预期值均为手算得到，浮点比较使用合理容差。
// ============================================================================

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "model/Recipe.h"
#include "recipe/RecipeService.h"

namespace {

// 营养比较容差：浮点累计误差控制在 0.01 以内足够。
constexpr double kEps = 0.01;

bool approxEq(double a, double b, double eps = kEps)
{
    return std::abs(a - b) <= eps;
}

RecipeIngredient makeIng(const std::string& name, double grams,
                         double cal, double pro, double carb, double fat)
{
    RecipeIngredient ing;
    ing.foodName = name;
    ing.amountGrams = grams;
    ing.nutritionPer100g = NutritionInfo{cal, pro, carb, fat};
    return ing;
}

Recipe makeRecipe(int id, std::vector<RecipeIngredient> ingredients)
{
    Recipe r;
    r.id = id;
    r.name = "营养测试菜谱-" + std::to_string(id);
    r.ingredients = std::move(ingredients);
    return r;
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

void checkInfo(const NutritionInfo& got,
               double expCal, double expPro, double expCarb, double expFat,
               const std::string& label)
{
    check(approxEq(got.calories, expCal),      label + " - calories 应为 " + std::to_string(expCal));
    check(approxEq(got.protein, expPro),       label + " - protein 应为 " + std::to_string(expPro));
    check(approxEq(got.carbohydrates, expCarb), label + " - carbohydrates 应为 " + std::to_string(expCarb));
    check(approxEq(got.fat, expFat),           label + " - fat 应为 " + std::to_string(expFat));
}

} // namespace

int main()
{
    std::cout << "=== C 模块阶段三验证：calculateNutrition ===" << std::endl;

    // ---- Case 1: 单一食材 ----
    // 食材 A：200g，每 100g = {100, 10, 20, 30}
    // 1 份贡献 = 200/100 × {100,10,20,30} = {200, 20, 40, 60}
    {
        std::cout << "[Case 1] 单一食材" << std::endl;
        Recipe r = makeRecipe(1, {makeIng("A", 200, 100, 10, 20, 30)});
        NutritionInfo n = calculateNutrition(r, 1);
        checkInfo(n, 200.0, 20.0, 40.0, 60.0, "1 份 · 200g@100g基准");
    }

    // ---- Case 2: 多种食材累加 ----
    // 食材 A：100g × {100,10,20,30} = {100,10,20,30}
    // 食材 B：50g  × {200,20,40,60} = {100,10,20,30}
    // 合计 1 份 = {200, 20, 40, 60}
    {
        std::cout << "[Case 2] 多种食材累加" << std::endl;
        Recipe r = makeRecipe(2, {
            makeIng("A", 100, 100, 10, 20, 30),
            makeIng("B", 50,  200, 20, 40, 60),
        });
        NutritionInfo n = calculateNutrition(r, 1);
        checkInfo(n, 200.0, 20.0, 40.0, 60.0, "A+B 累加");
    }

    // ---- Case 3: 1 / 2 / 3 份之间的比例 ----
    {
        std::cout << "[Case 3] 份数比例" << std::endl;
        Recipe r = makeRecipe(3, {makeIng("A", 150, 200, 10, 5, 8)});
        // 1 份 = 150/100 × {200,10,5,8} = {300, 15, 7.5, 12}
        NutritionInfo n1 = calculateNutrition(r, 1);
        checkInfo(n1, 300.0, 15.0, 7.5, 12.0, "1 份");

        NutritionInfo n2 = calculateNutrition(r, 2);
        checkInfo(n2, 600.0, 30.0, 15.0, 24.0, "2 份 = 1 份 × 2");

        NutritionInfo n3 = calculateNutrition(r, 3);
        checkInfo(n3, 900.0, 45.0, 22.5, 36.0, "3 份 = 1 份 × 3");

        // 交叉验证：n2 / n1 各字段 ≈ 2
        check(approxEq(n2.calories / n1.calories, 2.0),
              "n2.calories / n1.calories ≈ 2");
        check(approxEq(n3.fat / n1.fat, 3.0),
              "n3.fat / n1.fat ≈ 3");
    }

    // ---- Case 4: 空菜谱（无 ingredients）----
    {
        std::cout << "[Case 4] 空菜谱" << std::endl;
        NutritionInfo n = calculateNutrition(Recipe{}, 5);
        checkInfo(n, 0.0, 0.0, 0.0, 0.0, "空菜谱 + 5 份");
    }

    // ---- Case 5: 非法份数 ----
    {
        std::cout << "[Case 5] 非法份数（<= 0）→ 零营养" << std::endl;
        Recipe r = makeRecipe(5, {makeIng("A", 200, 100, 10, 20, 30)});
        checkInfo(calculateNutrition(r, 0),   0.0, 0.0, 0.0, 0.0, "servings=0");
        checkInfo(calculateNutrition(r, -1),  0.0, 0.0, 0.0, 0.0, "servings=-1");
        checkInfo(calculateNutrition(r, -50), 0.0, 0.0, 0.0, 0.0, "servings=-50");
    }

    // ---- Case 6: 非法食材重量（<= 0）→ 跳过 ----
    {
        std::cout << "[Case 6] 负数/零食材重量" << std::endl;
        Recipe r = makeRecipe(6, {
            makeIng("正常", 100, 100, 10, 20, 30),  // 贡献 {100,10,20,30}
            makeIng("负重量", -50, 500, 50, 100, 150), // 跳过
            makeIng("零重量", 0,   500, 50, 100, 150), // 跳过
        });
        NutritionInfo n = calculateNutrition(r, 1);
        checkInfo(n, 100.0, 10.0, 20.0, 30.0, "负/零重量食材被跳过");

        // 全部食材都是负/零重量
        Recipe r2 = makeRecipe(7, {
            makeIng("坏A", -10, 100, 10, 20, 30),
            makeIng("坏B", 0,   200, 20, 40, 60),
        });
        NutritionInfo n2 = calculateNutrition(r2, 2);
        checkInfo(n2, 0.0, 0.0, 0.0, 0.0, "全负/零重量 → 零营养");
    }

    // ---- Case 7: 小数精度（用真实菜谱数据交叉验证）----
    // 番茄炒蛋：番茄 200g + 鸡蛋 150g + 食用油 15g，1 份
    // 番茄 200g × {20, 0.9, 4.0, 0.2}/100 = {40, 1.8, 8.0, 0.4}
    // 鸡蛋 150g × {144, 13.3, 2.8, 8.8}/100 = {216, 19.95, 4.2, 13.2}
    // 食用油 15g × {899, 0, 0, 99.9}/100 = {134.85, 0, 0, 14.985}
    // 合计 1 份 = {390.85, 21.75, 12.2, 28.585}
    // 2 份 = {781.70, 43.50, 24.40, 57.17}
    {
        std::cout << "[Case 7] 真实菜谱数据交叉验证（番茄炒蛋）" << std::endl;
        Recipe r = makeRecipe(8, {
            makeIng("番茄",  200, 20.0, 0.9, 4.0, 0.2),
            makeIng("鸡蛋",  150, 144.0, 13.3, 2.8, 8.8),
            makeIng("食用油", 15, 899.0, 0.0, 0.0, 99.9),
        });
        NutritionInfo n1 = calculateNutrition(r, 1);
        checkInfo(n1, 390.85, 21.75, 12.2, 28.585, "番茄炒蛋 1 份");
        NutritionInfo n2 = calculateNutrition(r, 2);
        checkInfo(n2, 781.70, 43.50, 24.4, 57.17, "番茄炒蛋 2 份 = 手算");
    }

    std::cout << std::endl << "=== 阶段三验证完成："
              << g_pass << " 通过 / " << g_fail << " 失败 ===" << std::endl;
    return g_fail == 0 ? 0 : 1;
}
