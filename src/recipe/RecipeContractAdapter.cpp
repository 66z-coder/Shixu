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
// recordMealOnce — 阶段二显式过渡状态
//
// 幂等存储（eventId 去重、版本化格式、原子替换、重启后生效）是阶段三补齐。
// 在阶段二尚未具备这些能力时，本函数诚实返回 InvalidState，
// 不调用旧 MealRecordStorage::append 写入任何记录，避免制造伪成功和重复写入。
//
// 阶段三完成后，本函数将改为：
//   1) 读全量查同 eventId；同内容 → AlreadyApplied + savedRecord；
//      不同内容 → EventConflict；
//   2) 无冲突 → 版本化格式 + eventId 列写入，临时文件 + 原子替换。
// ---------------------------------------------------------------------------

MealWriteResult RecipeContractAdapter::recordMealOnce(
    const MealWriteRequest& request)
{
    // 阶段二过渡：幂等存储尚未就绪，诚实返回 InvalidState，
    // 不调用旧 append 制造伪成功和重复记录。
    MealWriteResult result;
    result.outcome = fail(ErrorCode::InvalidState,
        "recordMealOnce 幂等存储尚未实现（阶段三补齐 eventId 持久化、版本化格式与原子替换）。"
        "当前调用会被拒绝，请改用旧 recordMeal 或等待阶段三。");
    result.eventId = request.eventId;
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
