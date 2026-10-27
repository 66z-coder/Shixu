// ============================================================================
// C 模块阶段一：findRecipesByFoods 验证程序。
// 不依赖 Qt，可在任何有 C++17 编译器的环境单独构建。
// 运行后控制台输出测试结果，全部通过时返回 0。
// ============================================================================

#include <cassert>
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

bool containsRecipeByName(const std::vector<Recipe>& recipes, const std::string& name)
{
    for (const Recipe& r : recipes) {
        if (r.name == name) return true;
    }
    return false;
}

std::size_t indexOfRecipeByName(const std::vector<Recipe>& recipes, const std::string& name)
{
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        if (recipes[i].name == name) return i;
    }
    return static_cast<std::size_t>(-1);
}

void check(bool condition, const std::string& description)
{
    if (condition) {
        std::cout << "  OK : " << description << std::endl;
    } else {
        std::cerr << "FAIL : " << description << std::endl;
        std::exit(1);
    }
}

} // namespace

int main()
{
    std::cout << "=== C 模块阶段一验证：findRecipesByFoods ===" << std::endl;

    // 1. 空库存 → 返回空
    {
        std::cout << "[Case 1] 空库存" << std::endl;
        auto results = findRecipesByFoods({});
        check(results.empty(), "空库存应返回空列表");
    }

    // 2. 所有库存 amount = 0 → 不可用，返回空
    {
        std::cout << "[Case 2] 库存数量为 0" << std::endl;
        std::vector<FoodBatch> inv = {makeBatch("番茄", 0.0), makeBatch("鸡蛋", 0.0)};
        auto results = findRecipesByFoods(inv);
        check(results.empty(), "amount=0 的食材应视为不可用");
    }

    // 3. 已知食材 → 番茄炒蛋应完整匹配排第一
    {
        std::cout << "[Case 3] 完整匹配应优先" << std::endl;
        // 覆盖所有菜谱的 3 种食材：番茄炒蛋、青椒土豆丝、红烧茄子 都应完整匹配
        std::vector<FoodBatch> inv = {
            makeBatch("番茄", 100), makeBatch("鸡蛋", 100), makeBatch("食用油", 50),
            makeBatch("土豆", 100), makeBatch("青椒", 50),
            makeBatch("茄子", 200), makeBatch("大蒜", 30),
        };
        auto results = findRecipesByFoods(inv);
        check(!results.empty(), "应返回候选菜谱");
        check(containsRecipeByName(results, "番茄炒蛋"), "应包含番茄炒蛋");
        check(containsRecipeByName(results, "青椒土豆丝"), "应包含青椒土豆丝");
        check(containsRecipeByName(results, "红烧茄子"), "应包含红烧茄子");

        // 完整匹配应有更高优先级：找到番茄炒蛋的位置
        const auto idx = indexOfRecipeByName(results, "番茄炒蛋");
        check(idx != static_cast<std::size_t>(-1), "番茄炒蛋在结果中");
        // 它前面不应该出现部分匹配（即 matched < total 的）
        for (std::size_t i = 0; i < idx; ++i) {
            // 检查 results[i] 是否也是完整匹配
            std::size_t total = results[i].ingredients.size();
            std::size_t matched = 0;
            for (const auto& ing : results[i].ingredients) {
                if (ing.foodName == "番茄" || ing.foodName == "鸡蛋" || ing.foodName == "食用油"
                    || ing.foodName == "土豆" || ing.foodName == "青椒"
                    || ing.foodName == "茄子" || ing.foodName == "大蒜") {
                    ++matched;
                }
            }
            check(matched == total, "番茄炒蛋前面的菜谱必须也是完整匹配");
        }
    }

    // 4. 只有一种食材 → 应返回部分匹配的菜谱
    {
        std::cout << "[Case 4] 部分匹配" << std::endl;
        std::vector<FoodBatch> inv = {makeBatch("番茄", 200)};
        auto results = findRecipesByFoods(inv);
        check(!results.empty(), "一种食材也应返回候选（番茄炒蛋、番茄鸡蛋面、番茄蛋花汤等都含番茄）");
        check(containsRecipeByName(results, "番茄炒蛋"), "应包含番茄炒蛋");
        check(containsRecipeByName(results, "番茄鸡蛋面"), "应包含番茄鸡蛋面");
        check(containsRecipeByName(results, "番茄蛋花汤"), "应包含番茄蛋花汤");
    }

    // 5. 验证 30 道示例菜谱都被正确加载（用倒排索引交叉验证）
    {
        std::cout << "[Case 5] 内置菜谱库大小与多样性" << std::endl;
        // 用极端丰富库存把所有菜谱都"完整匹配"一遍（因为我们只是查 findRecipesByFoods
        // 不做严格数量检查，所以用一种万能食材覆盖所有菜谱）。
        std::vector<FoodBatch> inv;
        for (int i = 1; i <= 50; ++i) {
            inv.push_back(makeBatch("通用食材" + std::to_string(i), 100));
        }
        // 直接用倒排索引逻辑验证 —— 通过遍历几个关键食材
        std::vector<std::string> allKeyIngredients = {
            "番茄", "鸡蛋", "土豆", "青椒", "鸡胸肉", "花生", "黄瓜",
            "猪里脊", "木耳", "胡萝卜", "豆腐", "五花肉", "鸡翅",
            "可乐", "西兰花", "大蒜", "小白菜", "牛肉", "洋葱", "米饭",
            "青豆", "面条", "油菜", "香菇", "冬瓜", "排骨", "紫菜",
            "韭菜", "芹菜", "香干", "油麦菜", "茄子", "豆角",
            "鲈鱼", "姜", "大葱", "虾", "菠菜", "西葫芦"
        };
        std::vector<FoodBatch> superInv;
        for (const auto& n : allKeyIngredients) superInv.push_back(makeBatch(n, 100));
        auto results = findRecipesByFoods(superInv);
        check(results.size() >= 30, "内置菜谱库应至少有 30 道菜谱可用（实际匹配到 "
                                        + std::to_string(results.size()) + "）");

        // 常见标签/分类覆盖检查
        bool has荤菜 = false, has素菜 = false, has汤类 = false, has主食 = false;
        for (const auto& r : results) {
            if (r.category == "荤菜") has荤菜 = true;
            if (r.category == "素菜") has素菜 = true;
            if (r.category == "汤类") has汤类 = true;
            if (r.category == "主食") has主食 = true;
        }
        check(has荤菜, "菜谱库应包含荤菜分类");
        check(has素菜, "菜谱库应包含素菜分类");
        check(has汤类, "菜谱库应包含汤类分类");
        check(has主食, "菜谱库应包含主食分类");
    }

    // 6. 名称规范化：前后空格应被忽略
    {
        std::cout << "[Case 6] 食材名称规范化（trim whitespace）" << std::endl;
        std::vector<FoodBatch> inv = {
            makeBatch("  番茄  ", 100), makeBatch("鸡蛋", 100), makeBatch(" 食用油", 50)
        };
        auto results = findRecipesByFoods(inv);
        check(containsRecipeByName(results, "番茄炒蛋"), "名称前后空格应被规范化");
    }

    std::cout << std::endl << "=== 全部验证通过 ===" << std::endl;
    return 0;
}
