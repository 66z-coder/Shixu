#include "contracts/ActivityContracts.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace Shixu::Contracts::V1 {
namespace {
bool ok(const OperationResult &r) {
    return r.status == OperationStatus::Succeeded || r.status == OperationStatus::AlreadyApplied;
}
OperationResult success() { return {OperationStatus::Succeeded, ErrorCode::None, {}}; }
QueryResult<CookingActivitySnapshot> error(ErrorCode code, const char *message) {
    return {{OperationStatus::Failed, code, message}, {}};
}
bool timestamp(const QString &s) {
    const auto d = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
    return d.isValid() && d.toString("yyyy-MM-dd HH:mm:ss") == s;
}
bool sameMeal(const MealRecord &a, const CookingSnapshot &b) {
    const auto &n = b.nutrition;
    return a.recipeId == b.recipe.id && a.recipeName == b.recipe.name && a.servings == b.request.servings &&
           a.time == b.request.completedAt && a.date == b.request.completedAt.substr(0, 10) &&
           a.nutrition.calories == n.calories && a.nutrition.protein == n.protein &&
           a.nutrition.carbohydrates == n.carbohydrates && a.nutrition.fat == n.fat;
}
const std::set<std::string> codes = {"first_cooking", "cooking_streak_3", "expiring_3", "recipes_5"};
class ActivityService final : public ICookingActivityService {
    IRecipeService &recipes;
    IInventoryService &inventory;
    ICookingCompletionService &completions;
    QString path;

  public:
    ActivityService(IRecipeService &c, IInventoryService &b, ICookingCompletionService &coordinator,
                    const std::string &file)
        : recipes(c), inventory(b), completions(coordinator),
          path(QFileInfo(QString::fromUtf8(file.c_str())).absoluteFilePath()) {}
    QueryResult<CookingActivitySnapshot> refresh(const LocalDate &referenceDate) override {
        const auto dateText = QString::fromStdString(referenceDate);
        const QDate today = QDate::fromString(dateText, Qt::ISODate);
        if (!today.isValid() || today.toString(Qt::ISODate) != dateText)
            return error(ErrorCode::InvalidArgument, "统计日期无效。");
        QLockFile lock(path + ".lock");
        if (!lock.tryLock(0))
            return error(ErrorCode::StorageFailure, "成长成就文件无法访问或正被使用，请稍后重试。");
        std::map<std::string, std::string> unlocked;
        if (QFileInfo::exists(path)) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return error(ErrorCode::StorageFailure, "无法读取成长成就文件。");
            const auto bytes = file.readAll();
            if (file.error() != QFileDevice::NoError)
                return error(ErrorCode::StorageFailure, "读取成长成就文件失败。");
            QJsonParseError parse;
            const auto doc = QJsonDocument::fromJson(bytes, &parse);
            if (parse.error != QJsonParseError::NoError || !doc.isObject() ||
                doc.object().value("version").toInt() != 1 || !doc.object().value("unlocked").isArray())
                return error(ErrorCode::InvalidData, "成长成就文件已损坏，请保留原文件。");
            for (const auto &item : doc.object().value("unlocked").toArray()) {
                const auto object = item.toObject();
                const auto code = object.value("code").toString().toStdString();
                const auto at = object.value("unlockedAt").toString();
                if (!item.isObject() || !codes.count(code) || unlocked.count(code) || !timestamp(at))
                    return error(ErrorCode::InvalidData, "成长成就记录无效，请保留原文件。");
                unlocked[code] = at.toStdString();
            }
        }
        // Completed rows are immutable. Read them before C so concurrent completions cannot
        // appear here without their already committed meal record.
        auto completed = completions.getCompletedCompletions();
        if (!ok(completed.outcome))
            return {completed.outcome, {}};
        auto meals = recipes.getAllMeals();
        if (!ok(meals.outcome))
            return {meals.outcome, {}};
        CookingActivitySnapshot out;
        std::map<EventId, MealRecord> byEvent;
        for (const auto &meal : meals.value) {
            if (meal.eventId.empty()) {
                out.legacyMeals.push_back(meal);
                continue;
            }
            if (!byEvent.emplace(meal.eventId, meal).second)
                return error(ErrorCode::InvalidData, "饮食记录包含重复的事件编号。");
        }
        std::sort(completed.value.begin(), completed.value.end(), [](const auto &a, const auto &b) {
            return std::tie(a.snapshot.request.completedAt, a.progress.eventId) <
                   std::tie(b.snapshot.request.completedAt, b.progress.eventId);
        });
        std::set<EventId> seen;
        std::set<RecipeId> recipeIds;
        std::set<QDate> cookingDates;
        QDate previous;
        int streak = 0;
        auto unlock = [&](const std::string &code, const std::string &at) {
            if (!unlocked.count(code)) {
                unlocked[code] = at;
                out.newlyUnlocked.push_back(code);
            }
        };
        for (const auto &detail : completed.value) {
            const auto &id = detail.progress.eventId;
            const auto &snapshot = detail.snapshot;
            if (detail.progress.stage != CompletionStage::Completed || id.empty() ||
                id != snapshot.request.eventId || !seen.insert(id).second ||
                !timestamp(QString::fromStdString(snapshot.request.completedAt)))
                return error(ErrorCode::InvalidData, "已完成做饭记录不一致。");
            const auto meal = byEvent.find(id);
            if (meal == byEvent.end() || !sameMeal(meal->second, snapshot))
                return error(ErrorCode::InvalidData, "做饭事件与饮食记录不一致，暂时无法统计。");
            const auto consumed = inventory.getConsumption(id);
            if (!ok(consumed.outcome))
                return {consumed.outcome, {}};
            if (consumed.value.eventId != id || !ok(consumed.value.outcome))
                return error(ErrorCode::InvalidData, "库存消费记录与做饭事件不一致。");
            const bool expiring =
                std::any_of(consumed.value.consumed.begin(), consumed.value.consumed.end(),
                            [](const auto &batch) { return batch.consumedGrams > 0 && batch.wasExpiring; });
            if (expiring != consumed.value.consumedExpiring)
                return error(ErrorCode::InvalidData, "临期消耗记录不一致。");
            ++out.totalCookingCount;
            if (expiring)
                ++out.expiringCookingCount;
            out.recentMeals.push_back(meal->second);
            recipeIds.insert(meal->second.recipeId);
            const QDate day = QDate::fromString(QString::fromStdString(meal->second.date), Qt::ISODate);
            if (cookingDates.insert(day).second) {
                streak = previous.isValid() && previous.addDays(1) == day ? streak + 1 : 1;
                previous = day;
                out.bestCookingStreak = std::max(out.bestCookingStreak, streak);
            }
            unlock("first_cooking", meal->second.time);
            if (streak >= 3)
                unlock("cooking_streak_3", meal->second.time);
            if (out.expiringCookingCount >= 3)
                unlock("expiring_3", meal->second.time);
            if (recipeIds.size() >= 5)
                unlock("recipes_5", meal->second.time);
        }
        QDate cursor = cookingDates.count(today) ? today : today.addDays(-1);
        while (cookingDates.count(cursor)) {
            ++out.currentCookingStreak;
            cursor = cursor.addDays(-1);
        }
        auto newest = [](const MealRecord &a, const MealRecord &b) {
            return std::tie(a.time, a.eventId) > std::tie(b.time, b.eventId);
        };
        std::sort(out.recentMeals.begin(), out.recentMeals.end(), newest);
        if (out.recentMeals.size() > 10)
            out.recentMeals.resize(10);
        std::stable_sort(out.legacyMeals.begin(), out.legacyMeals.end(), newest);
        if (!out.newlyUnlocked.empty()) {
            QJsonArray array;
            for (const auto &item : unlocked)
                array.append(QJsonObject{{"code", QString::fromStdString(item.first)},
                                         {"unlockedAt", QString::fromStdString(item.second)}});
            const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"unlocked", array}})
                                   .toJson(QJsonDocument::Compact);
            QSaveFile file(path);
            file.setDirectWriteFallback(false);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
                return error(ErrorCode::StorageFailure, "成就保存失败，尚未颁发新成就，请重试。");
        }
        for (const auto &item : unlocked)
            out.achievements.push_back({item.first, item.second});
        return {success(), std::move(out)};
    }
};
} // namespace
std::unique_ptr<ICookingActivityService> createCookingActivityService(IRecipeService &c, IInventoryService &b,
                                                                      ICookingCompletionService &coordinator,
                                                                      const std::string &path) {
    return std::make_unique<ActivityService>(c, b, coordinator, path);
}
} // namespace Shixu::Contracts::V1
