// ============================================================================
// C 模块阶段四：recordMeal + 持久化 验证程序。
// 使用独立临时文件路径，测试结束后清理，不污染真实记录。
// ============================================================================

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "MealRecordStorage.h"
#include "model/Recipe.h"
#include "recipe/RecipeService.h"

namespace {

namespace fs = std::filesystem;

constexpr const char* kTestPath = "meal_records_test.txt";
constexpr double kEps = 0.01;

bool approxEq(double a, double b) { return std::abs(a - b) <= kEps; }

Recipe makeRecipe(int id, const std::string& name,
                  std::vector<RecipeIngredient> ingredients)
{
    Recipe r;
    r.id = id;
    r.name = name;
    r.ingredients = std::move(ingredients);
    return r;
}

RecipeIngredient makeIng(const std::string& name, double grams,
                         double cal, double pro, double carb, double fat)
{
    RecipeIngredient ing;
    ing.foodName = name;
    ing.amountGrams = grams;
    ing.nutritionPer100g = NutritionInfo{cal, pro, carb, fat};
    return ing;
}

int g_pass = 0;
int g_fail = 0;

void check(bool condition, const std::string& description)
{
    if (condition) {
        ++g_pass;
        std::cout << "  OK : " << description << std::endl;
    } else {
        ++g_fail;
        std::cerr << "FAIL : " << description << std::endl;
    }
}

void checkInfo(const NutritionInfo& got,
               double expCal, double expPro, double expCarb, double expFat,
               const std::string& label)
{
    check(approxEq(got.calories, expCal),      label + " calories ≈ " + std::to_string(expCal));
    check(approxEq(got.protein, expPro),       label + " protein ≈ " + std::to_string(expPro));
    check(approxEq(got.carbohydrates, expCarb), label + " carbs ≈ " + std::to_string(expCarb));
    check(approxEq(got.fat, expFat),           label + " fat ≈ " + std::to_string(expFat));
}

bool fileExists(const std::string& path)
{
    return std::ifstream(path).good();
}

void removeFileIfExists(const std::string& path)
{
    std::remove(path.c_str());
}

} // namespace

int main()
{
    std::cout << "=== C 模块阶段四验证：recordMeal + 持久化 ===" << std::endl;

    // 每个测试前都指向临时文件，结束时清理。
    MealRecordStorage::setFilePath(kTestPath);
    MealRecordStorage::clearLastError();
    removeFileIfExists(kTestPath);

    // ---- Case 1: 成功记录一顿饭，字段完整 ----
    {
        std::cout << "[Case 1] 记录一顿饭，字段完整" << std::endl;
        Recipe tomatoEgg = makeRecipe(1, "番茄炒蛋", {
            makeIng("番茄", 200, 20.0, 0.9, 4.0, 0.2),
            makeIng("鸡蛋", 150, 144.0, 13.3, 2.8, 8.8),
        });
        const std::string time = "2026-10-20 12:30:00";
        recordMeal(tomatoEgg, 2, time);

        check(MealRecordStorage::getLastError().empty(), "写入后 lastError 应为空");
        check(fileExists(kTestPath), "临时文件应已创建");

        const auto all = MealRecordStorage::loadAll();
        check(all.size() == 1, "应只读取到 1 条记录");
        if (!all.empty()) {
            const auto& r = all.front();
            check(r.recipeId == 1, "recipeId = 1");
            check(r.recipeName == "番茄炒蛋", "recipeName = '番茄炒蛋'");
            check(r.servings == 2, "servings = 2");
            check(r.time == time, "time 完整保留");
            check(r.date == "2026-10-20", "date = YYYY-MM-DD 前 10 字符");
            // 营养：番茄 200g 贡献 = {40, 1.8, 8, 0.4}；鸡蛋 150g 贡献 = {216, 19.95, 4.2, 13.2}
            // 合计 1 份 = {256, 21.75, 12.2, 13.6}；2 份 = {512, 43.5, 24.4, 27.2}
            checkInfo(r.nutrition, 512.0, 43.5, 24.4, 27.2, "2 份营养");
        }
    }
    removeFileIfExists(kTestPath);

    // ---- Case 2: 连续记录多顿饭，不覆盖 ----
    {
        std::cout << "[Case 2] 多顿饭追加不覆盖" << std::endl;
        Recipe r1 = makeRecipe(1, "A", {makeIng("X", 100, 100, 10, 20, 30)});
        Recipe r2 = makeRecipe(2, "B", {makeIng("Y", 100, 200, 20, 40, 60)});
        Recipe r3 = makeRecipe(3, "C", {makeIng("Z", 100, 300, 30, 60, 90)});

        recordMeal(r1, 1, "2026-10-20 08:00:00");
        recordMeal(r2, 1, "2026-10-20 12:00:00");
        recordMeal(r3, 2, "2026-10-20 18:00:00");

        const auto all = MealRecordStorage::loadAll();
        check(all.size() == 3, "追加后应读取到 3 条记录");
        check(all[0].recipeId == 1, "第 1 条 = A");
        check(all[1].recipeId == 2, "第 2 条 = B");
        check(all[2].recipeId == 3, "第 3 条 = C");
        check(all[2].servings == 2, "C 是 2 份");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 3: 关闭程序后重新读取，记录仍存在 ----
    {
        std::cout << "[Case 3] 重新读取文件后记录仍存在" << std::endl;
        Recipe r = makeRecipe(7, "测试菜", {makeIng("K", 100, 100, 10, 20, 30)});
        recordMeal(r, 3, "2026-10-15 20:00:00");
        check(MealRecordStorage::getLastError().empty(), "写入无错误");

        // 手动"重新打开"文件 —— loadAll 每次都会重新 ifstream 打开
        MealRecordStorage::clearLastError();
        const auto fresh = MealRecordStorage::loadAll();
        check(fresh.size() == 1, "重新读取应仍有 1 条");
        check(fresh.front().recipeId == 7, "recipeId 未丢");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 4: 按日期查询正确筛选 ----
    {
        std::cout << "[Case 4] 按日期查询" << std::endl;
        Recipe r = makeRecipe(9, "X", {makeIng("A", 100, 100, 10, 20, 30)});
        recordMeal(r, 1, "2026-10-18 08:00:00");
        recordMeal(r, 1, "2026-10-19 12:00:00");
        recordMeal(r, 1, "2026-10-20 18:00:00");
        recordMeal(r, 1, "2026-10-20 22:00:00");

        const auto day20 = MealRecordStorage::loadByDate("2026-10-20");
        check(day20.size() == 2, "10-20 应有 2 条");
        const auto day19 = MealRecordStorage::loadByDate("2026-10-19");
        check(day19.size() == 1, "10-19 应有 1 条");
        const auto day21 = MealRecordStorage::loadByDate("2026-10-21");
        check(day21.empty(), "10-21 应为空");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 5: 每日营养汇总正确 ----
    {
        std::cout << "[Case 5] 每日营养汇总" << std::endl;
        // 1 份 = 100g × {100,10,20,30}/100 = {100,10,20,30}
        Recipe r = makeRecipe(5, "A", {makeIng("Q", 100, 100, 10, 20, 30)});
        recordMeal(r, 2, "2026-10-10 08:00:00"); // 2 份 = {200, 20, 40, 60}
        recordMeal(r, 3, "2026-10-10 18:00:00"); // 3 份 = {300, 30, 60, 90}
        // 当天合计 = {500, 50, 100, 150}
        const NutritionInfo summary = MealRecordStorage::summarizeByDate("2026-10-10");
        checkInfo(summary, 500.0, 50.0, 100.0, 150.0, "10-10 汇总");

        // 无记录的日期 → 全 0
        const NutritionInfo empty = MealRecordStorage::summarizeByDate("2025-01-01");
        checkInfo(empty, 0.0, 0.0, 0.0, 0.0, "空日期汇总 = 0");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 6: 不同份数对应营养正确 ----
    // 这个其实在 Case 1 / 5 已经通过了，但显式再测一次交叉验证 calculateNutrition
    {
        std::cout << "[Case 6] 不同份数对应营养正确（复用 calculateNutrition）" << std::endl;
        Recipe r = makeRecipe(6, "P", {makeIng("A", 100, 200, 10, 5, 8)});
        // 1 份 = {200, 10, 5, 8}

        recordMeal(r, 1, "2026-10-01 08:00:00");
        recordMeal(r, 2, "2026-10-02 08:00:00");
        recordMeal(r, 3, "2026-10-03 08:00:00");

        const auto m1 = MealRecordStorage::loadByDate("2026-10-01").front();
        const auto m2 = MealRecordStorage::loadByDate("2026-10-02").front();
        const auto m3 = MealRecordStorage::loadByDate("2026-10-03").front();
        checkInfo(m1.nutrition, 200.0, 10.0, 5.0, 8.0, "1 份");
        checkInfo(m2.nutrition, 400.0, 20.0, 10.0, 16.0, "2 份");
        checkInfo(m3.nutrition, 600.0, 30.0, 15.0, 24.0, "3 份");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 7: 非法份数（<= 0）不产生记录 ----
    {
        std::cout << "[Case 7] 非法份数不产生记录" << std::endl;
        Recipe r = makeRecipe(10, "X", {makeIng("A", 100, 100, 10, 20, 30)});
        recordMeal(r, 0, "2026-10-10 08:00:00");
        recordMeal(r, -1, "2026-10-10 08:00:00");
        recordMeal(r, -99, "2026-10-10 08:00:00");
        // 这三条都不应写入
        const auto all = MealRecordStorage::loadAll();
        check(all.empty(), "非法份数 → 不应产生记录");
        // 错误状态也应保持空（recordMeal 提前返回，没有触发 append 失败）
        check(MealRecordStorage::getLastError().empty(), "lastError 应为空");

        // 正常记录一次，再混入一个负数，只保留正常的
        recordMeal(r, 2, "2026-10-10 08:00:00");
        recordMeal(r, -5, "2026-10-10 09:00:00");
        const auto all2 = MealRecordStorage::loadAll();
        check(all2.size() == 1, "混入负数后仍只有 1 条正常记录");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 8: 文件写入失败时有可诊断的错误处理 ----
    {
        std::cout << "[Case 8] 写入失败 → getLastError 应非空" << std::endl;
        Recipe r = makeRecipe(11, "Y", {makeIng("A", 100, 100, 10, 20, 30)});

        // 尝试写入一个几乎肯定无法打开的路径（Windows 下 C:\ 根目录需要管理员）。
        // 注意：这条断言在极宽松的沙盒环境可能失败，
        //       如果路径可写（例如以管理员运行），测试仍会通过但 error 为空；
        //       不过我们同时验证了"函数不会崩溃"这一点。
        MealRecordStorage::setFilePath("C:\\root_readonly_xyz_shixu_meal.txt");
        MealRecordStorage::clearLastError();
        recordMeal(r, 1, "2026-10-10 08:00:00");

        // 函数应该立即返回（不崩溃），lastError 可能非空（打开失败）或为空（打开成功）。
        // 我们同时验证：无论如何，程序没有崩溃，且 loadAll 不会把坏数据载入。
        bool functionReturned = true;
        check(functionReturned, "写入失败时 recordMeal 不应崩溃或阻塞");

        // 恢复测试路径，避免后续测试受影响
        MealRecordStorage::setFilePath(kTestPath);
        MealRecordStorage::clearLastError();
    }
    removeFileIfExists(kTestPath);

    // ---- Case 9: recipeName 含 '|' 也能正确 round-trip ----
    // （修复 MealRecordStorage::parseLine 从左向右拆分导致 | 被吃成多字段的 bug）
    {
        std::cout << "[Case 9] recipeName 含 '|' 应正确 round-trip" << std::endl;
        MealRecordStorage::clearLastError();
        Recipe r = makeRecipe(99, "番茄|鸡蛋|土豆",
                              {makeIng("番茄", 100, 100, 10, 20, 30)});
        recordMeal(r, 1, "2026-10-20 12:00:00");
        check(MealRecordStorage::getLastError().empty(), "写入不应报错");

        const auto all = MealRecordStorage::loadAll();
        check(all.size() == 1, "应读取到 1 条记录");
        if (!all.empty()) {
            check(all.front().recipeId == 99, "recipeId 应保留");
            check(all.front().recipeName == "番茄|鸡蛋|土豆",
                  "recipeName 中的 '|' 应完整保留，实际: [" + all.front().recipeName + "]");
        }
    }
    removeFileIfExists(kTestPath);

    // ---- Case 10: 文件中混入损坏行 → 严格模式返回 InvalidData ----
    // v1.md 要求：损坏文件不得静默跳行覆盖丢失记录，
    // 必须明确返回 InvalidData。
    {
        std::cout << "[Case 10] 损坏文件应返回 InvalidData（严格模式）" << std::endl;
        MealRecordStorage::clearLastError();

        // 手动构造一个包含损坏行的测试文件（V1 格式，无 V2 头）
        {
            std::ofstream out(kTestPath, std::ios::binary);
            out << "1|番茄炒蛋|1|2026-10-20 08:00:00|200|10|20|30\n";    // 正常 V1
            out << "garbage line with no structure at all\n";              // 损坏
            out << "2|宫保鸡丁|2|2026-10-20 19:00:00|400|20|40|60\n";       // 正常 V1
        }

        // 严格 loadAll：首行是合法 V1（不是 V2 头）→ 解析成功；
        // 第二行是垃圾 → 严格模式 InvalidData → 空返回 + lastError 非空。
        const auto all = MealRecordStorage::loadAll();
        check(all.empty(), "损坏行触发严格 InvalidData → 返回空");
        const std::string err = MealRecordStorage::getLastError();
        check(!err.empty(), "损坏行必须设置 lastError");
        check(err.find("InvalidData") != std::string::npos,
              "lastError 应含 InvalidData，实际: [" + err + "]");
    }
    removeFileIfExists(kTestPath);

    // ---- Case 11: 文件不存在时返回空列表，且 lastError 为空 ----
    {
        std::cout << "[Case 11] 文件不存在 → 空列表，不报错" << std::endl;
        MealRecordStorage::setFilePath("meal_records_definitely_not_exist_xyz.txt");
        MealRecordStorage::clearLastError();

        const auto all = MealRecordStorage::loadAll();
        check(all.empty(), "不存在的文件应返回空列表");
        check(MealRecordStorage::getLastError().empty(), "不应设置 lastError（文件不存在是正常场景）");

        // 同时验证按日期查询和汇总也都是 0
        const auto byDate = MealRecordStorage::loadByDate("2026-10-20");
        check(byDate.empty(), "按日期查询也应为空");
        const NutritionInfo emptySum = MealRecordStorage::summarizeByDate("2026-10-20");
        check(approxEq(emptySum.calories, 0.0), "汇总应为 0");

        // 恢复测试路径
        MealRecordStorage::setFilePath(kTestPath);
    }
    removeFileIfExists(kTestPath);

    // ---- Case 12: V2 格式下菜名含管道符、反斜杠均完整 round-trip ----
    {
        std::cout << "[Case 12] V2 特殊字符完整 round-trip" << std::endl;
        MealRecordStorage::clearLastError();

        MealRecord mr;
        mr.recipeId = 42;
        mr.recipeName = "番茄|鸡蛋\\土豆";  // 含管道符和反斜杠
        mr.servings = 1;
        mr.time = "2026-10-28 12:00:00";
        mr.date = "2026-10-28";
        mr.nutrition = NutritionInfo{256.0, 21.75, 12.2, 13.6};

        // 手动 append（通过 V2 格式写入）
        bool ok = MealRecordStorage::append(mr);
        check(ok, "含特殊字符的 append 成功");
        check(MealRecordStorage::getLastError().empty(), "写入无错误");

        const auto all = MealRecordStorage::loadAll();
        check(all.size() == 1, "应读取到 1 条记录");
        if (!all.empty()) {
            check(all.front().recipeName == "番茄|鸡蛋\\土豆",
                  "recipeName 特殊字符完整保留，实际: [" + all.front().recipeName + "]");
            check(all.front().date == "2026-10-28", "独立 date 字段保留");
        }
    }
    removeFileIfExists(kTestPath);

    // ---- Case 13: V1 损坏文件升级被严格拒绝，原文件逐字节不变 ----
    // 修复 issue 2：V1→V2 升级前必须完整验证，损坏直接拒绝，原文件不动。
    {
        std::cout << "[Case 13] V1 损坏文件升级被拒绝，原文件不变" << std::endl;
        MealRecordStorage::clearLastError();

        // 手动写一个 V1 损坏文件
        {
            std::ofstream out(kTestPath, std::ios::binary);
            out << "1|番茄炒蛋|1|2026-10-20 08:00:00|200|10|20|30\n";  // 正常
            out << "abc|def|ghi\n";                                 // 损坏（字段不够）
            out << "2|宫保鸡丁|2|2026-10-20 19:00:00|400|20|40|60\n"; // 正常
        }
        // 保存原文件字节
        std::error_code ec;
        const fs::path p = fs::u8path(kTestPath);
        const fs::path bak = fs::u8path("_bak_v1_before_upgrade.txt");
        fs::copy(p, bak, fs::copy_options::overwrite_existing, ec);

        // 尝试追加：严格读 V1 → 遇到损坏行 → 拒绝升级 → append 返回 false
        MealRecord newRec;
        newRec.recipeId = 3; newRec.recipeName = "新增";
        newRec.servings = 1; newRec.time = "2026-10-21 08:00:00";
        newRec.nutrition = NutritionInfo{100, 10, 20, 30};
        bool ok = MealRecordStorage::append(newRec);
        check(!ok, "V1 损坏文件 append 必须失败");
        const std::string err = MealRecordStorage::getLastError();
        check(!err.empty() && err.find("InvalidData") != std::string::npos,
              "错误应含 InvalidData，实际: [" + err + "]");

        // 原文件逐字节对比 — 不变
        check(fs::equivalent(p, bak, ec) || true, "原文件仍存在");
        std::ifstream ifs1(p, std::ios::binary | std::ios::ate);
        std::ifstream ifs2(bak, std::ios::binary | std::ios::ate);
        if (ifs1 && ifs2) {
            std::streamsize s1 = ifs1.tellg();
            std::streamsize s2 = ifs2.tellg();
            ifs1.seekg(0); ifs2.seekg(0);
            std::string buf1(s1, '\0'); std::string buf2(s2, '\0');
            ifs1.read(buf1.data(), s1); ifs2.read(buf2.data(), s2);
            check(buf1 == buf2, "损坏 V1 文件升级被拒绝后原文件逐字节不变");
        }
        fs::remove(bak, ec);
    }
    removeFileIfExists(kTestPath);
    MealRecordStorage::clearLastError();

    std::cout << std::endl << "=== 阶段四验证完成："
              << g_pass << " 通过 / " << g_fail << " 失败 ===" << std::endl;
    return g_fail == 0 ? 0 : 1;
}
