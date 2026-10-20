#pragma once

#include <string>
#include <vector>

#include "model/MealRecord.h"

// ============================================================================
// C 模块内部：用餐记录的本地文件存储管理。
//
// 不是团队《公共接口文档 V2.0》冻结的接口，供 C 模块自身使用，
// 并供后续 E 模块的统计功能读取（E 通过 C 模块调用此查询能力）。
//
// 文件格式（管道分隔，一行一记录，追加写入，易读易解析，无第三方依赖）：
//   recipeId|recipeName|servings|time|calories|protein|carbohydrates|fat
// 示例：
//   1|番茄炒蛋|2|2026-10-20 19:30:00|781.70|43.50|24.40|57.17
//
// 错误处理：recordMeal 返回 void，因此存储层将错误诊断存于内部 lastError，
// 调用方可通过 getLastError() 查询。文件路径可通过 setFilePath 覆盖，
// 测试时指向临时目录即可隔离。
// ============================================================================

namespace MealRecordStorage {

// 设置用餐记录文件的绝对路径（或工作目录相对路径）。
// 默认路径为 "meal_records.txt"（工作目录下），适合命令行测试与原型。
// Qt 桌面应用集成时，建议 E 模块在程序启动时通过此函数设置到绝对路径
// （例如 QStandardPaths::writableLocation 所返回的应用数据目录），
// 避免因从不同工作目录启动而产生多份记录文件。
// 本存储层不直接依赖 Qt，保持可独立测试。
void setFilePath(const std::string& path);

// 获取当前使用的文件路径（主要用于日志和调试）。
std::string filePath();

// 将一条用餐记录追加写入文件。
// 写入失败（例如无法打开）时函数返回 false，详细信息存入 lastError。
bool append(const MealRecord& record);

// 从文件完整读取全部用餐记录。读取失败时返回空向量，
// 详细错误存入 lastError。文件不存在时返回空（视为合法空库）。
std::vector<MealRecord> loadAll();

// 按日期查询，date 格式 YYYY-MM-DD（匹配 record.date 前 10 字符）。
std::vector<MealRecord> loadByDate(const std::string& date);

// 按日期汇总总营养（返回 0.0 的日期说明当天没记录）。
NutritionInfo summarizeByDate(const std::string& date);

// 最近一次文件操作的错误描述；无错误时为空串。
std::string getLastError();

// 清空错误状态（便于调用方重置后再次尝试）。
void clearLastError();

} // namespace MealRecordStorage
