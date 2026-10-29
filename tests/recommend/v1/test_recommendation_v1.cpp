#include "contracts/RecommendationContract.h"
#include <QCoreApplication>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace V = Shixu::Contracts::V1;
namespace D = Shixu::RecommendationEngine;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
V::OperationResult ok() { return {V::OperationStatus::Succeeded, V::ErrorCode::None, {}}; }
template <class T> V::QueryResult<T> unused() {
    throw std::runtime_error("Unexpected unrelated service call");
}
struct Recipes final : V::IRecipeService {
    V::QueryResult<std::vector<MealRecord>> getAllMeals() const override {
        return unused<std::vector<MealRecord>>();
    }
    std::vector<Recipe> data;
    bool fail = false;
    V::QueryResult<std::vector<Recipe>> getAllRecipes() const override {
        return {fail ? V::OperationResult{V::OperationStatus::Failed, V::ErrorCode::StorageFailure,
                                          "菜谱读取失败"}
                     : ok(),
                data};
    }
    V::QueryResult<Recipe> findRecipeById(int) const override { return unused<Recipe>(); }
    V::QueryResult<std::vector<MealRecord>> getMealsByDate(const std::string &) const override {
        return unused<std::vector<MealRecord>>();
    }
    V::QueryResult<NutritionInfo> getNutritionByDate(const std::string &) const override {
        return unused<NutritionInfo>();
    }
    V::MealWriteResult recordMealOnce(const V::MealWriteRequest &) override {
        throw std::runtime_error("D must never write meals");
    }
};
// Tests supply B's assessed states. This double does not implement B's stock rules.
struct Stock final : V::IInventoryService {
    std::vector<V::IngredientAvailability> answers;
    mutable std::vector<V::EvaluationRequest> requests;
    bool fail = false;
    bool enoughByDefault = false;
    bool omit = false;
    V::QueryResult<V::InventorySnapshot> getInventoryBatches() const override {
        return unused<V::InventorySnapshot>();
    }
    V::QueryResult<std::vector<V::IngredientAvailability>>
    evaluateRequirements(const V::EvaluationRequest &r) const override {
        requests.push_back(r);
        if (fail)
            return {{V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "库存评估失败"}, {}};
        std::vector<V::IngredientAvailability> result;
        for (const auto &requirement : r.requirements) {
            auto a = V::IngredientAvailability{requirement.foodName,
                                               requirement.requiredGrams,
                                               enoughByDefault ? requirement.requiredGrams : 0,
                                               enoughByDefault ? 0 : requirement.requiredGrams,
                                               false,
                                               false,
                                               enoughByDefault ? V::AvailabilityState::Enough
                                                               : V::AvailabilityState::Insufficient};
            for (const auto &configured : answers)
                if (configured.foodName == requirement.foodName) {
                    a = configured;
                    a.requiredGrams = requirement.requiredGrams;
                }
            result.push_back(a);
        }
        if (omit && !result.empty())
            result.pop_back();
        return {ok(), result};
    }
    V::QueryResult<V::ConsumptionResult> getConsumption(const std::string &) const override {
        return unused<V::ConsumptionResult>();
    }
    V::ConsumptionPreview previewConsumption(const V::ConsumptionRequest &) const override {
        throw std::runtime_error("D must never preview consumption");
    }
    V::ConsumptionResult consumeForCooking(const V::ConsumptionRequest &, const std::string &) override {
        throw std::runtime_error("D must never consume stock");
    }
};
Recipe recipe(int id, int minutes = 20, std::vector<std::string> tags = {}, std::string food = "番茄",
              std::string category = "菜") {
    Recipe r;
    r.id = id;
    r.name = "菜谱" + std::to_string(id);
    r.cookingTimeMinutes = minutes;
    r.tags = std::move(tags);
    r.category = std::move(category);
    r.steps = {"清洗", "制作"};
    r.ingredients = {{std::move(food), 100, {100, 20, 5, 2}}};
    return r;
}
D::UserContext context(V::Mood mood = V::Mood::Tired, int cap = 30) {
    D::UserContext c;
    c.preferences = V::RecommendationPreferences{};
    c.preferences->mood = mood;
    c.preferences->maxPreparationMinutes = cap;
    c.preferences->referenceDate = "2026-10-28";
    c.inventorySnapshot.revision = "snapshot-42";
    c.inventorySnapshot.batches = {{101, "番茄", 10, "个", "2026-10-30", 150, 1500}};
    return c;
}
std::vector<int> ids(const V::RecommendationResult &result) {
    std::vector<int> v;
    for (const auto &card : result.cards)
        v.push_back(card.recipeId);
    return v;
}
void validCards(const V::RecommendationResult &result, int cap) {
    check(result.outcome.status == V::OperationStatus::Succeeded, "Success status");
    check(result.cards.size() <= 3, "At most three cards");
    std::set<int> seen;
    for (std::size_t i = 0; i < result.cards.size(); ++i) {
        const auto &c = result.cards[i];
        check(seen.insert(c.recipeId).second, "Unique IDs");
        check(c.role == static_cast<V::CardRole>(i), "Ordered roles");
        check(c.recipeId == c.recipe.id && c.dishName.toStdString() == c.recipe.name,
              "Original recipe preserved");
        check(c.servings == 1 && c.effectiveMaxPreparationMinutes == cap, "V1 fields");
        check(c.recipe.cookingTimeMinutes <= cap, "Hard time constraint");
        check(c.reasons.size() >= 1 && c.reasons.size() <= 2, "One or two reasons");
        check(std::isfinite(c.totalScore) && c.totalScore >= 0 && c.totalScore <= 100, "Finite score");
        check(!c.reasons.join(";").contains(QStringLiteral("已经消耗")), "No false consumption claim");
    }
}
void timesAndValidation() {
    Recipes r;
    Stock b;
    auto service = V::createRecommendationService(r, b);
    const std::pair<V::Mood, int> defaults[] = {{V::Mood::Tired, 30},
                                                {V::Mood::WantGoodFood, 45},
                                                {V::Mood::LowAppetite, 30},
                                                {V::Mood::WantLight, 40},
                                                {V::Mood::TreatMyself, 75}};
    for (const auto &entry : defaults) {
        r.data = {recipe(1, entry.second), recipe(2, entry.second + 1)};
        auto result = service->recommend(context(entry.first, entry.second));
        validCards(result, entry.second);
        check(ids(result) == std::vector<int>{1}, "Default boundary");
    }
    r.data = {recipe(1, 60), recipe(2, 61)};
    check(ids(service->recommend(context(V::Mood::Tired, 60))) == std::vector<int>{1},
          "Tired user can increase cap to 60");
    r.data = {recipe(1, 30), recipe(2, 31)};
    check(ids(service->recommend(context(V::Mood::TreatMyself, 30))) == std::vector<int>{1},
          "Treat user can lower cap to 30");
    auto base = context();
    for (int scenario = 0; scenario < 9; ++scenario) {
        auto c = base;
        switch (scenario) {
        case 0:
            c.preferences.reset();
            break;
        case 1:
            c.preferences->mood = V::Mood::NotSelected;
            break;
        case 2:
            c.preferences->maxPreparationMinutes = 0;
            break;
        case 3:
            c.preferences->servings = 2;
            break;
        case 4:
            c.preferences->referenceDate = "2026-02-30";
            break;
        case 5:
            c.preferences->inventoryPolicy = static_cast<V::InventoryPolicy>(99);
            break;
        case 6:
            c.preferences->refinement = static_cast<V::Refinement>(99);
            break;
        case 7:
            c.targetCaloriesPerServing = std::numeric_limits<double>::quiet_NaN();
            break;
        case 8:
            c.inventorySnapshot.revision.clear();
            break;
        }
        auto result = service->recommend(c);
        check(result.outcome.status == V::OperationStatus::Failed &&
                  result.outcome.errorCode == V::ErrorCode::InvalidArgument,
              "Invalid conditions fail");
    }
}
void policiesAndUnknown() {
    Recipes r;
    Stock b;
    r.data = {recipe(1)};
    auto s = V::createRecommendationService(r, b);
    auto c = context();
    b.answers = {{"番茄", 100, 50, 50, true, true, V::AvailabilityState::Unknown}};
    auto unknown = s->recommend(c);
    validCards(unknown, 30);
    check(unknown.cards.size() == 1 &&
              unknown.cards[0].availability[0].state == V::AvailabilityState::Unknown,
          "Keep unknown state");
    auto reason = unknown.cards[0].reasons.join(";");
    check(reason.contains(QStringLiteral("重量未确认")) && !reason.contains(QStringLiteral("均能确认充足")),
          "Honest unknown reason");
    c.preferences->inventoryPolicy = V::InventoryPolicy::SmallPurchase;
    check(s->recommend(c).cards.empty(), "Unknown not certain purchase");
    c.preferences->inventoryPolicy = V::InventoryPolicy::OnlyInventory;
    check(s->recommend(c).cards.empty(), "Strict rejects unknown");
    b.answers = {{"番茄", 100, 100, 0, true, true, V::AvailabilityState::Enough}};
    check(s->recommend(c).cards.size() == 1, "Trust B Enough despite unknown extra batch");
    check(s->recommend(c).cards[0].expiryUrgencyScore == 100, "Use B expiry flag");
    b.answers.clear();
    r.data[0].ingredients.push_back({"食用油", 10, {}});
    c.preferences->inventoryPolicy = V::InventoryPolicy::SmallPurchase;
    auto two = s->recommend(c);
    check(two.cards.size() == 1 && two.cards[0].availability.size() == 2,
          "Two shortages allowed including oil");
    r.data[0].ingredients.push_back({"鸡蛋", 100, {}});
    check(s->recommend(c).cards.empty(), "Three shortages excluded");
    c.preferences->inventoryPolicy = V::InventoryPolicy::PreferInventory;
    c.inventorySnapshot.batches.clear();
    check(s->recommend(c).cards.size() == 1, "Empty inventory still gets C full recipes");
    r.data[0].ingredients.push_back({"番茄", 25, {}});
    s->recommend(c);
    check(b.requests.back().requirements.size() == 3 && b.requests.back().requirements[0].requiredGrams > 0,
          "Merged demand set");
    const auto &req = b.requests.back();
    auto tomato = std::find_if(req.requirements.begin(), req.requirements.end(),
                               [](const auto &x) { return x.foodName == "番茄"; });
    check(tomato != req.requirements.end() && tomato->requiredGrams == 125, "Duplicate ingredient summed");
}
void batchesAndRefinements() {
    Recipes r;
    Stock b;
    b.enoughByDefault = true;
    for (int i = 1; i <= 8; ++i)
        r.data.push_back(recipe(i, 20, {V::Tags::Quick, V::Tags::Easy}));
    r.data.push_back(r.data.front());
    auto s = V::createRecommendationService(r, b);
    auto c = context();
    auto first = s->recommend(c);
    validCards(first, 30);
    check(first.hasMore, "More unseen candidates");
    check(ids(first) == ids(s->recommend(c)), "Stable ties");
    std::set<int> seen;
    for (int batch = 0; batch < 3; ++batch) {
        auto out = s->recommend(c);
        validCards(out, 30);
        check(out.cards.size() == std::size_t(batch < 2 ? 3 : 2), "Never pad cards");
        check(out.hasMore == (batch < 2), "hasMore exact");
        for (const auto &card : out.cards) {
            check(seen.insert(card.recipeId).second, "No repeats across batches");
            c.preferences->excludedRecipeIds.push_back(card.recipeId);
        }
    }
    auto end = s->recommend(c);
    check(end.cards.empty() && !end.hasMore && !end.outcome.message.empty(),
          "Exhaustion success with message");
    c.preferences->excludedRecipeIds.clear();
    check(ids(s->recommend(c)) == ids(first), "Reset starts first batch");
    r.data = {recipe(1, 20, {V::Tags::Noodle}), recipe(2, 20, {V::Tags::Easy, V::Tags::Quick})};
    c.preferences->refinement = V::Refinement::NoNoodles;
    check(ids(s->recommend(c)) == std::vector<int>{2}, "NoNoodles is hard exclusion");
    c.preferences->inventoryPolicy = V::InventoryPolicy::OnlyInventory;
    c.preferences->refinement = V::Refinement::Simpler;
    check(s->recommend(c).cards[0].recipeId == 2, "Simpler adjusts ranking without clearing policy");
    c.preferences->refinement = V::Refinement::None;
    check(s->recommend(c).cards.size() == 2, "Toggle off restores candidates");
    c.preferences->refinements = {V::Refinement::Simpler, V::Refinement::NoNoodles, V::Refinement::Simpler};
    auto combined = s->recommend(c);
    check(combined.cards.size() == 1 && combined.cards[0].recipeId == 2 && combined.eligibleCount == 1,
          "stacked refinements filter and count");
    c.preferences->refinement = V::Refinement::Simpler;
    check(std::abs(s->recommend(c).cards[0].totalScore - combined.cards[0].totalScore) < 1e-8,
          "legacy and set deduplicate");
    c.excludedIngredients = {QStringLiteral("番茄")};
    check(s->recommend(c).cards.empty(), "Legacy exclusions remain hard");
}
void moodsAndRoles() {
    Recipes r;
    Stock b;
    b.enoughByDefault = true;
    r.data = {recipe(1, 20, {V::Tags::Easy, V::Tags::Quick}),
              recipe(2, 20, {V::Tags::Hearty, V::Tags::Appetizing}),
              recipe(3, 20, {V::Tags::Light, V::Tags::Appetizing}),
              recipe(4, 20, {V::Tags::Light, V::Tags::VegetableRich, V::Tags::HighProtein}),
              recipe(5, 20, {V::Tags::Hearty, V::Tags::Special})};
    auto s = V::createRecommendationService(r, b);
    for (int mood = 1; mood <= 5; ++mood) {
        auto c = context(static_cast<V::Mood>(mood), 75);
        auto out = s->recommend(c);
        validCards(out, 75);
        check(out.cards[0].recipeId == mood, "Five moods have explainable different winners");
    }
    const std::pair<V::Refinement, const char *> choices[] = {
        {V::Refinement::Heartier, V::Tags::Hearty},
        {V::Refinement::Lighter, V::Tags::Light},
        {V::Refinement::MoreAppetizing, V::Tags::Appetizing},
        {V::Refinement::MoreVegetables, V::Tags::VegetableRich},
        {V::Refinement::MoreProtein, V::Tags::HighProtein},
        {V::Refinement::MoreSpecial, V::Tags::Special}};
    for (const auto &choice : choices) {
        r.data = {recipe(1), recipe(2, 20, {choice.second})};
        auto c = context();
        c.preferences->refinement = choice.first;
        check(s->recommend(c).cards[0].recipeId == 2, "Each refinement boosts actual tag");
    }
    // Primary 1; alternative 2 can be quicker; third should diversify away from tomato.
    r.data = {recipe(1, 10, {V::Tags::Quick, V::Tags::Easy}), recipe(2, 11, {V::Tags::Quick, V::Tags::Easy}),
              recipe(3, 12, {V::Tags::Quick, V::Tags::Easy}),
              recipe(4, 20, {V::Tags::Quick, V::Tags::Easy}, "鸡蛋", "汤")};
    auto out = s->recommend(context());
    check(out.cards[0].recipeId == 1 && out.cards[1].recipeId == 2 && out.cards[2].recipeId == 4,
          "Cards not simple top three scores");
}
void isolationAndFailures() {
    Recipes r;
    Stock b;
    r.data = {recipe(1)};
    auto s = V::createRecommendationService(r, b);
    auto c = context();
    c.mood = D::Mood::NotSelected;
    c.maxPreparationMinutes = 1;
    c.servings = 99;
    c.topK = 0;
    c.inventory = {{"", "虚构库存", 999999}};
    c.expiringIngredients = {QStringLiteral("番茄")};
    auto result = s->recommend(c);
    check(result.cards.size() == 1 && result.cards[0].expiryUrgencyScore == 0, "Ignore old conditions/stock");
    check(b.requests[0].inventory.revision == "snapshot-42" &&
              b.requests[0].inventory.batches[0].batchId == 101,
          "Same caller snapshot sent to B");
    b.fail = true;
    result = s->recommend(c);
    check(result.outcome.status == V::OperationStatus::Failed &&
              result.outcome.errorCode == V::ErrorCode::StorageFailure && result.cards.empty(),
          "B failure not empty success");
    b.fail = false;
    r.fail = true;
    check(s->recommend(c).outcome.errorCode == V::ErrorCode::StorageFailure, "C error propagated");
    r.fail = false;
    b.omit = true;
    check(s->recommend(c).outcome.errorCode == V::ErrorCode::InvalidData, "Incomplete B result rejected");
    b.omit = false;
    r.data[0].ingredients[0].amountGrams = std::numeric_limits<double>::infinity();
    check(s->recommend(c).outcome.errorCode == V::ErrorCode::InvalidData, "Invalid recipe rejected");
    r.data = {recipe(1)};
    c.targetCaloriesPerServing = 100;
    result = s->recommend(c);
    check(result.cards[0].nutritionScoreAvailable && result.cards[0].nutritionScore == 100,
          "C nutrition formula reused");
}
void honestReasons() {
    Recipes r;
    Stock b;
    b.enoughByDefault = true;
    auto service = V::createRecommendationService(r, b);
    // Reviewer reproduction: only appetizing/hearty, never high-protein.
    for (const auto &tags :
         {std::vector<std::string>{V::Tags::Appetizing, V::Tags::Hearty}, std::vector<std::string>{}}) {
        r.data = {recipe(1, 20, tags), recipe(2, 20, tags), recipe(3, 20, tags)};
        for (int mood = 1; mood <= 5; ++mood) {
            auto result = service->recommend(context(static_cast<V::Mood>(mood), 75));
            validCards(result, 75);
            check(result.cards.size() == 3, "Reproduce all three roles");
            for (const auto &card : result.cards) {
                const auto reason = card.reasons.join(";");
                for (const char *absent : {"高蛋白", "蔬菜丰富", "清淡", "快手", "简单", "丰盛特色"})
                    check(!reason.contains(QString::fromUtf8(absent)),
                          "No absent-tag claims for any mood/role");
                check(!reason.contains(QStringLiteral("主要食材与前两道不同")) &&
                          !reason.contains(QStringLiteral("类别与前两道不同")),
                      "No fabricated diversity");
                if (tags.empty() && card.role != V::CardRole::Primary)
                    check(reason.contains(QStringLiteral("未命中该备选侧重标签")),
                          "Explain fallback selection");
            }
        }
    }
    r.data = {recipe(1, 20, {V::Tags::HighProtein}), recipe(2, 20, {V::Tags::VegetableRich}),
              recipe(3, 20, {V::Tags::Light})};
    auto result = service->recommend(context(V::Mood::WantLight, 40));
    for (const auto &card : result.cards) {
        const auto reason = card.reasons.join(";");
        for (const auto &pair : {std::pair<const char *, const char *>{V::Tags::HighProtein, "高蛋白"},
                                 {V::Tags::VegetableRich, "蔬菜丰富"},
                                 {V::Tags::Light, "清淡"}}) {
            const bool present = std::find(card.recipe.tags.begin(), card.recipe.tags.end(), pair.first) !=
                                 card.recipe.tags.end();
            check(reason.contains(QString::fromUtf8(pair.second)) == present,
                  "Labels follow each selected recipe");
        }
    }
}
QString outputExample() {
    Recipes r;
    Stock b;
    r.data = {recipe(101, 10, {V::Tags::Quick, V::Tags::Easy}, "番茄", "清炒"),
              recipe(102, 25, {V::Tags::Hearty, V::Tags::Appetizing}, "鸡蛋", "主菜"),
              recipe(103, 20, {V::Tags::Light, V::Tags::Appetizing}, "黄瓜", "凉拌"),
              recipe(104, 30, {V::Tags::Light, V::Tags::VegetableRich, V::Tags::HighProtein}, "青菜", "清炒"),
              recipe(105, 60, {V::Tags::Hearty, V::Tags::Special}, "牛肉", "炖菜"),
              recipe(106, 15, {V::Tags::Quick, V::Tags::Easy, V::Tags::Noodle}, "面条", "面食")};
    const char *names[] = {"快手番茄测试菜", "下饭鸡蛋测试菜", "清淡黄瓜测试菜",
                           "蔬菜蛋白测试菜", "丰盛牛肉测试菜", "简单面条测试菜"};
    for (std::size_t i = 0; i < r.data.size(); ++i)
        r.data[i].name = names[i];
    b.answers = {{"番茄", 100, 150, 0, false, true, V::AvailabilityState::Enough},
                 {"鸡蛋", 100, 120, 0, false, false, V::AvailabilityState::Enough},
                 {"黄瓜", 100, 20, 80, true, false, V::AvailabilityState::Unknown},
                 {"青菜", 100, 150, 0, false, false, V::AvailabilityState::Enough}};
    auto service = V::createRecommendationService(r, b);
    QString report = QStringLiteral(
        "D 第一轮推荐输出演示\n\n"
        "说明：以下菜谱、标签、营养和库存评估都是测试替身提供的合成数据，调用的是真实 V1 推荐算法。\n"
        "这不是 B/C 真实数据联调，不会修改个人库存，也不是最终界面。\n"
        "库存评估样例：番茄充足且临期；鸡蛋、青菜充足；黄瓜已知20g且另有未知重量；牛肉、面条不足。\n\n");
    auto append = [&](const QString &title, const D::UserContext &c) {
        const auto result = service->recommend(c);
        report += QStringLiteral("========== %1 ==========\n最终时间上限：%2 分钟\n")
                      .arg(title)
                      .arg(c.preferences->maxPreparationMinutes);
        check(result.outcome.status == V::OperationStatus::Succeeded, "Output example must succeed");
        for (const auto &card : result.cards) {
            const QString role = card.role == V::CardRole::Primary          ? QStringLiteral("今晚首选")
                                 : card.role == V::CardRole::AlternativeOne ? QStringLiteral("备选一")
                                                                            : QStringLiteral("备选二");
            report += QStringLiteral("\n%1：%2（ID %3）\n制作时间：%4 分钟；综合分：%5\n"
                                     "库存匹配：%6；临期利用：%7；心情：%8；时间：%9\n")
                          .arg(role, card.dishName)
                          .arg(card.recipeId)
                          .arg(card.recipe.cookingTimeMinutes)
                          .arg(card.totalScore, 0, 'f', 1)
                          .arg(card.ingredientMatchScore, 0, 'f', 1)
                          .arg(card.expiryUrgencyScore, 0, 'f', 1)
                          .arg(card.moodMatchScore, 0, 'f', 1)
                          .arg(card.preparationTimeScore, 0, 'f', 1);
            for (const auto &a : card.availability) {
                const QString state = a.state == V::AvailabilityState::Enough    ? QStringLiteral("充足")
                                      : a.state == V::AvailabilityState::Unknown ? QStringLiteral("未知")
                                                                                 : QStringLiteral("不足");
                report += QStringLiteral("食材：%1，%2；需求%3g，已知可用%4g，已知差额%5g\n")
                              .arg(QString::fromUtf8(a.foodName.c_str()), state)
                              .arg(a.requiredGrams)
                              .arg(a.knownAvailableGrams)
                              .arg(a.knownGapGrams);
            }
            report += QStringLiteral("推荐理由：%1\n").arg(card.reasons.join(QStringLiteral("；")));
        }
        report += QStringLiteral("\n还有下一批：%1\n%2\n\n")
                      .arg(result.hasMore ? QStringLiteral("有") : QStringLiteral("无"),
                           QString::fromUtf8(result.outcome.message.c_str()));
        return result;
    };
    const char *moods[] = {"很累", "想吃点好的", "没胃口", "想清爽一点", "想犒劳自己"};
    const int caps[] = {30, 45, 30, 40, 75};
    for (int i = 0; i < 5; ++i)
        append(QString::fromUtf8(moods[i]), context(static_cast<V::Mood>(i + 1), caps[i]));
    auto c = context();
    c.preferences->refinement = V::Refinement::NoNoodles;
    append(QStringLiteral("很累 + 不想吃面"), c);
    c.preferences->inventoryPolicy = V::InventoryPolicy::OnlyInventory;
    append(QStringLiteral("很累 + 不想吃面 + 只用家里的"), c);
    c = context(V::Mood::TreatMyself, 75);
    c.preferences->inventoryPolicy = V::InventoryPolicy::SmallPurchase;
    append(QStringLiteral("想犒劳自己 + 少量补购（黄瓜未知不能冒充补购）"), c);
    c = context(V::Mood::Tired, 60);
    auto first = append(QStringLiteral("很累但用户将时间提高到60分钟"), c);
    for (const auto &card : first.cards)
        c.preferences->excludedRecipeIds.push_back(card.recipeId);
    append(QStringLiteral("保持条件，换一批（不重复上一批ID）"), c);
    return report;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        timesAndValidation();
        policiesAndUnknown();
        batchesAndRefinements();
        moodsAndRoles();
        isolationAndFailures();
        honestReasons();
        if (app.arguments().contains(QStringLiteral("--show-output"))) {
            const auto report = outputExample().toUtf8();
            std::cout.write(report.constData(), report.size());
            const int option = app.arguments().indexOf(QStringLiteral("--output"));
            if (option >= 0) {
                check(option + 1 < app.arguments().size(), "--output requires filename");
                QFile file(app.arguments().at(option + 1));
                check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Cannot open output report");
                check(file.write(report) == report.size() && file.flush(), "Cannot save output report");
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "D01-D05 recommendation V1 business checks passed\n";
}
