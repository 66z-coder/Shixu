#pragma once

#include <string>
#include <vector>

#include "model/MealRecord.h"
#include "contracts/CoreContracts.h"  // OperationResult / ErrorCode

// ============================================================================
// C 模块内部：用餐记录的本地文件存储管理。
//
// 文件格式（版本化，一行一记录，UTF-8）：
//   首行固定头：V2
//   V2 记录行：10 字段，\x1F（Unit Separator）分隔，顺序：
//     eventId, recipeId, recipeName, servings, time, date,
//     calories, protein, carbohydrates, fat
//   字段转义规则（确保任何字符串均可往返写入，含管道符、反斜杠、换行、CR）：
//     \ → \\    | → \|    \n → \n    \r → \r
//     解码器用 splitEscaped 跳过分隔符前的转义序列，不会误切字段。
//   date 是独立持久化字段，不从 time 截取（契约要求完整冻结快照）。
//   营养值用 std::setprecision(max_digits10) 序列化。
//
// 旧 recordMeal() 继续通过本命名空间写入，但 eventId 留空串，
// 不触发幂等去重（与 recordMealOnce 的 eventId 非空严格区分）。
//
// 写入路径：
//   append（旧自由函数用）：文件不存在/空 → 先写 V2 头 + 空 eventId 记录；
//   已存在 V2 文件 → 直接追加空 eventId 记录。不做全量读写。
//   recordMealOnce（新接口）：先严格读全量校验版本与旧内容，
//   幂等判断后将完整内容写入临时文件，再原子替换。
//
// 可靠替换：写同目录 .tmp → flush + close → std::filesystem::rename。
//   Windows 下 rename 若目标已存在会失败，改用 MoveFileEx
//   (MOVEFILE_REPLACE_EXISTING)。替换失败时原文件完好返回 StorageFailure。
// ============================================================================

namespace MealRecordStorage {

// 设置用餐记录文件的绝对路径（或工作目录相对路径）。
// 默认路径为 "meal_records.txt"。
// Windows 下 UTF-8 路径通过 std::filesystem::u8path 转换。
void setFilePath(const std::string& path);

// 获取当前使用的文件路径。
std::string filePath();

// 将一条用餐记录追加写入文件（旧 recordMeal() 调用）。
// 调用方传入的 record.eventId 必须为空（本函数不做事件去重）。
// 文件不存在/空 → 先写 V2 头，再追加空 eventId 记录。
// 写入失败时返回 false，错误信息存入 lastError。
bool append(const MealRecord& record);

// ---------------------------------------------------------------------------
// 严格读取：区分三种情况
//   正常：返回所有记录（V1 无 eventId 列的遗留记录也会读入，eventId 字段为空串）。
//   空文件 / 不存在：返回空向量，lastError 清空（合法空库，非错误）。
//   损坏/版本未知/字段错：lastError 设为 "InvalidData: ..." 前缀，
//     返回空向量，禁止静默跳行。
// ---------------------------------------------------------------------------
std::vector<MealRecord> loadAll();

// 按日期查询（date 格式 YYYY-MM-DD）。
std::vector<MealRecord> loadByDate(const std::string& date);

// 按日期汇总总营养。
NutritionInfo summarizeByDate(const std::string& date);

// ---------------------------------------------------------------------------
// 幂等写入 —— 新接口调用。
//
// 空 eventId → 返回 Failed + InvalidArgument，不写入任何记录。
// 同 eventId 同内容 → AlreadyApplied，savedRecord 为此前保存的记录快照。
// 同 eventId 不同内容 → Failed + EventConflict。
// 无匹配 → 完整内容写入临时文件 + 原子替换，成功返回 Succeeded。
//
// 失败时原文件必须完好（临时文件不会覆盖目标）。
// OperationResult.errorCode 使用 CoreContracts.h 中 ErrorCode 枚举。
// savedRecord 指向写入前加载/构造的记录快照，而非内存引用。
// ---------------------------------------------------------------------------
struct IdempotentResult {
    Shixu::Contracts::V1::OperationResult outcome;
    std::optional<MealRecord> savedRecord;
};
IdempotentResult recordMealOnce(const std::string& eventId, const MealRecord& record);

// 最近一次文件操作的错误描述；无错误时为空串。
// "InvalidData:" 前缀表示损坏/格式错误；
// 其他前缀（如 "StorageFailure:"、"InvalidArgument:"）由调用方读取。
std::string getLastError();

// 清空错误状态。
void clearLastError();

} // namespace MealRecordStorage
