#include "recipe/RecipeService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "MealRecordStorage.h"
#include "RecipeData.h"
#include "RecipeRepository.h"

namespace {

// 进程内唯一的菜谱库（内置示例数据），首次使用时构造。
const RecipeRepository& repository()
{
    static const RecipeRepository instance(loadBuiltinRecipes());
    return instance;
}

std::string normalizeName(const std::string& name)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = name.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = name.find_last_not_of(whitespace);
    return name.substr(begin, end - begin + 1);
}

// 阶段一的“可用”判定：库存中存在同名食材且数量 > 0。
// 是否要求数量足以覆盖菜谱用量，待 FoodBatch 单位冻结后再细化。
bool isAvailable(const RecipeIngredient& ingredient,
                 const std::unordered_set<std::string>& availableNames)
{
    return availableNames.count(normalizeName(ingredient.foodName)) > 0;
}

} // namespace

std::vector<Recipe> findRecipesByFoods(const std::vector<FoodBatch>& foods)
{
    // 1. 汇总可用食材名称（数量 > 0 才视为可用）。
    std::unordered_set<std::string> availableNames;
    for (const FoodBatch& batch : foods) {
        if (batch.amount > 0.0) {
            availableNames.insert(normalizeName(batch.foodName));
        }
    }
    if (availableNames.empty()) {
        return {};
    }

    // 2. 用倒排索引收集候选菜谱（至少含一种可用食材）。
    std::unordered_set<std::size_t> candidateIndices;
    for (const std::string& name : availableNames) {
        for (const std::size_t index : repository().findByIngredient(name)) {
            candidateIndices.insert(index);
        }
    }

    // 3. 计算每个候选的匹配情况：matched == 总数 为完整匹配。
    struct Candidate {
        std::size_t index;
        std::size_t matched;
        std::size_t total;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(candidateIndices.size());
    for (const std::size_t index : candidateIndices) {
        const Recipe& recipe = repository().all()[index];
        std::size_t matched = 0;
        for (const RecipeIngredient& ingredient : recipe.ingredients) {
            if (isAvailable(ingredient, availableNames)) {
                ++matched;
            }
        }
        candidates.push_back({index, matched, recipe.ingredients.size()});
    }

    // 4. 排序：完整匹配优先，其次覆盖率降序，最后按菜谱 id 升序保证稳定。
    std::sort(candidates.begin(), candidates.end(),
              [&repositoryRef = repository()](const Candidate& left, const Candidate& right) {
        const bool leftFull = left.matched == left.total;
        const bool rightFull = right.matched == right.total;
        if (leftFull != rightFull) {
            return leftFull;
        }
        const double leftRate = static_cast<double>(left.matched) / static_cast<double>(left.total);
        const double rightRate = static_cast<double>(right.matched) / static_cast<double>(right.total);
        if (leftRate != rightRate) {
            return leftRate > rightRate;
        }
        return repositoryRef.all()[left.index].id < repositoryRef.all()[right.index].id;
    });

    // 5. 输出排序后的菜谱。
    std::vector<Recipe> results;
    results.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        results.push_back(repository().all()[candidate.index]);
    }
    return results;
}

// ----------------------------------------------------------------------------
// 以下三个接口在阶段一仅冻结签名、提供 stub 实现，后续阶段替换为正式逻辑。
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// 特殊情况处理约定（均在下方实现中体现）：
//   1) 菜谱中的 ingredient.amountGrams <= 0：该食材视为"不需要"，
//      从分子分母中同时排除，available/missing 列表也不出现。
//   2) 库存 FoodBatch.amount <= 0：该批次视为不可用，不参与聚合。
//   3) 同种食材在 recipe.ingredients 中出现多行：先汇总需求量再匹配。
//   4) 同种食材在 inventory 中多批次出现：先汇总可用量再匹配。
//   5) 菜谱所有食材都 amountGrams <= 0（无有效食材）：matchRate = 100.0，
//      available/missing 为空。避免除零。
//   6) 空菜谱 / 空库存 / 仅非正需求量：matchRate 始终给出确定值，
//      永不除以零或返回 NaN。
// ----------------------------------------------------------------------------
MatchResult calculateMatch(const Recipe& recipe,
                           const std::vector<FoodBatch>& inventory)
{
    MatchResult result;

    // 1. 按规范化名称汇总库存可用量（仅 amount > 0 计入）。
    std::unordered_map<std::string, double> inventoryByNorm;
    for (const FoodBatch& batch : inventory) {
        if (batch.amount > 0.0) {
            inventoryByNorm[normalizeName(batch.foodName)] += batch.amount;
        }
    }

    // 2. 按规范化名称汇总菜谱需求量（仅 amountGrams > 0 计入）。
    std::unordered_map<std::string, double> demandByNorm;
    // 同时保留规范化名称到首次出现的原始显示名称，用于返回结果。
    std::unordered_map<std::string, std::string> normToDisplay;
    for (const RecipeIngredient& ingredient : recipe.ingredients) {
        const std::string norm = normalizeName(ingredient.foodName);
        if (ingredient.amountGrams > 0.0) {
            demandByNorm[norm] += ingredient.amountGrams;
            if (normToDisplay.find(norm) == normToDisplay.end()) {
                normToDisplay[norm] = ingredient.foodName;
            }
        }
    }

    // 3. 对每种"不同的食材种类"进行数量匹配。
    std::size_t satisfied = 0;
    for (const auto& [norm, required] : demandByNorm) {
        const auto invIt = inventoryByNorm.find(norm);
        const double available = (invIt == inventoryByNorm.end()) ? 0.0 : invIt->second;

        if (available >= required) {
            ++satisfied;
            result.availableIngredients.push_back(normToDisplay[norm]);
        } else {
            result.missingIngredients.push_back(normToDisplay[norm]);
        }
    }

    // 4. matchRate：种类数（不是条目数）→ 百分比，保留两位小数。
    if (demandByNorm.empty()) {
        // 菜谱不含有效食材：定义为"已满足"，避免除零。
        result.matchRate = 100.0;
    } else {
        const double rawRate = 100.0 * static_cast<double>(satisfied)
                                   / static_cast<double>(demandByNorm.size());
        result.matchRate = std::round(rawRate * 100.0) / 100.0;
    }
    return result;
}

NutritionInfo calculateNutrition(const Recipe& recipe, int servings)
{
    NutritionInfo total;

    // 异常份数字段统一处理：servings <= 0 视为非法，返回零营养值。
    // 理由：不能为负数（负营养没有物理意义），0 份也不需要任何营养。
    if (servings <= 0) {
        return total;
    }

    double totalCalories = 0.0;
    double totalProtein = 0.0;
    double totalCarbs = 0.0;
    double totalFat = 0.0;

    for (const RecipeIngredient& ingredient : recipe.ingredients) {
        // 负数/零重量跳过，不能贡献负营养。
        if (ingredient.amountGrams <= 0.0) {
            continue;
        }
        // 每种食材每 100 克营养 × 份量比例 = 该食材对"1 份菜谱"的贡献。
        const double ratio = ingredient.amountGrams / 100.0;
        totalCalories  += ratio * ingredient.nutritionPer100g.calories;
        totalProtein   += ratio * ingredient.nutritionPer100g.protein;
        totalCarbs     += ratio * ingredient.nutritionPer100g.carbohydrates;
        totalFat       += ratio * ingredient.nutritionPer100g.fat;
    }

    total.calories      = totalCalories  * static_cast<double>(servings);
    total.protein       = totalProtein   * static_cast<double>(servings);
    total.carbohydrates = totalCarbs     * static_cast<double>(servings);
    total.fat           = totalFat       * static_cast<double>(servings);

    return total;
}

void recordMeal(const Recipe& recipe, int servings, const std::string& time)
{
    // servings <= 0：非法份数，不记录、不写入存储。
    // 避免零份/负份产生零营养或负营养的脏数据。
    if (servings <= 0) {
        return;
    }

    MealRecord record;
    record.recipeId = recipe.id;
    record.recipeName = recipe.name;
    record.servings = servings;
    record.time = time;
    // 存储层从 time 提取 YYYY-MM-DD 日期。这里也显式记录一份，
    // 方便后续读取时直接复用，不用再做字符串处理。
    if (time.size() >= 10) {
        record.date = time.substr(0, 10);
    }
    // 复用 calculateNutrition：计算本次用餐的总营养。
    record.nutrition = calculateNutrition(recipe, servings);

    MealRecordStorage::append(record);
    // 返回 void，无法向上层传递成功/失败。
    // 写入失败的详细原因存储在 MealRecordStorage::getLastError()，
    // 上层（如 E 模块）在需要时可查询。
}
