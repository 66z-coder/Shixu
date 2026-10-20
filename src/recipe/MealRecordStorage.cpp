#include "MealRecordStorage.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace MealRecordStorage {

namespace {

std::string g_filePath = "meal_records.txt";
std::string g_lastError;

void setError(const std::string& message) { g_lastError = message; }

// 按分隔符切分字符串。空字段保留为空串；不做额外过滤。
std::vector<std::string> split(const std::string& line, char delim)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : line) {
        if (c == delim) {
            parts.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(std::move(current));
    return parts;
}

bool tryParseInt(const std::string& text, int& out)
{
    if (text.empty()) return false;
    try {
        std::size_t consumed = 0;
        out = std::stoi(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

bool tryParseDouble(const std::string& text, double& out)
{
    if (text.empty()) return false;
    try {
        std::size_t consumed = 0;
        out = std::stod(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

// 从完整时间字符串中提取日期部分（前 10 字符，格式 YYYY-MM-DD）。
// 若字符串长度不足 10，返回空串。
std::string extractDate(const std::string& time)
{
    if (time.size() < 10) return {};
    return time.substr(0, 10);
}

// 将一行解析为 MealRecord；字段不足或格式错误返回 false。
// 格式：recipeId|recipeName|servings|time|calories|protein|carbohydrates|fat
// 右边固定 6 个字段（servings/time/4 个营养）永远不包含 '|'，
// 从右向左读取它们；左边第一个字段是 recipeId，
// 中间剩余部分按 '|' 拼回就是 recipeName（即使原名中含 '|' 也能还原）。
bool parseLine(const std::string& line, MealRecord& out)
{
    const auto fields = split(line, '|');
    // 最少：recipeId(1) + recipeName(1) + 固定 6 字段 = 8 个
    if (fields.size() < 8) return false;

    const std::size_t n = fields.size();

    int recipeId = 0;
    int servings = 0;
    double calories = 0.0, protein = 0.0, carbs = 0.0, fat = 0.0;

    // 左边第一个字段 → recipeId
    if (!tryParseInt(fields[0], recipeId)) return false;

    // 从右向左取固定 6 个字段（它们永远不含 '|'）
    if (!tryParseInt(fields[n - 6], servings)) return false;   // servings
    const std::string time = fields[n - 5];                      // time
    if (!tryParseDouble(fields[n - 4], calories)) return false; // calories
    if (!tryParseDouble(fields[n - 3], protein)) return false;  // protein
    if (!tryParseDouble(fields[n - 2], carbs)) return false;    // carbs
    if (!tryParseDouble(fields[n - 1], fat)) return false;      // fat

    // 左边界 1，右边界 n-7（右边 6 个固定字段从 n-6 开始）
    // fields[1..n-7] 按 '|' 拼回就是 recipeName
    out.recipeId = recipeId;
    out.recipeName = fields[1];
    for (std::size_t i = 2; i <= n - 7; ++i) {
        out.recipeName.push_back('|');
        out.recipeName += fields[i];
    }
    out.servings = servings;
    out.time = time;
    out.date = extractDate(out.time);
    out.nutrition.calories = calories;
    out.nutrition.protein = protein;
    out.nutrition.carbohydrates = carbs;
    out.nutrition.fat = fat;
    return true;
}

} // namespace

void setFilePath(const std::string& path) { g_filePath = path; }

std::string filePath() { return g_filePath; }

bool append(const MealRecord& record)
{
    std::ofstream out(g_filePath, std::ios::app | std::ios::binary);
    if (!out) {
        setError("MealRecordStorage::append: 无法打开文件 '" + g_filePath + "' 写入");
        return false;
    }
    out << record.recipeId << '|'
        << record.recipeName << '|'
        << record.servings << '|'
        << record.time << '|'
        << record.nutrition.calories << '|'
        << record.nutrition.protein << '|'
        << record.nutrition.carbohydrates << '|'
        << record.nutrition.fat << '\n';
    out.close();
    if (!out) {
        setError("MealRecordStorage::append: 文件写入后关闭失败 '" + g_filePath + "'");
        return false;
    }
    clearLastError();
    return true;
}

std::vector<MealRecord> loadAll()
{
    std::vector<MealRecord> result;
    std::ifstream in(g_filePath, std::ios::binary);
    if (!in) {
        // 文件不存在或无法打开：返回空列表。
        // 文件不存在是正常的"首次运行、尚无记录"场景，不是错误。
        // 真的无法打开（权限/损坏）也返回空，不崩溃。
        return result;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        MealRecord record;
        if (parseLine(line, record)) {
            result.push_back(std::move(record));
        }
        // 解析失败的行静默跳过（可能是旧版本遗留或手工编辑过的坏行），
        // 不影响其他正常记录，也不崩溃。
    }
    return result;
}

std::vector<MealRecord> loadByDate(const std::string& date)
{
    std::vector<MealRecord> result;
    if (date.size() != 10) return result;

    for (const MealRecord& record : loadAll()) {
        if (record.date == date) {
            result.push_back(record);
        }
    }
    return result;
}

NutritionInfo summarizeByDate(const std::string& date)
{
    NutritionInfo sum;
    for (const MealRecord& record : loadByDate(date)) {
        sum.calories      += record.nutrition.calories;
        sum.protein       += record.nutrition.protein;
        sum.carbohydrates += record.nutrition.carbohydrates;
        sum.fat           += record.nutrition.fat;
    }
    return sum;
}

std::string getLastError() { return g_lastError; }

void clearLastError() { g_lastError.clear(); }

} // namespace MealRecordStorage
