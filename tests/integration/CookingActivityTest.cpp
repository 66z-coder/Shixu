#include "contracts/ActivityContracts.h"
#include "contracts/ServiceFactories.h"
#include "core/AppClock.h"
#include "inventory/inventory.h"
#include "recipe/RecipeService.h"
#include "reward/RewardSystem.h"
#include "ui/StatisticsWidget.h"
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QLockFile>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <cmath>
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
struct GoalActivity final : V::ICookingActivityService {
    V::CookingActivitySnapshot snapshot;
    std::string requestedDate;
    V::QueryResult<V::CookingActivitySnapshot> refresh(const V::LocalDate &date) override {
        requestedDate = date;
        return {{V::OperationStatus::Succeeded, V::ErrorCode::None, {}}, snapshot};
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
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == "offscreen") {
        int font = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        if (font >= 0)
            app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
    }
#endif

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
            {
                StatisticsWidget empty(&rewards, nullptr, activity.get());
                check(empty.findChild<QLabel *>("recentMealsHeading")->text() == QStringLiteral("最近做饭 · 显示 0 条"),
                      "empty history displays zero records");
            }
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
            check(widget.findChild<QLabel *>("achievementCount")->text() == QStringLiteral("已获得 4 / 4 枚成就") &&
                      !widget.findChild<QLabel *>("checkInBadges"),
                  "achievement count matches four visible cooking badges");
            check(widget.findChild<QLabel *>("recentMealsHeading")->text() == QStringLiteral("最近做饭 · 显示 6 条"),
                  "heading uses actual displayed count");
            const auto today = activity->refresh(Shixu::appNow().date().toString(Qt::ISODate).toStdString());
            check(widget.findChild<QLabel *>("todayMealCount")->text() == QStringLiteral("%1 次").arg(today.value.todayMealCount) &&
                  widget.findChild<QLabel *>("todayCalories")->text() == QStringLiteral("%1 千卡").arg(today.value.todayNutrition.calories, 0, 'f', 0),
                  "overview displays real current-day totals");
            check(widget.findChild<QProgressBar *>("goalBar1")->value() == 0 &&
                  widget.findChild<QLabel *>("goalValue1")->text().contains(QStringLiteral("未设置目标")),
                  "unset nutrition goal displays no progress");
            check(!widget.findChild<QLabel *>("legacyCookingMeals"), "legacy records have no UI entry");
            check(activity->refresh("2026-10-30").value.legacyMeals.size() == 1, "legacy data preserved");
            const auto legacyRecipe = c.findRecipeById(1).value;
            for (int i = 0; i < 11; ++i)
                recordMeal(legacyRecipe, 1, "2026-10-27 13:00:00");
            const auto summary = activity->refresh("2026-10-27");
            const auto expected = calculateNutrition(legacyRecipe, 1);
            check(ok(summary.outcome) && summary.value.todayMealCount == 12 &&
                      std::abs(summary.value.todayNutrition.calories - expected.calories * 12) < 0.01 &&
                      std::abs(summary.value.todayNutrition.protein - expected.protein * 12) < 0.01,
                  "today nutrition counts all records, including beyond recent ten");
            const auto completedDay = activity->refresh("2026-10-30");
            check(completedDay.value.todayMealCount == 3, "completed events counted on their own day");
            auto *checkIn = widget.findChild<QPushButton *>("growthCheckIn");
            check(checkIn && !checkIn->isEnabled(), "cooking auto checkin disables same-day button");
            auto before = rewards.getGrowthSnapshot("2026-10-29").value.totalPoints;
            checkIn->click();
            check(rewards.getGrowthSnapshot("2026-10-29").value.totalPoints == before,
                  "disabled repeat adds no points");
            const auto output = qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
            if (!output.isEmpty()) {
                QDir().mkpath(output);
                widget.resize(1000, 800);
                widget.show();
                QApplication::processEvents();
                check(widget.grab().save(output + "/growth-unlocked.png"), "capture unlocked growth");
                auto *scroll = widget.findChild<QScrollArea *>();
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                QApplication::processEvents();
                check(widget.grab().save(output + "/growth-nutrition.png"), "capture nutrition and recent meals");
                scroll->ensureWidgetVisible(widget.findChild<QLabel *>("todayMealCount"), 0, 120);
                QApplication::processEvents();
                check(widget.grab().save(output + "/growth-overview.png"), "capture nutrition overview");
            }
            {
                RewardSystem manual(nullptr, dir.filePath("manual-reward.ini"));
                StatisticsWidget daily(&manual, nullptr, activity.get());
                auto *button = daily.findChild<QPushButton *>("growthCheckIn");
                check(button->isEnabled(), "new day allows manual checkin");
                button->click();
                check(!button->isEnabled(), "manual checkin updates UI");
                auto points =
                    manual.getGrowthSnapshot(Shixu::appNow().date().toString(Qt::ISODate).toStdString())
                        .value.totalPoints;
                button->click();
                check(manual.getGrowthSnapshot(Shixu::appNow().date().toString(Qt::ISODate).toStdString())
                              .value.totalPoints == points,
                      "manual double click no duplicate");
            }
            c.readFail = true;
            widget.refresh();
            check(widget.findChild<QLabel *>("verifiedCookingCount")->text() == QStringLiteral("暂不可用") &&
                      widget.findChild<QLabel *>("recentMealsHeading")->text() == QStringLiteral("最近做饭") &&
                      widget.findChild<QLabel *>("todayCalories")->text() == QStringLiteral("暂不可用") &&
                      widget.findChild<QProgressBar *>("goalBar1")->value() == 0 &&
                      !widget.findChild<QLabel *>("growthError")->text().isEmpty(),
                  "UI exposes source errors instead of zero");
        }
        {
            const QString goalsDir = dir.filePath("goals");
            check(QDir().mkpath(goalsDir), "create goals test directory");
            RewardSystem rewards(nullptr, goalsDir + "/rewards.ini");
            GoalActivity activity;
            activity.snapshot.todayMealCount = 2;
            activity.snapshot.todayNutrition = {240, 60, 10, 5};
            StatisticsWidget widget(&rewards, nullptr, &activity);
            auto *open = widget.findChild<QPushButton *>("editNutritionGoals");
            auto *calories = widget.findChild<QLabel *>("goalValue0");
            auto *protein = widget.findChild<QLabel *>("goalValue1");
            auto *bar = widget.findChild<QProgressBar *>("goalBar0");
            check(open && calories->text().contains(QStringLiteral("未设置目标")) && bar->value() == 0,
                  "new user starts with no goals");
            auto edit = [&](const QStringList &values, bool save) {
                QTimer::singleShot(0, [&] {
                    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                    check(dialog && dialog->objectName() == "nutritionGoalsDialog", "goals dialog opened");
                    for (int i = 0; i < 4; ++i)
                        dialog->findChild<QLineEdit *>(QStringLiteral("nutritionGoalInput%1").arg(i))->setText(values[i]);
                    auto *buttons = dialog->findChild<QDialogButtonBox *>();
                    buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
                });
                open->click();
            };
            edit({"200", "", "", ""}, true);
            check(calories->text().contains(QStringLiteral("240 / 200 千卡 · 120%")) && bar->value() == 1000 &&
                      protein->text().contains(QStringLiteral("未设置目标")), "partial goals and over 100 percent");
            edit({"999", "99", "99", "99"}, false);
            check(calories->text().contains(QStringLiteral("240 / 200")), "cancel does not alter saved goals");
            edit({"480", "120", "20", "10"}, true);
            check(calories->text().contains(QStringLiteral("50%")) && bar->value() == 500 &&
                      protein->text().contains(QStringLiteral("60.0 / 120 克 · 50%")), "all four targets update");
            const auto goalCapture = qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
            if (!goalCapture.isEmpty()) {
                QDir().mkpath(goalCapture);
                widget.resize(980, 620);
                widget.show();
                QApplication::processEvents();
                widget.findChild<QScrollArea *>()->ensureWidgetVisible(widget.findChild<QPushButton *>("editNutritionGoals"), 0, 15);
                QApplication::processEvents();
                check(widget.grab().save(goalCapture + "/nutrition-goals.png"), "capture goal progress");
            }
            bool validationShown = false;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) return;
                dialog->findChild<QLineEdit *>("nutritionGoalInput0")->setText("0");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
                validationShown = dialog->isVisible() && !dialog->findChild<QLabel *>("nutritionGoalsError")->text().isEmpty();
                dialog->reject();
            });
            open->click();
            check(validationShown && calories->text().contains(QStringLiteral("240 / 480")),
                  "invalid UI input stays open and leaves goal unchanged");
            QLockFile held(goalsDir + "/nutrition-goals.json.lock");
            check(held.tryLock(0), "lock goal settings for failure test");
            bool failureShown = false;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) return;
                dialog->findChild<QLineEdit *>("nutritionGoalInput0")->setText("900");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
                failureShown = dialog->isVisible() && !dialog->findChild<QLabel *>("nutritionGoalsError")->text().isEmpty();
                dialog->reject();
            });
            open->click();
            held.unlock();
            check(failureShown && calories->text().contains(QStringLiteral("240 / 480")),
                  "failed UI save keeps last saved target");
            StatisticsWidget restored(&rewards, nullptr, &activity);
            check(restored.findChild<QLabel *>("goalValue0")->text().contains(QStringLiteral("240 / 480")),
                  "saved goals survive widget restart");
            activity.snapshot.todayNutrition = {};
            activity.snapshot.todayMealCount = 0;
            restored.refresh();
            check(restored.findChild<QProgressBar *>("goalBar0")->value() == 0 &&
                  restored.findChild<QLabel *>("goalValue0")->text().contains(QStringLiteral("0%")) &&
                  activity.requestedDate == Shixu::appNow().date().toString(Qt::ISODate).toStdString(),
                  "empty day reports zero progress");
            edit({"", "", "", ""}, true);
            check(calories->text().contains(QStringLiteral("未设置目标")) && bar->value() == 0,
                  "clear goals returns to unset");
            NutritionGoalsStore store(goalsDir + "/nutrition-goals.json");
            NutritionGoals invalid{};
            invalid[0] = -1;
            QString error;
            const auto before = readFile(goalsDir + "/nutrition-goals.json");
            check(!store.save(invalid, error) && !error.isEmpty() &&
                      readFile(goalsDir + "/nutrition-goals.json") == before, "invalid values leave saved file intact");
            NutritionGoalsStore blocked(goalsDir + "/missing/nutrition-goals.json");
            NutritionGoals valid{}; valid[0] = 200;
            check(!blocked.save(valid, error) && !error.isEmpty(), "write failure reported");
            writeFile(goalsDir + "/nutrition-goals.json", "broken");
            NutritionGoals original{}; original[0] = 123;
            check(!store.load(original, error) && original[0] == 123 && !store.save(valid, error) &&
                      readFile(goalsDir + "/nutrition-goals.json") == "broken", "bad file preserved");
        }
        QDir::setCurrent(cwd);
        std::cout << "Cooking activity history, achievements, failures and UI PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
