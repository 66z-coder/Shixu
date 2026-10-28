#include "MealRecordStorage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace Shixu::Contracts::V1;

namespace MealRecordStorage {

namespace {

std::string g_filePath = "meal_records.txt";
std::string g_lastError;

void setError(const std::string& message) { g_lastError = message; }

// V2 字段分隔符（ASCII Unit Separator, 0x1F）
constexpr char kFieldSep = '\x1F';

// 前置声明（内部）
bool parseLineV1(const std::string& line, MealRecord& out);
bool parseLineV2(const std::string& line, MealRecord& out);
std::string serializeV2Line(const MealRecord& r);

// ----- UTF-8 路径 -----
fs::path u8Path(const std::string& path)
{
    return fs::u8path(path);
}

// ----- V2 字段转义（确保任何字符串字段均可往返写入） -----
// 转义规则：\| \\ \n \r 分别表示字面量 | \ 换行 CR
// kFieldSep (0x1F) 不会出现在业务数据中，不需要转义
std::string encodeField(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        if (c == '\\')      out += "\\\\";
        else if (c == '|')  out += "\\|";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else                out.push_back(static_cast<char>(c));
    }
    return out;
}

bool decodeField(const std::string& enc, std::string& out)
{
    out.clear();
    out.reserve(enc.size());
    for (std::size_t i = 0; i < enc.size(); ++i) {
        char c = enc[i];
        if (c == '\\') {
            if (i + 1 >= enc.size()) return false;
            char next = enc[++i];
            if      (next == '\\') out.push_back('\\');
            else if (next == '|')  out.push_back('|');
            else if (next == 'n')  out.push_back('\n');
            else if (next == 'r')  out.push_back('\r');
            else return false;
        } else {
            out.push_back(c);
        }
    }
    return true;
}

// ----- 转义感知的字段切分 -----
// 遇到 '\' 跳过下一个字符（视为字面量），只对裸 kFieldSep 切分。
std::vector<std::string> splitEscaped(const std::string& line, char delim)
{
    std::vector<std::string> parts;
    std::string current;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            // 转义序列整体作为当前字段的字面内容
            current.push_back(c);
            current.push_back(line[++i]);
        } else if (c == delim) {
            parts.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(std::move(current));
    return parts;
}

// ----- 朴素切分（给 V1 旧格式用，不处理转义） -----
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

std::string extractDate(const std::string& time)
{
    if (time.size() < 10) return {};
    return time.substr(0, 10);
}

// ----- V1 行解析（旧 8 字段管道分隔，裸 '|'，无独立 date/无 eventId） -----
bool parseLineV1(const std::string& line, MealRecord& out)
{
    const auto fields = split(line, '|');
    if (fields.size() < 8) return false;
    const std::size_t n = fields.size();

    int recipeId = 0, servings = 0;
    if (!tryParseInt(fields[0], recipeId)) return false;
    if (!tryParseInt(fields[n - 6], servings)) return false;
    const std::string time = fields[n - 5];
    double calories = 0.0, protein = 0.0, carbs = 0.0, fat = 0.0;
    if (!tryParseDouble(fields[n - 4], calories)) return false;
    if (!tryParseDouble(fields[n - 3], protein)) return false;
    if (!tryParseDouble(fields[n - 2], carbs)) return false;
    if (!tryParseDouble(fields[n - 1], fat)) return false;

    out.recipeId = recipeId;
    out.recipeName = fields[1];
    for (std::size_t i = 2; i <= n - 7; ++i) {
        out.recipeName.push_back('|');
        out.recipeName += fields[i];
    }
    out.servings = servings;
    out.time = time;
    out.date = extractDate(out.time);  // V1 从 time 推导 date
    out.nutrition.calories = calories;
    out.nutrition.protein = protein;
    out.nutrition.carbohydrates = carbs;
    out.nutrition.fat = fat;
    out.eventId.clear();
    return true;
}

// ----- V2 行解析（10 字段 \x1F 分隔，含独立 date） -----
// 字段顺序：eventId, recipeId, recipeName, servings, time, date,
//           calories, protein, carbohydrates, fat
// 字符串字段（eventId, recipeName, time, date）经过 encodeField 编码，
// 此处用 splitEscaped + decodeField 还原。
bool parseLineV2(const std::string& line, MealRecord& out)
{
    const auto fields = splitEscaped(line, kFieldSep);
    if (fields.size() != 10) return false;

    // 反转义四个字符串字段
    std::string eventId, recipeName, time, date;
    if (!decodeField(fields[0], eventId)) return false;
    if (!decodeField(fields[2], recipeName)) return false;
    if (!decodeField(fields[4], time)) return false;
    if (!decodeField(fields[5], date)) return false;

    int recipeId = 0, servings = 0;
    if (!tryParseInt(fields[1], recipeId)) return false;
    if (!tryParseInt(fields[3], servings)) return false;

    double calories = 0.0, protein = 0.0, carbs = 0.0, fat = 0.0;
    if (!tryParseDouble(fields[6], calories)) return false;
    if (!tryParseDouble(fields[7], protein)) return false;
    if (!tryParseDouble(fields[8], carbs)) return false;
    if (!tryParseDouble(fields[9], fat)) return false;

    out.eventId = std::move(eventId);
    out.recipeId = recipeId;
    out.recipeName = std::move(recipeName);
    out.servings = servings;
    out.time = std::move(time);
    // 独立 date：如持久化值为空，用 time 前 10 字符兜底
    out.date = !date.empty() ? std::move(date) : extractDate(out.time);
    out.nutrition.calories = calories;
    out.nutrition.protein = protein;
    out.nutrition.carbohydrates = carbs;
    out.nutrition.fat = fat;
    return true;
}

// ----- V2 序列化（10 字段 \x1F 分隔，含独立 date + 字段转义） -----
std::string serializeV2Line(const MealRecord& r)
{
    std::ostringstream oss;
    oss << encodeField(r.eventId) << kFieldSep
        << r.recipeId << kFieldSep
        << encodeField(r.recipeName) << kFieldSep
        << r.servings << kFieldSep
        << encodeField(r.time) << kFieldSep
        << encodeField(r.date.empty() ? extractDate(r.time) : r.date) << kFieldSep
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << r.nutrition.calories << kFieldSep
        << r.nutrition.protein << kFieldSep
        << r.nutrition.carbohydrates << kFieldSep
        << r.nutrition.fat;
    return oss.str();
}

// ----- 可靠替换 -----
bool replaceAtomically(const std::string& targetPath, const std::vector<MealRecord>& records)
{
    const fs::path target = u8Path(targetPath);
    const fs::path tmp = target.parent_path() / (target.filename().string() + ".tmp");

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            setError("StorageFailure: 无法打开临时文件 '" + tmp.u8string() + "'");
            return false;
        }
        out << "V2\n";
        for (const auto& r : records) {
            out << serializeV2Line(r) << '\n';
        }
        out.flush();
        if (!out) { setError("StorageFailure: 临时文件刷新失败"); return false; }
        out.close();
        if (!out) { setError("StorageFailure: 临时文件关闭失败"); return false; }
    }

#ifdef _WIN32
    const std::wstring tmpW = tmp.wstring();
    const std::wstring targetW = target.wstring();
    if (!MoveFileExW(tmpW.c_str(), targetW.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD err = GetLastError();
        try { fs::remove(tmp); } catch (...) {}
        setError("StorageFailure: 原子替换失败 (Win32 err=" + std::to_string(err) + ")");
        return false;
    }
#else
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) {
        try { fs::remove(tmp); } catch (...) {}
        setError("StorageFailure: 原子替换失败 (" + ec.message() + ")");
        return false;
    }
#endif

    clearLastError();
    return true;
}

bool fileExists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(u8Path(path), ec);
}

// ----- 严格读取（recordMealOnce + loadAll + append V1→V2 升级前的验证） -----
std::vector<MealRecord> loadAllStrict(const std::string& path, std::string& errOut)
{
    std::vector<MealRecord> result;
    std::error_code ec;
    if (!fs::exists(u8Path(path), ec)) { errOut.clear(); return result; }

    std::ifstream in(u8Path(path), std::ios::binary);
    if (!in) { errOut = "StorageFailure: 无法打开文件读取"; return {}; }

    // 读首行判断版本
    std::string head;
    if (!std::getline(in, head)) { errOut.clear(); return {}; }
    while (!head.empty() && head.back() == '\r') head.pop_back();

    const bool isV2 = (head == "V2");
    MealRecord mr;

    // V1 首行本身是第一条记录
    if (!isV2) {
        if (!parseLineV1(head, mr)) {
            errOut = "InvalidData: V1 首行损坏";
            return {};
        }
        result.push_back(std::move(mr));
    }

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const bool ok = isV2 ? parseLineV2(line, mr) : parseLineV1(line, mr);
        if (!ok) {
            errOut = "InvalidData: 损坏记录 '" + line.substr(0, 80)
                     + (line.size() > 80 ? "...'" : "'");
            return {};
        }
        result.push_back(std::move(mr));
    }
    errOut.clear();
    return result;
}

// ----- 幂等比较 -----
bool recordsEqualIgnoringEvent(const MealRecord& a, const MealRecord& b)
{
    return a.recipeId == b.recipeId
        && a.recipeName == b.recipeName
        && a.servings == b.servings
        && a.time == b.time
        && a.date == b.date
        && a.nutrition.calories == b.nutrition.calories
        && a.nutrition.protein == b.nutrition.protein
        && a.nutrition.carbohydrates == b.nutrition.carbohydrates
        && a.nutrition.fat == b.nutrition.fat;
}

} // namespace

// ============================================================================
// 对外接口
// ============================================================================

void setFilePath(const std::string& path) { g_filePath = path; }
std::string filePath() { return g_filePath; }

// ----- append（旧 recordMeal() 用） -----
bool append(const MealRecord& record)
{
    if (!record.eventId.empty()) {
        setError("InvalidArgument: append 不接受非空 eventId，请用 recordMealOnce");
        return false;
    }

    std::error_code ec;
    const fs::path path = u8Path(g_filePath);
    const bool exists = fs::exists(path, ec);
    const bool empty = !exists || fs::is_empty(path, ec);

    if (empty) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            setError("StorageFailure: 无法创建文件 '" + g_filePath + "'");
            return false;
        }
        out << "V2\n" << serializeV2Line(record) << '\n';
        out.flush(); out.close();
        if (!out) { setError("StorageFailure: 创建文件后关闭失败"); return false; }
        clearLastError();
        return true;
    }

    // 已存在：读首行判断版本
    std::ifstream chk(path, std::ios::binary);
    if (!chk) { setError("StorageFailure: 无法打开文件 '" + g_filePath + "'"); return false; }
    std::string head;
    std::getline(chk, head);
    while (!head.empty() && head.back() == '\r') head.pop_back();
    chk.close();

    if (head == "V2") {
        // 已是 V2：严格读（校验旧文件完整）→ 直接 ios::app 追加
        std::string readErr;
        loadAllStrict(g_filePath, readErr);
        if (!readErr.empty()) {
            setError("InvalidData: 追加前校验 V2 文件失败 — " + readErr);
            return false;
        }
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) { setError("StorageFailure: 无法追加打开文件"); return false; }
        out << serializeV2Line(record) << '\n';
        out.flush(); out.close();
        if (!out) { setError("StorageFailure: 追加关闭失败"); return false; }
        clearLastError();
        return true;
    }

    // V1 遗留文件 → 严格读验证完整性（遇损坏拒绝升级）→ 追加 → 原子替换
    // 修复 issue 2：之前用宽松读会跳过损坏行，现在严格读，损坏直接拒绝
    std::string readErr;
    std::vector<MealRecord> existing = loadAllStrict(g_filePath, readErr);
    if (!readErr.empty()) {
        // 原文件有损坏或无法解析 → 拒绝升级，原文件逐字节不变
        setError(readErr);
        return false;
    }
    existing.push_back(record);
    if (!replaceAtomically(g_filePath, existing)) return false;
    return true;
}

// ----- loadAll（严格模式） -----
std::vector<MealRecord> loadAll()
{
    std::string err;
    auto result = loadAllStrict(g_filePath, err);
    if (!err.empty()) {
        setError(err);
        return {};
    }
    clearLastError();
    return result;
}

std::vector<MealRecord> loadByDate(const std::string& date)
{
    std::vector<MealRecord> result;
    if (date.size() != 10) return result;
    for (const MealRecord& r : loadAll()) {
        if (r.date == date) result.push_back(r);
    }
    return result;
}

NutritionInfo summarizeByDate(const std::string& date)
{
    NutritionInfo sum;
    for (const MealRecord& r : loadByDate(date)) {
        sum.calories      += r.nutrition.calories;
        sum.protein       += r.nutrition.protein;
        sum.carbohydrates += r.nutrition.carbohydrates;
        sum.fat           += r.nutrition.fat;
    }
    return sum;
}

// ----- recordMealOnce（幂等） -----
IdempotentResult recordMealOnce(const std::string& eventId, const MealRecord& recordIn)
{
    IdempotentResult result;

    if (eventId.empty()) {
        result.outcome.status = OperationStatus::Failed;
        result.outcome.errorCode = ErrorCode::InvalidArgument;
        result.outcome.message = "recordMealOnce: eventId 不能为空（旧 recordMeal() 自由函数不参与幂等）";
        return result;
    }

    // 独立 date 字段：如果调用方没填，从 time 提取
    MealRecord candidate = recordIn;
    candidate.eventId = eventId;
    if (candidate.date.empty()) candidate.date = extractDate(candidate.time);

    std::string readErr;
    const std::vector<MealRecord> existing = loadAllStrict(g_filePath, readErr);
    if (!readErr.empty()) {
        result.outcome.status = OperationStatus::Failed;
        if (readErr.rfind("InvalidData:", 0) == 0) {
            result.outcome.errorCode = ErrorCode::InvalidData;
        } else {
            result.outcome.errorCode = ErrorCode::StorageFailure;
        }
        result.outcome.message = readErr;
        return result;
    }

    for (const MealRecord& r : existing) {
        if (r.eventId.empty()) continue;
        if (r.eventId != eventId) continue;

        if (recordsEqualIgnoringEvent(r, candidate)) {
            result.outcome.status = OperationStatus::AlreadyApplied;
            result.outcome.errorCode = ErrorCode::None;
            result.outcome.message = "事件 " + eventId + " 已成功保存";
            result.savedRecord = r;
            return result;
        } else {
            result.outcome.status = OperationStatus::Failed;
            result.outcome.errorCode = ErrorCode::EventConflict;
            result.outcome.message = "事件 " + eventId + " 已存在但记录内容不同，拒绝覆盖";
            return result;
        }
    }

    std::vector<MealRecord> toWrite = existing;
    toWrite.push_back(candidate);
    if (!replaceAtomically(g_filePath, toWrite)) {
        result.outcome.status = OperationStatus::Failed;
        result.outcome.errorCode = ErrorCode::StorageFailure;
        result.outcome.message = getLastError();
        return result;
    }

    result.outcome.status = OperationStatus::Succeeded;
    result.outcome.errorCode = ErrorCode::None;
    result.savedRecord = candidate;
    return result;
}

std::string getLastError() { return g_lastError; }
void clearLastError() { g_lastError.clear(); }

} // namespace MealRecordStorage
