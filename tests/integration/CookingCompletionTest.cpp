#include "contracts/RecommendationContract.h"
#include "contracts/ServiceFactories.h"
#include "inventory/inventory.h"
#include "recognition/RecognitionService.h"
#include "reward/RewardSystem.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
namespace V = Shixu::Contracts::V1;
void check(bool b, const char *message) {
    if (!b)
        throw std::runtime_error(message);
}
bool ok(const V::OperationResult &r) {
    return r.status == V::OperationStatus::Succeeded || r.status == V::OperationStatus::AlreadyApplied;
}
struct RecipeFault : V::IRecipeService {
    V::QueryResult<std::vector<MealRecord>> getAllMeals() const override { return real.getAllMeals(); }
    V::IRecipeService &real;
    bool fail = false;
    explicit RecipeFault(V::IRecipeService &r) : real(r) {}
    V::QueryResult<std::vector<Recipe>> getAllRecipes() const override { return real.getAllRecipes(); }
    V::QueryResult<Recipe> findRecipeById(int id) const override { return real.findRecipeById(id); }
    V::QueryResult<std::vector<MealRecord>> getMealsByDate(const std::string &d) const override {
        return real.getMealsByDate(d);
    }
    V::QueryResult<NutritionInfo> getNutritionByDate(const std::string &d) const override {
        return real.getNutritionByDate(d);
    }
    V::MealWriteResult recordMealOnce(const V::MealWriteRequest &r) override {
        if (!fail)
            return real.recordMealOnce(r);
        V::MealWriteResult e;
        e.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "injected meal failure"};
        return e;
    }
};
struct RewardFault : V::IRewardService {
    V::IRewardService &real;
    bool fail = false;
    explicit RewardFault(V::IRewardService &r) : real(r) {}
    V::CookingRewardResult applyCookingRewardOnce(const V::CookingRewardRequest &r) override {
        if (!fail)
            return real.applyCookingRewardOnce(r);
        V::CookingRewardResult e;
        e.outcome = {V::OperationStatus::Failed, V::ErrorCode::StorageFailure, "injected reward failure"};
        return e;
    }
    V::QueryResult<V::GrowthSnapshot> getGrowthSnapshot(const std::string &d) const override {
        return real.getGrowthSnapshot(d);
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir(QDir::currentPath() + "/completion-XXXXXX");
        check(dir.isValid(), "temporary directory");
        const auto cwd = QDir::currentPath();
        QDir::setCurrent(dir.path());
        {
            InventoryManager manager;
            RewardSystem reward(nullptr, dir.filePath("rewards.ini"));
            auto b = V::createInventoryService(manager);
            auto c = V::createRecipeService(dir.filePath("meals.txt").toStdString());
            auto e = V::createRewardService(reward);
            RecipeFault cf(*c);
            RewardFault ef(*e);
            const auto journal = dir.filePath("events.sqlite").toStdString();
            auto service = V::createCookingCompletionService(*b, cf, ef, journal);
            auto recipe = c->findRecipeById(1);
            check(ok(recipe.outcome), "real recipe");
            auto add = [&] {
                QList<FoodCandidate> foods;
                for (const auto &i : recipe.value.ingredients) {
                    FoodCandidate f;
                    f.name = QString::fromUtf8(i.foodName.c_str());
                    f.quantity = i.amountGrams * 10;
                    f.unit = "g";
                    f.totalWeightGrams = f.quantity;
                    f.expiryDate = QDate(2026, 10, 30);
                    foods.append(f);
                }
                QString error;
                check(manager.addFoods(foods, &error), "stock real recipe");
            };
            add();
            auto request = [](int i) {
                V::CookingCompletionRequest r;
                r.eventId = "123e4567-e89b-42d3-a456-4266141740" +
                            (i < 10 ? std::string("0") : std::string()) + std::to_string(i);
                r.recipeId = 1;
                r.completedAt = "2026-10-28 12:00:00";
                return r;
            };
            auto d = V::createRecommendationService(*c, *b);
            for (int mood = 1; mood <= 5; ++mood) {
                Shixu::RecommendationEngine::UserContext context;
                context.preferences = V::RecommendationPreferences{};
                context.preferences->mood = static_cast<V::Mood>(mood);
                context.preferences->maxPreparationMinutes = 75;
                context.preferences->referenceDate = "2026-10-28";
                context.inventorySnapshot = b->getInventoryBatches().value;
                check(ok(d->recommend(context).outcome), "five moods with real B/C");
            }
            auto r = request(1);
            auto p = service->previewCompletion(r);
            check(ok(p.outcome), "initial preview");
            auto done = service->completeCooking(r, p.confirmationToken);
            check(done.stage == V::CompletionStage::Completed && ok(done.outcome), "complete real B C E");
            check(done.reward && done.reward->pointsAdded == 30, "actual expiring consumption award");
            check(service->completeCooking(r, p.confirmationToken).outcome.status ==
                      V::OperationStatus::AlreadyApplied,
                  "double submit");
            service.reset();
            service = V::createCookingCompletionService(*b, cf, ef, journal);
            check(service->retryCompletion(r.eventId).outcome.status == V::OperationStatus::AlreadyApplied,
                  "restart replay");
            check(c->getMealsByDate("2026-10-28").value.size() == 1 && reward.totalCookingCount() == 1,
                  "no duplicate meal/reward");
            cf.fail = true;
            r = request(2);
            p = service->previewCompletion(r);
            auto partial = service->completeCooking(r, p.confirmationToken);
            check(partial.stage == V::CompletionStage::InventorySaved && !ok(partial.outcome),
                  "C failure preserves B stage");
            auto stock = b->getInventoryBatches().value.revision;
            check(service->cancelPreparedCompletion(r.eventId).outcome.errorCode ==
                      V::ErrorCode::InvalidState,
                  "cannot cancel after B");
            service.reset();
            cf.fail = false;
            service = V::createCookingCompletionService(*b, cf, ef, journal);
            check(service->getCompletion(r.eventId).value.snapshot.recipe.name == recipe.value.name,
                  "frozen details survive restart");
            check(ok(service->retryCompletion(r.eventId).outcome), "resume C failure");
            check(b->getInventoryBatches().value.revision == stock, "resume must not deduct twice");
            ef.fail = true;
            r = request(3);
            p = service->previewCompletion(r);
            partial = service->completeCooking(r, p.confirmationToken);
            check(partial.stage == V::CompletionStage::MealSaved, "E failure preserves C stage");
            ef.fail = false;
            check(ok(service->retryCompletion(r.eventId).outcome), "resume E failure");
            check(c->getMealsByDate("2026-10-28").value.size() == 3 && reward.totalCookingCount() == 3,
                  "E retry no duplicate meal");
            r = request(4);
            p = service->previewCompletion(r);
            add();
            partial = service->completeCooking(r, p.confirmationToken);
            check(partial.outcome.status == V::OperationStatus::NeedsConfirmation &&
                      partial.stage == V::CompletionStage::Prepared,
                  "stale preview no deduction");
            auto changed = r;
            changed.externallyPreparedFoods = {"番茄"};
            check(service->previewCompletion(changed).outcome.errorCode == V::ErrorCode::EventConflict,
                  "frozen request cannot change");
            check(ok(service->cancelPreparedCompletion(r.eventId).outcome), "safe cancellation");
            check(service->retryCompletion(r.eventId).outcome.errorCode == V::ErrorCode::InvalidState,
                  "cancel tombstone");
            r = request(5);
            p = service->previewCompletion(r);
            add();
            service->completeCooking(r, p.confirmationToken);
            service.reset();
            b = V::createInventoryService(manager);
            service = V::createCookingCompletionService(*b, cf, ef, journal);
            check(service->retryCompletion(r.eventId).outcome.status == V::OperationStatus::NeedsConfirmation,
                  "restart needs explicit confirmation");
            p = service->previewCompletion(r);
            check(ok(p.outcome) && ok(service->completeCooking(r, p.confirmationToken).outcome),
                  "repreview and explicitly confirm");
            // Inject a failure on a later SQL update: B must roll back every earlier
            // batch and event.
            QSqlQuery sql(QSqlDatabase::database("ShixuInventoryConnection"));
            check(sql.exec("CREATE TRIGGER fail_second BEFORE UPDATE ON foods WHEN "
                           "OLD.name='鸡蛋' BEGIN SELECT RAISE(ABORT,'injected'); END"),
                  "install transaction fault");
            r = request(6);
            p = service->previewCompletion(r);
            stock = b->getInventoryBatches().value.revision;
            const auto beforeRollback = b->getInventoryBatches().value.batches;
            partial = service->completeCooking(r, p.confirmationToken);
            check(!ok(partial.outcome) && partial.stage == V::CompletionStage::Prepared,
                  "B failure stops C E");
            check(b->getInventoryBatches().value.revision == stock &&
                      b->getConsumption(r.eventId).outcome.errorCode == V::ErrorCode::NotFound,
                  "B rollback includes revision and event");
            const auto afterRollback = b->getInventoryBatches().value.batches;
            check(beforeRollback.size() == afterRollback.size(), "rollback batch count");
            for (size_t i = 0; i < beforeRollback.size(); ++i)
                check(beforeRollback[i].quantity == afterRollback[i].quantity &&
                          beforeRollback[i].totalWeightGrams == afterRollback[i].totalWeightGrams,
                      "rollback restores each batch quantity and weight");
            check(sql.exec("DROP TRIGGER fail_second"), "remove transaction fault");
            check(ok(service->retryCompletion(r.eventId).outcome), "B failure retry");
            check(service->getPendingCompletions().value.empty(), "all pending cleared");
            check(c->getMealsByDate("2026-10-28").value.size() == 5 && reward.totalCookingCount() == 5,
                  "final counts agree");
            // B commits but the next journal write fails: restart must replay, not deduct.
            {
                auto journalDb = QSqlDatabase::addDatabase("QSQLITE", "journal-fault");
                journalDb.setDatabaseName(QString::fromStdString(journal));
                check(journalDb.open(), "open journal fault connection");
                QSqlQuery fault(journalDb);
                check(fault.exec("CREATE TRIGGER fail_progress BEFORE INSERT ON cooking_completions WHEN "
                                 "EXISTS (SELECT 1 FROM cooking_completions WHERE event_id=NEW.event_id) "
                                 "BEGIN SELECT RAISE(ABORT,'journal fault'); END"),
                      "journal fault trigger");
                r = request(7);
                p = service->previewCompletion(r);
                partial = service->completeCooking(r, p.confirmationToken);
                check(partial.stage == V::CompletionStage::Prepared && !ok(partial.outcome) &&
                          ok(b->getConsumption(r.eventId).outcome),
                      "B committed before journal failure");
                stock = b->getInventoryBatches().value.revision;
                check(fault.exec("DROP TRIGGER fail_progress"), "remove journal fault");
                service.reset();
                service = V::createCookingCompletionService(*b, cf, ef, journal);
                check(ok(service->retryCompletion(r.eventId).outcome), "recover B commit with stale journal");
                check(stock == b->getInventoryBatches().value.revision && reward.totalCookingCount() == 6,
                      "journal recovery no duplicate B E");
            }
            QSqlDatabase::removeDatabase("journal-fault");
            // Legacy alias migration uses A, preserving batches and invalidating stale tokens.
            check(sql.exec("UPDATE foods SET name='西红柿' WHERE name='番茄'"), "legacy alias fixture");
            stock = b->getInventoryBatches().value.revision;
            RecognitionService normalizer;
            QString error;
            check(manager.normalizeNames(
                      [&](const QString &name) { return normalizer.normalizeFoodName(name); }, &error),
                  "atomic alias migration");
            auto normalized = b->getInventoryBatches();
            check(normalized.value.revision != stock, "migration advances revision");
            for (const auto &batch : normalized.value.batches)
                check(batch.name != "西红柿", "canonical name migrated");
            auto blocked = V::createCookingCompletionService(
                *b, cf, ef, dir.filePath("missing-parent/events.sqlite").toStdString());
            check(blocked->previewCompletion(request(8)).outcome.errorCode == V::ErrorCode::StorageFailure,
                  "journal unavailable blocks before inventory mutation");
            {
                auto corruptDb = QSqlDatabase::addDatabase("QSQLITE", "journal-corrupt");
                corruptDb.setDatabaseName(QString::fromStdString(journal));
                check(corruptDb.open(), "open corruption fixture");
                QSqlQuery corrupt(corruptDb);
                check(corrupt.exec("UPDATE cooking_completions SET payload=x'0001'"),
                      "corrupt journal fixture");
                check(service->getCompletion(request(1).eventId).outcome.errorCode ==
                          V::ErrorCode::InvalidData,
                      "corrupt journal cannot be treated as a new event");
                check(service->getPendingCompletions().outcome.errorCode == V::ErrorCode::InvalidData,
                      "corrupt journal must not look like an empty pending list");
            }
            QSqlDatabase::removeDatabase("journal-corrupt");
        }
        QDir::setCurrent(cwd);
        std::cout << "Real B/C/D/E integration, faults, replay, cancellation and "
                     "restart PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
