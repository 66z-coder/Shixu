#include "RecipeContractAdapter.h"

#include <algorithm>
#include <utility>

#include "MealRecordStorage.h"
#include "RecipeData.h"
#include "RecipeRepository.h"

namespace {

// 进程内唯一菜谱库，与 RecipeService.cpp 中的 repository() 同源，
// 都调用 loadBuiltinRecipes()，保证数据完全一致。
const RecipeRepository& repository()
{
    static const RecipeRepository instance(loadBuiltinRecipes());
    return instance;
}

} // namespace

namespace Shixu::Contracts::V1 {

// ---------------------------------------------------------------------------
// 辅助：构造 OperationResult
// ---------------------------------------------------------------------------

OperationResult RecipeContractAdapter::ok()
{
    OperationResult r;
    r.status = OperationStatus::Succeeded;
    r.errorCode = ErrorCode::None;
    return r;
}

OperationResult RecipeContractAdapter::fail(ErrorCode code, const std::string& message)
{
    OperationResult r;
    r.status = OperationStatus::Failed;
    r.errorCode = code;
    r.message = message;
    return r;
}

OperationResult RecipeContractAdapter::notFound(const std::string& what)
{
    return fail(ErrorCode::NotFound, what + " not found");
}

OperationResult RecipeContractAdapter::storageFailure(const std::string& action)
{
    const std::string detail = MealRecordStorage::getLastError();
    // 区分损坏 vs 权限/IO 失败
    if (detail.rfind("InvalidData:", 0) == 0) {
        return fail(ErrorCode::InvalidData, detail);
    }
    return fail(ErrorCode::StorageFailure,
                "Failed to " + action + (detail.empty() ? "" : ": " + detail));
}

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------

RecipeContractAdapter::RecipeContractAdapter(const std::string& mealRecordsPath)
{
    MealRecordStorage::setFilePath(mealRecordsPath);
    MealRecordStorage::clearLastError();
}

// ---------------------------------------------------------------------------
// 查询接口
// ---------------------------------------------------------------------------

QueryResult<std::vector<Recipe>> RecipeContractAdapter::getAllRecipes() const
{
    QueryResult<std::vector<Recipe>> result;
    result.outcome = ok();
    result.value = repository().all();
    return result;
}

QueryResult<Recipe> RecipeContractAdapter::findRecipeById(RecipeId id) const
{
    QueryResult<Recipe> result;
    const Recipe* found = repository().findById(id);
    if (found == nullptr) {
        result.outcome = notFound("Recipe with id=" + std::to_string(id));
        return result;
    }
    result.outcome = ok();
    result.value = *found;
    return result;
}

QueryResult<std::vector<MealRecord>> RecipeContractAdapter::getMealsByDate(
    const LocalDate& date) const
{
    QueryResult<std::vector<MealRecord>> result;
    MealRecordStorage::clearLastError();
    result.value = MealRecordStorage::loadByDate(date);
    const std::string err = MealRecordStorage::getLastError();
    if (!err.empty()) {
        result.outcome = storageFailure("load meals by date");
        return result;
    }
    result.outcome = ok();
    // 空列表是合法结果（该日期没记录），不视为找不到。
    return result;
}

QueryResult<NutritionInfo> RecipeContractAdapter::getNutritionByDate(
    const LocalDate& date) const
{
    QueryResult<NutritionInfo> result;
    MealRecordStorage::clearLastError();
    result.value = MealRecordStorage::summarizeByDate(date);
    const std::string err = MealRecordStorage::getLastError();
    if (!err.empty()) {
        result.outcome = storageFailure("summarize nutrition by date");
        return result;
    }
    result.outcome = ok();
    return result;
}

// ---------------------------------------------------------------------------
// recordMealOnce — 阶段三：可靠幂等存储
//
// 调用 MealRecordStorage::recordMealOnce 完成：
//   • eventId 空 → StorageFailure(InvalidArgument)
//   • 同 eventId + 同内容 → AlreadyApplied + savedRecord
//   • 同 eventId + 不同内容 → Failed + EventConflict
//   • 无冲突 → Succeeded + savedRecord（原子替换写入）
//   • 存储损坏 → Failed + InvalidData；写入失败 → Failed + StorageFailure
// ---------------------------------------------------------------------------

MealWriteResult RecipeContractAdapter::recordMealOnce(
    const MealWriteRequest& request)
{
    MealWriteResult result;
    result.eventId = request.eventId;

    MealRecordStorage::clearLastError();
    const auto storageResult = MealRecordStorage::recordMealOnce(
        request.eventId, request.record);
    result.outcome = storageResult.outcome;
    result.savedRecord = storageResult.savedRecord;
    return result;
}

// ---------------------------------------------------------------------------
// 工厂函数
// ---------------------------------------------------------------------------

std::unique_ptr<IRecipeService> createRecipeService(
    const std::string& mealRecordsPath)
{
    return std::make_unique<RecipeContractAdapter>(mealRecordsPath);
}

} // namespace Shixu::Contracts::V1
