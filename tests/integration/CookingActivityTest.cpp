#include "contracts/ActivityContracts.h"
#include "contracts/ServiceFactories.h"
#include "inventory/inventory.h"
#include "recipe/RecipeService.h"
#include "reward/RewardSystem.h"
#include "ui/StatisticsWidget.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace V = Shixu::Contracts::V1;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
bool ok(const V::OperationResult &r) {
    return r.status == V::OperationStatus::Succeeded || r.status == V::OperationStatus::AlreadyApplied;
}
struct Recipes : V::IRecipeService {
    V::IRecipeService &real;
    bool writeFail = false, readFail = false, missing = false;
    explicit Recipes(V::IRecipeService &r) : real(r) {}
    V::QueryResult<std::vector<Recipe>> getAllRecipes() const override { return real.getAllRecipes(); }
    V::QueryResult<Recipe> findRecipeById(int id) const override { return real.findRecipeById(id); }
    V::QueryResult<std::vector<MealRecord>> getAllMeals() const override {
        if (readFail)
            return {{V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "meal read failure"}, {}};
        auto r = real.getAllMeals();
        if (missing)
            r.value.clear();
        return r;
    }
    V::QueryResult<std::vector<MealRecord>> getMealsByDate(const std::string &d) const override {
        return real.getMealsByDate(d);
    }
    V::QueryResult<NutritionInfo> getNutritionByDate(const std::string &d) const override {
        return real.getNutritionByDate(d);
    }
    V::MealWriteResult recordMealOnce(const V::MealWriteRequest &r) override {
        if (!writeFail)
            return real.recordMealOnce(r);
        V::MealWriteResult result;
        result.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "meal write failure"};
        return result;
    }
};
struct Stock : V::IInventoryService {
    V::IInventoryService &real;
    bool fail = false;
    explicit Stock(V::IInventoryService &r) : real(r) {}
    V::QueryResult<V::InventorySnapshot> getInventoryBatches() const override {
        return real.getInventoryBatches();
    }
    V::QueryResult<std::vector<V::IngredientAvailability>>
    evaluateRequirements(const V::EvaluationRequest &r) const override {
        return real.evaluateRequirements(r);
    }
    V::QueryResult<V::ConsumptionResult> getConsumption(const std::string &id) const override {
        if (fail)
            return {{V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "inventory read failure"}, {}};
        return real.getConsumption(id);
    }
    V::ConsumptionPreview previewConsumption(const V::ConsumptionRequest &r) const override {
        return real.previewConsumption(r);
    }
    V::ConsumptionResult consumeForCooking(const V::ConsumptionRequest &r, const std::string &t) override {
        return real.consumeForCooking(r, t);
    }
};
struct Rewards : V::IRewardService {
    V::IRewardService &real;
    bool fail = false;
    explicit Rewards(V::IRewardService &r) : real(r) {}
    V::CookingRewardResult applyCookingRewardOnce(const V::CookingRewardRequest &r) override {
        if (!fail)
            return real.applyCookingRewardOnce(r);
        V::CookingRewardResult out;
        out.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "reward write failure"};
        return out;
    }
    V::QueryResult<V::GrowthSnapshot> getGrowthSnapshot(const std::string &date) const override {
        return real.getGrowthSnapshot(date);
    }
};
QByteArray readFile(const QString &path) {
    QFile f(path);
    check(f.open(QIODevice::ReadOnly), "read file");
    return f.readAll();
}
void writeFile(const QString &path, const QByteArray &bytes) {
    QFile f(path);
    check(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "write file");
    check(f.write(bytes) == bytes.size(), "write all");
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        QTemporaryDir dir(QDir::currentPath() + "/activity-XXXXXX");
        check(dir.isValid(), "temporary directory");
        const auto cwd = QDir::currentPath();
        QDir::setCurrent(dir.path());
        {
            InventoryManager manager;
            auto realC = V::createRecipeService(dir.filePath("meals.txt").toStdString());
            Recipes c(*realC);
            auto realB = V::createInventoryService(manager);
            Stock b(*realB);
            RewardSystem rewards(nullptr, dir.filePath("rewards.ini"));
            auto e = V::createRewardService(rewards);
            Rewards ef(*e);
            auto coordinator =
                V::createCookingCompletionService(b, c, ef, dir.filePath("events.sqlite").toStdString());
            const auto store = dir.filePath("achievements.json");
            auto activity = V::createCookingActivityService(c, b, *coordinator, store.toStdString());
            QList<FoodCandidate> foods;
            for (int id = 1; id <= 5; ++id)
                for (const auto &ingredient : c.findRecipeById(id).value.ingredients) {
                    FoodCandidate f;
                    f.name = QString::fromUtf8(ingredient.foodName.c_str());
                    f.quantity = ingredient.amountGrams * 10;
                    f.unit = "g";
                    f.totalWeightGrams = f.quantity;
                    f.expiryDate = QDate(2026, 10, 30);
                    foods.append(f);
                }
            QString message;
            check(manager.addFoods(foods, &message), "seed real stock");
            recordMeal(c.findRecipeById(1).value, 1, "2026-10-27 12:00:00");
            auto r = activity->refresh("2026-10-28");
            check(ok(r.outcome) && r.value.totalCookingCount == 0 && r.value.legacyMeals.size() == 1 &&
                      r.value.achievements.empty(),
                  "legacy displayed but not counted");
            auto request = [](int id, int recipe, const char *time) {
                V::CookingCompletionRequest r;
                r.eventId = "123e4567-e89b-42d3-a456-4266141740" +
                            (id < 10 ? std::string("0") : std::string()) + std::to_string(id);
                r.recipeId = recipe;
                r.completedAt = time;
                return r;
            };
            const char *times[] = {"2026-10-28 23:59:58", "2026-10-28 23:59:59", "2026-10-29 00:00:00",
                                   "2026-10-30 12:00:00", "2026-10-30 18:00:00"};
            for (int i = 0; i < 5; ++i) {
                auto req = request(i + 1, i + 1, times[i]);
                auto p = coordinator->previewCompletion(req);
                check(ok(p.outcome), "preview real cooking");
                check(ok(coordinator->completeCooking(req, p.confirmationToken).outcome),
                      "finish real cooking");
                r = activity->refresh(std::string(times[i]).substr(0, 10));
                check(ok(r.outcome) && r.value.totalCookingCount == i + 1 &&
                          r.value.expiringCookingCount == i + 1,
                      "one expiring count per meal even with multiple batches");
                check(r.value.achievements.size() == size_t(i == 0 || i == 1 ? 1 : i),
                      "achievement thresholds");
                check(ok(coordinator->retryCompletion(req.eventId).outcome), "repeat cooking event");
                check(activity->refresh("2026-10-30").value.newlyUnlocked.empty(), "no repeat awards");
            }
            r = activity->refresh("2026-10-30");
            check(r.value.achievements.size() == 4 && r.value.currentCookingStreak == 3 &&
                      r.value.bestCookingStreak == 3,
                  "all four achievements and natural days");
            check(r.value.recentMeals.front().recipeId == 5 &&
                      r.value.recentMeals.front().nutrition.calories > 0,
                  "C meal details and nutrition descending");
            activity.reset();
            activity = V::createCookingActivityService(c, b, *coordinator, store.toStdString());
            r = activity->refresh("2026-11-01");
            check(r.value.currentCookingStreak == 0 && r.value.achievements.size() == 4 &&
                      r.value.newlyUnlocked.empty(),
                  "restart and broken streak retain awards");
            const auto saved = readFile(store);
            ef.fail = true;
            auto pending = request(6, 1, "2026-10-30 19:00:00");
            auto p = coordinator->previewCompletion(pending);
            check(coordinator->completeCooking(pending, p.confirmationToken).stage ==
                      V::CompletionStage::MealSaved,
                  "pending reward failure after C saved");
            check(activity->refresh("2026-10-30").value.totalCookingCount == 5,
                  "C record with pending reward excluded");
            ef.fail = false;
            check(ok(coordinator->retryCompletion(pending.eventId).outcome), "pending recovered");
            check(activity->refresh("2026-10-30").value.totalCookingCount == 6, "recovery counted once");
            auto cancel = request(7, 1, "2026-10-30 20:00:00");
            p = coordinator->previewCompletion(cancel);
            check(manager.addFoods(foods, &message), "invalidate preview");
            coordinator->completeCooking(cancel, p.confirmationToken);
            check(ok(coordinator->cancelPreparedCompletion(cancel.eventId).outcome), "cancel prepared");
            check(activity->refresh("2026-10-30").value.totalCookingCount == 6, "cancel excluded");
            c.readFail = true;
            check(activity->refresh("2026-10-30").outcome.errorCode == V::ErrorCode::StorageFailure,
                  "C read error propagated");
            c.readFail = false;
            b.fail = true;
            check(activity->refresh("2026-10-30").outcome.errorCode == V::ErrorCode::StorageFailure,
                  "B read error propagated");
            b.fail = false;
            c.missing = true;
            check(activity->refresh("2026-10-30").outcome.errorCode == V::ErrorCode::InvalidData,
                  "missing corresponding C record fails closed");
            c.missing = false;
            writeFile(store, "not json");
            check(activity->refresh("2026-10-30").outcome.errorCode == V::ErrorCode::InvalidData &&
                      readFile(store) == "not json",
                  "corrupt achievements preserved");
            writeFile(store, saved);
#ifdef Q_OS_WIN
            const auto lockedStore = dir.filePath("locked.json");
            const QByteArray empty = "{\"version\":1,\"unlocked\":[]}";
            writeFile(lockedStore, empty);
            HANDLE handle =
                CreateFileW(reinterpret_cast<LPCWSTR>(lockedStore.utf16()), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            check(handle != INVALID_HANDLE_VALUE, "hold destination against replacement");
            auto blocked = V::createCookingActivityService(c, b, *coordinator, lockedStore.toStdString());
            auto failed = blocked->refresh("2026-10-30");
            CloseHandle(handle);
            check(failed.outcome.errorCode == V::ErrorCode::StorageFailure &&
                      failed.value.achievements.empty() && readFile(lockedStore) == empty,
                  "atomic save failure gives no award and preserves old file");
            check(blocked->refresh("2026-10-30").value.newlyUnlocked.size() == 4,
                  "retry awards exactly once after write failure");
#endif
            StatisticsWidget widget(&rewards, nullptr, activity.get());
            widget.refresh();
            check(widget.findChild<QLabel *>("verifiedCookingCount")->text() == QStringLiteral("6 次"),
                  "statistics uses completed C meals");
            check(widget.findChild<QLabel *>("legacyCookingMeals")->text().contains("2026-10-27"),
                  "legacy visible separately");
            c.readFail = true;
            widget.refresh();
            check(widget.findChild<QLabel *>("verifiedCookingCount")->text() == QStringLiteral("暂不可用") &&
                      !widget.findChild<QLabel *>("growthError")->text().isEmpty(),
                  "UI exposes source errors instead of zero");
        }
        QDir::setCurrent(cwd);
        std::cout << "Cooking activity history, achievements, failures and UI PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
