#include "contracts/ServiceFactories.h"
#include "recipe/RecipeService.h"
#include "recognition/RecognitionService.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QFileInfo>
#include <QLockFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <map>

namespace Shixu::Contracts::V1 {
namespace {
QString q(const std::string &s) { return QString::fromUtf8(s.c_str()); }
bool ok(const OperationResult &r) {
    return r.status == OperationStatus::Succeeded || r.status == OperationStatus::AlreadyApplied;
}
OperationResult success() { return {OperationStatus::Succeeded, ErrorCode::None, {}}; }
OperationResult failure(ErrorCode e, const char *m) { return {OperationStatus::Failed, e, m}; }
OperationResult reconfirm() {
    return {OperationStatus::NeedsConfirmation, ErrorCode::InventoryChanged,
            "库存已变化或确认已过期，请重新预览并确认。"};
}
// Versioned binary journal stores exact floating point values and all frozen
// recipe fields.
struct Codec {
    QDataStream &s;
    bool reading;
    void value(std::string &v) {
        QString t = q(v);
        if (reading) {
            s >> t;
            v = t.toUtf8().toStdString();
        } else
            s << t;
    }
    void value(int &v) {
        qint32 t = v;
        if (reading) {
            s >> t;
            v = t;
        } else
            s << t;
    }
    void value(double &v) {
        if (reading)
            s >> v;
        else
            s << v;
    }
    void value(bool &v) {
        if (reading)
            s >> v;
        else
            s << v;
    }
    void value(BatchId &v) {
        qint64 t = v;
        if (reading) {
            s >> t;
            v = t;
        } else
            s << t;
    }
    template <class E> void enumeration(E &v) {
        int t = static_cast<int>(v);
        value(t);
        if (reading)
            v = static_cast<E>(t);
    }
    template <class T> void list(std::vector<T> &v) {
        int n = int(v.size());
        value(n);
        if (n < 0 || n > 100000) {
            s.setStatus(QDataStream::ReadCorruptData);
            return;
        }
        if (reading)
            v.resize(n);
        for (auto &a : v)
            value(a);
    }
    template <class T> void optional(std::optional<T> &v) {
        bool b = v.has_value();
        value(b);
        if (reading) {
            if (b)
                v.emplace();
            else
                v.reset();
        }
        if (b)
            value(*v);
    }
    void value(NutritionInfo &n) {
        value(n.calories);
        value(n.protein);
        value(n.carbohydrates);
        value(n.fat);
    }
    void value(RecipeIngredient &i) {
        value(i.foodName);
        value(i.amountGrams);
        value(i.nutritionPer100g);
    }
    void value(Recipe &r) {
        value(r.id);
        value(r.name);
        list(r.ingredients);
        list(r.steps);
        value(r.cookingTimeMinutes);
        value(r.category);
        list(r.tags);
    }
    void value(IngredientRequirement &r) {
        value(r.foodName);
        value(r.requiredGrams);
    }
    void value(CookingCompletionRequest &r) {
        value(r.eventId);
        value(r.recipeId);
        value(r.servings);
        value(r.completedAt);
        list(r.externallyPreparedFoods);
    }
    void value(CookingSnapshot &r) {
        value(r.request);
        value(r.recipe);
        list(r.requirements);
        value(r.nutrition);
    }
    void value(OperationResult &r) {
        enumeration(r.status);
        enumeration(r.errorCode);
        value(r.message);
    }
    void value(IngredientAvailability &a) {
        value(a.foodName);
        value(a.requiredGrams);
        value(a.knownAvailableGrams);
        value(a.knownGapGrams);
        value(a.hasUnknownWeight);
        value(a.hasExpiringStock);
        enumeration(a.state);
    }
    void value(BatchConsumption &b) {
        value(b.batchId);
        value(b.foodName);
        value(b.consumedGrams);
        value(b.quantityBefore);
        value(b.quantityAfter);
        value(b.weightBeforeGrams);
        value(b.weightAfterGrams);
        value(b.wasExpiring);
    }
    void value(ConsumptionResult &r) {
        value(r.outcome);
        value(r.eventId);
        list(r.availability);
        list(r.consumed);
        value(r.consumedExpiring);
    }
    void value(MealRecord &r) {
        value(r.recipeId);
        value(r.recipeName);
        value(r.servings);
        value(r.time);
        value(r.date);
        value(r.nutrition);
        value(r.eventId);
    }
    void value(MealWriteResult &r) {
        value(r.outcome);
        value(r.eventId);
        optional(r.savedRecord);
    }
    void value(CookingRewardResult &r) {
        value(r.outcome);
        value(r.eventId);
        value(r.pointsAdded);
        value(r.newCheckIn);
        value(r.totalCookingCount);
        value(r.totalPoints);
        list(r.newlyUnlockedAchievements);
    }
    void value(CookingCompletionResult &r) {
        value(r.outcome);
        value(r.eventId);
        enumeration(r.stage);
        optional(r.inventory);
        optional(r.meal);
        optional(r.reward);
    }
};
struct Entry {
    CookingSnapshot snapshot;
    CookingCompletionResult progress;
    std::string inventoryToken;
};
QByteArray encode(Entry e) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << quint32(1);
    Codec c{stream, false};
    c.value(e.snapshot);
    c.value(e.progress);
    c.value(e.inventoryToken);
    return bytes + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}
bool decode(const QByteArray &bytes, Entry &e) {
    if (bytes.size() < 32)
        return false;
    const auto payload = bytes.left(bytes.size() - 32);
    if (QCryptographicHash::hash(payload, QCryptographicHash::Sha256) != bytes.right(32))
        return false;
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_5_15);
    quint32 version;
    stream >> version;
    if (version != 1)
        return false;
    Codec c{stream, true};
    c.value(e.snapshot);
    c.value(e.progress);
    c.value(e.inventoryToken);
    return stream.status() == QDataStream::Ok && stream.atEnd() &&
           e.progress.eventId == e.snapshot.request.eventId &&
           e.progress.stage >= CompletionStage::Prepared && e.progress.stage <= CompletionStage::Cancelled;
}
bool same(CookingCompletionRequest a, CookingCompletionRequest b) {
    std::sort(a.externallyPreparedFoods.begin(), a.externallyPreparedFoods.end());
    std::sort(b.externallyPreparedFoods.begin(), b.externallyPreparedFoods.end());
    return a.eventId == b.eventId && a.recipeId == b.recipeId && a.servings == b.servings &&
           a.completedAt == b.completedAt && a.externallyPreparedFoods == b.externallyPreparedFoods;
}
ConsumptionRequest consumption(const CookingSnapshot &s) {
    return {s.request.eventId, s.request.completedAt, s.requirements, s.request.externallyPreparedFoods};
}

class Coordinator final : public ICookingCompletionService {
    IInventoryService &inventory;
    IRecipeService &recipes;
    IRewardService &rewards;
    QString connection, path;
    QSqlDatabase db;
    bool ready = false;
    struct Preview {
        Entry entry;
        std::string token;
    };
    std::map<EventId, Preview> previews;
    OperationResult load(const EventId &id, Entry &e) const {
        if (!ready)
            return failure(ErrorCode::StorageFailure, "无法打开做饭进度记录。");
        QSqlQuery query(db);
        query.prepare("SELECT payload FROM cooking_completions WHERE event_id=?");
        query.addBindValue(q(id));
        if (!query.exec())
            return failure(ErrorCode::StorageFailure, "无法读取做饭进度。");
        if (!query.next())
            return failure(ErrorCode::NotFound, "未找到做饭记录。");
        if (!decode(query.value(0).toByteArray(), e) || e.progress.eventId != id)
            return failure(ErrorCode::InvalidData, "做饭进度记录已损坏。");
        return success();
    }
    bool save(const Entry &e) {
        if (!ready)
            return false;
        QSqlQuery query(db);
        query.prepare("INSERT OR REPLACE INTO "
                      "cooking_completions(event_id,payload) VALUES(?,?)");
        query.addBindValue(q(e.progress.eventId));
        query.addBindValue(encode(e));
        return query.exec();
    }
    CookingCompletionResult result(const EventId &id, OperationResult error) const {
        CookingCompletionResult r;
        r.eventId = id;
        r.outcome = error;
        return r;
    }
    CookingCompletionResult stop(Entry &e, OperationResult error) {
        e.progress.outcome = error;
        if (!save(e))
            e.progress.outcome = failure(ErrorCode::StorageFailure, "无法保存做饭进度，请稍后重试。");
        return e.progress;
    }
    bool advance(Entry &e, CompletionStage stage) {
        auto old = e.progress.stage;
        e.progress.stage = stage;
        e.progress.outcome = success();
        if (save(e))
            return true;
        e.progress.stage = old;
        e.progress.outcome = failure(ErrorCode::StorageFailure, "业务已处理，但进度保存失败；请重试恢复。");
        return false;
    }
    CookingCompletionResult run(Entry &e) {
        if (e.progress.stage == CompletionStage::Cancelled) {
            auto r = e.progress;
            r.outcome = failure(ErrorCode::InvalidState, "这条记录已取消，请为新的做饭重新确认。");
            return r;
        }
        if (e.progress.stage == CompletionStage::Completed) {
            auto r = e.progress;
            r.outcome = success();
            r.outcome.status = OperationStatus::AlreadyApplied;
            return r;
        }
        // Always consult B, even when its commit preceded a crash in this journal.
        const auto existing = inventory.getConsumption(e.progress.eventId);
        if (!ok(existing.outcome) && existing.outcome.errorCode != ErrorCode::NotFound)
            return stop(e, existing.outcome);
        if (e.progress.stage >= CompletionStage::InventorySaved && !ok(existing.outcome))
            return stop(e, failure(ErrorCode::InvalidData, "库存事件与做饭进度不一致，请保留记录。"));
        if (e.progress.stage == CompletionStage::Prepared) {
            if (!ok(existing.outcome) && e.inventoryToken.empty())
                return stop(e, reconfirm());
            const auto used = inventory.consumeForCooking(consumption(e.snapshot), e.inventoryToken);
            if (!ok(used.outcome))
                return stop(e, used.outcome);
            e.progress.inventory = used;
            if (!advance(e, CompletionStage::InventorySaved))
                return e.progress;
        }
        if (e.progress.stage == CompletionStage::InventorySaved) {
            MealRecord meal;
            meal.recipeId = e.snapshot.recipe.id;
            meal.recipeName = e.snapshot.recipe.name;
            meal.servings = e.snapshot.request.servings;
            meal.time = e.snapshot.request.completedAt;
            meal.date = meal.time.substr(0, 10);
            meal.nutrition = e.snapshot.nutrition;
            auto written = recipes.recordMealOnce({e.progress.eventId, meal});
            if (!ok(written.outcome))
                return stop(e, written.outcome);
            e.progress.meal = written;
            if (!advance(e, CompletionStage::MealSaved))
                return e.progress;
        }
        if (e.progress.stage == CompletionStage::MealSaved) {
            if (!e.progress.inventory)
                return stop(e, failure(ErrorCode::InvalidData, "缺少已扣减库存记录。"));
            auto awarded = rewards.applyCookingRewardOnce(
                {e.progress.eventId, e.snapshot.request.completedAt, e.progress.inventory->consumedExpiring});
            if (!ok(awarded.outcome))
                return stop(e, awarded.outcome);
            e.progress.reward = awarded;
            if (!advance(e, CompletionStage::RewardSaved))
                return e.progress;
        }
        advance(e, CompletionStage::Completed);
        return e.progress;
    }

  public:
    Coordinator(IInventoryService &b, IRecipeService &c, IRewardService &e, const std::string &file)
        : inventory(b), recipes(c), rewards(e), connection("cooking-" + QUuid::createUuid().toString()),
          path(QFileInfo(q(file)).absoluteFilePath()) {
        db = QSqlDatabase::addDatabase("QSQLITE", connection);
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            ready = query.exec("CREATE TABLE IF NOT EXISTS cooking_completions(event_id "
                               "TEXT PRIMARY KEY,payload BLOB NOT NULL)");
        }
    }
    ~Coordinator() override {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection);
    }
    CookingCompletionPreview previewCompletion(const CookingCompletionRequest &request) override {
        CookingCompletionPreview out;
        out.snapshot.request = request;
        QLockFile lock(path + ".lock");
        if (!lock.tryLock(0)) {
            out.outcome = failure(ErrorCode::StorageFailure, "做饭记录正被使用，请稍后重试。");
            return out;
        }
        const QString id = q(request.eventId), time = q(request.completedAt);
        if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id || request.servings != 1 ||
            request.recipeId <= 0 || !QDateTime::fromString(time, "yyyy-MM-dd HH:mm:ss").isValid() ||
            QDateTime::fromString(time, "yyyy-MM-dd HH:mm:ss").toString("yyyy-MM-dd HH:mm:ss") != time) {
            out.outcome = failure(ErrorCode::InvalidArgument, "做饭请求的菜谱、时间或事件编号无效。");
            return out;
        }
        Entry e;
        auto found = load(request.eventId, e);
        if (ok(found)) {
            if (!same(request, e.snapshot.request)) {
                out.outcome =
                    failure(ErrorCode::EventConflict, "待处理记录不能修改，请先安全取消后重新确认。");
                return out;
            }
            if (e.progress.stage != CompletionStage::Prepared) {
                out.outcome = failure(ErrorCode::InvalidState, "该记录已扣减或已取消，请使用恢复操作。");
                return out;
            }
            auto committed = inventory.getConsumption(request.eventId);
            if (ok(committed.outcome)) {
                out.outcome = failure(ErrorCode::InvalidState, "库存已经扣减，请继续完成未结算步骤。");
                return out;
            }
            if (committed.outcome.errorCode != ErrorCode::NotFound) {
                out.outcome = committed.outcome;
                return out;
            }
        } else {
            if (found.errorCode != ErrorCode::NotFound) {
                out.outcome = found;
                return out;
            }
            auto recipe = recipes.findRecipeById(request.recipeId);
            if (!ok(recipe.outcome)) {
                out.outcome = recipe.outcome;
                return out;
            }
            e.snapshot.request = request;
            e.snapshot.recipe = recipe.value;
            e.snapshot.nutrition = calculateNutrition(recipe.value, 1);
            if (recipe.value.id != request.recipeId || recipe.value.ingredients.empty()) {
                out.outcome = failure(ErrorCode::InvalidData, "菜谱信息不完整。");
                return out;
            }
            const auto &nutrition = e.snapshot.nutrition;
            for (double value :
                 {nutrition.calories, nutrition.protein, nutrition.carbohydrates, nutrition.fat}) {
                if (!std::isfinite(value) || value < 0) {
                    out.outcome = failure(ErrorCode::InvalidData, "菜谱营养数据无效。");
                    return out;
                }
            }
            RecognitionService normalizer;
            std::map<std::string, double> requirements;
            for (const auto &ingredient : recipe.value.ingredients) {
                const auto name = normalizer.normalizeFoodName(q(ingredient.foodName)).toUtf8().toStdString();
                if (name.empty() || !std::isfinite(ingredient.amountGrams) || ingredient.amountGrams <= 0) {
                    out.outcome = failure(ErrorCode::InvalidData, "菜谱食材用量无效。");
                    return out;
                }
                requirements[name] += ingredient.amountGrams;
            }
            for (const auto &item : requirements)
                e.snapshot.requirements.push_back({item.first, item.second});
        }
        out.snapshot = e.snapshot;
        out.inventory = inventory.previewConsumption(consumption(e.snapshot));
        out.outcome = out.inventory.outcome;
        previews.erase(request.eventId);
        if (ok(out.outcome) && !out.inventory.previewToken.empty()) {
            e.inventoryToken = out.inventory.previewToken;
            const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            previews[request.eventId] = {e, token};
            out.confirmationToken = token;
        }
        return out;
    }
    CookingCompletionResult completeCooking(const CookingCompletionRequest &request,
                                            const std::string &token) override {
        QLockFile lock(path + ".lock");
        if (!lock.tryLock(0))
            return result(request.eventId,
                          failure(ErrorCode::StorageFailure, "做饭记录正被使用，请稍后重试。"));
        Entry e;
        auto found = load(request.eventId, e);
        if (ok(found)) {
            if (!same(request, e.snapshot.request))
                return result(request.eventId,
                              failure(ErrorCode::EventConflict, "事件编号对应的请求不一致。"));
            if (e.progress.stage != CompletionStage::Prepared)
                return run(e);
        } else if (found.errorCode != ErrorCode::NotFound)
            return result(request.eventId, found);
        auto p = previews.find(request.eventId);
        if (p == previews.end() || p->second.token != token ||
            !same(request, p->second.entry.snapshot.request)) {
            if (ok(found))
                return stop(e, reconfirm());
            return result(request.eventId, reconfirm());
        }
        e = p->second.entry;
        e.progress.eventId = request.eventId;
        e.progress.stage = CompletionStage::Prepared;
        e.progress.outcome = success();
        if (!save(e))
            return result(request.eventId,
                          failure(ErrorCode::StorageFailure, "无法保存待处理记录，尚未扣减库存。"));
        previews.erase(p);
        return run(e);
    }
    CookingCompletionResult retryCompletion(const EventId &id) override {
        QLockFile lock(path + ".lock");
        if (!lock.tryLock(0))
            return result(id, failure(ErrorCode::StorageFailure, "做饭记录正被使用。"));
        Entry e;
        auto f = load(id, e);
        return ok(f) ? run(e) : result(id, f);
    }
    QueryResult<CookingCompletionDetails> getCompletion(const EventId &id) const override {
        Entry e;
        auto f = load(id, e);
        return {f, {e.snapshot, e.progress}};
    }
    CookingCompletionResult cancelPreparedCompletion(const EventId &id) override {
        QLockFile lock(path + ".lock");
        if (!lock.tryLock(0))
            return result(id, failure(ErrorCode::StorageFailure, "做饭记录正被使用。"));
        Entry e;
        auto f = load(id, e);
        if (!ok(f))
            return result(id, f);
        if (e.progress.stage == CompletionStage::Cancelled) {
            auto r = e.progress;
            r.outcome.status = OperationStatus::AlreadyApplied;
            return r;
        }
        if (e.progress.stage != CompletionStage::Prepared) {
            auto r = e.progress;
            r.outcome = failure(ErrorCode::InvalidState, "已扣减库存的记录不能取消。");
            return r;
        }
        auto used = inventory.getConsumption(id);
        if (ok(used.outcome))
            return stop(e, failure(ErrorCode::InvalidState, "库存已经扣减，请继续完成。"));
        if (used.outcome.errorCode != ErrorCode::NotFound)
            return stop(e, used.outcome);
        advance(e, CompletionStage::Cancelled);
        previews.erase(id);
        return e.progress;
    }
    QueryResult<std::vector<CookingCompletionResult>> getPendingCompletions() const override {
        QueryResult<std::vector<CookingCompletionResult>> out;
        if (!ready) {
            out.outcome = failure(ErrorCode::StorageFailure, "无法打开做饭进度记录。");
            return out;
        }
        QSqlQuery query(db);
        if (!query.exec("SELECT payload FROM cooking_completions ORDER BY event_id")) {
            out.outcome = failure(ErrorCode::StorageFailure, "无法读取待处理记录。");
            return out;
        }
        while (query.next()) {
            Entry e;
            if (!decode(query.value(0).toByteArray(), e)) {
                out.value.clear();
                out.outcome = failure(ErrorCode::InvalidData, "做饭记录已损坏。");
                return out;
            }
            if (e.progress.stage != CompletionStage::Completed &&
                e.progress.stage != CompletionStage::Cancelled)
                out.value.push_back(e.progress);
        }
        out.outcome = success();
        return out;
    }
};
} // namespace
std::unique_ptr<ICookingCompletionService> createCookingCompletionService(IInventoryService &b,
                                                                          IRecipeService &c,
                                                                          IRewardService &e,
                                                                          const std::string &path) {
    return std::make_unique<Coordinator>(b, c, e, path);
}
} // namespace Shixu::Contracts::V1
