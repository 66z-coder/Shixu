#pragma once

#include <memory>
#include <string>

#include "contracts/CoreContracts.h"
#include "contracts/ServiceFactories.h"

// ============================================================================
// C 模块内部：IRecipeService 的具体实现。
// 该类是共享契约（CoreContracts.h）和 C 模块内部实现之间的薄适配层。
// 构造时传入 mealRecordsPath，设置 MealRecordStorage 的静态路径，
// 所有查询写入都使用同一份存储。旧 recordMeal() 与本适配器独立，
// 但共享 MealRecordStorage 的路径配置。
// ============================================================================

namespace Shixu::Contracts::V1 {

class RecipeContractAdapter final : public IRecipeService
{
public:
    // 构造：接收用餐记录文件路径，设置 MealRecordStorage 静态配置。
    explicit RecipeContractAdapter(const std::string& mealRecordsPath);

    // IRecipeService 五方法。
    QueryResult<std::vector<Recipe>> getAllRecipes() const override;
    QueryResult<Recipe> findRecipeById(RecipeId id) const override;
    QueryResult<std::vector<MealRecord>> getMealsByDate(const LocalDate& date) const override;
    QueryResult<NutritionInfo> getNutritionByDate(const LocalDate& date) const override;
    MealWriteResult recordMealOnce(const MealWriteRequest& request) override;

private:
    // 辅助：构造成功 OperationResult。
    static OperationResult ok();

    // 辅助：构造指定状态和错误码的 OperationResult。
    static OperationResult fail(ErrorCode code, const std::string& message);

    // 辅助：构造 NotFound 结果。
    static OperationResult notFound(const std::string& what);

    // 辅助：构造 StorageFailure 结果（从 MealRecordStorage::getLastError() 取信息）。
    static OperationResult storageFailure(const std::string& action);
};

} // namespace Shixu::Contracts::V1
