#include "recipe/RecipeService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

NutritionInfo calculateNutrition(const Recipe& /*recipe*/, int /*servings*/)
{
    NutritionInfo info;
    // stub：仅返回默认值。阶段三实现。
    return info;
}

void recordMeal(const Recipe& /*recipe*/, int /*servings*/, const std::string& /*time*/)
{
    // stub：阶段三实现（需要持久化 MealRecord）。
}
