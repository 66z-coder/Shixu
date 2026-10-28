
#include "inventory.h"


#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QMap>
#include <QStringList>



#include <QSet>
#include <vector>

#include "contracts/CoreContracts.h"
#include "contracts/ServiceFactories.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDate>
#include <QString>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include <QDateTime>

#include <cmath>
#include <memory>


#include <algorithm>


namespace Shixu::Contracts::V1 {


    static std::string makePreviewToken(
    const ConsumptionRequest& request,
    const QString& revision);

class InventoryContractAdapter final : public IInventoryService {
public:
    explicit InventoryContractAdapter(InventoryManager& inventory)
        : inventory_(inventory) {}

    QueryResult<InventorySnapshot> getInventoryBatches() const override
    {
        QueryResult<InventorySnapshot> result;

        if (!inventory_.database.isOpen()) {
            result.outcome = {
                OperationStatus::Failed,
                ErrorCode::StorageFailure,
                "库存数据库未打开"
            };
            return result;
        }

        QSqlDatabase& db = inventory_.database;

        if (!db.transaction()) {
            result.outcome = {
                OperationStatus::Failed,
                ErrorCode::StorageFailure,
                "无法开始库存读取事务"
            };
            return result;
        }

        auto fail = [&](ErrorCode code, const std::string& message) {
            db.rollback();
            result.outcome = {
                OperationStatus::Failed,
                code,
                message
            };
            return result;
        };

        QSqlQuery revisionQuery(db);

        if (!revisionQuery.exec(
            "SELECT revision FROM shixu_inventory_meta WHERE id = 1"
        )) {
            return fail(ErrorCode::StorageFailure, "读取库存版本失败");
        }

        if (!revisionQuery.next()) {
            return fail(ErrorCode::InvalidData, "库存版本记录不存在");
        }

        result.value.revision =
            revisionQuery.value(0).toString().toStdString();

        revisionQuery.finish();

        QSqlQuery query(db);

        if (!query.exec(
            "SELECT id, name, quantity, unit, expiryDate, "
            "gramsPerUnit, totalWeightGrams "
            "FROM foods WHERE quantity > 0 ORDER BY id"
        )) {
            return fail(ErrorCode::StorageFailure, "读取库存批次失败");
        }

        while (query.next()) {
            InventoryBatch batch;

            batch.batchId = query.value(0).toLongLong();
            batch.name = query.value(1).toString().toUtf8().toStdString();
            batch.quantity = query.value(2).toDouble();
            batch.unit = query.value(3).toString().toUtf8().toStdString();
            batch.expiryDate = query.value(4).toString().toStdString();
            batch.gramsPerUnit = query.value(5).toDouble();
batch.totalWeightGrams = query.value(6).toDouble();

if (batch.totalWeightGrams == 0) {
    const QString unit = QString::fromStdString(batch.unit).trimmed().toLower();

    double grams = 0;

    if (unit == "g" || unit == "克") {
        grams = batch.quantity;
    } else if (unit == "kg" || unit == "千克" || unit == "公斤") {
        grams = batch.quantity * 1000.0;
    } else if (unit == "斤") {
        grams = batch.quantity * 500.0;
    } else if (unit == "mg" || unit == "毫克") {
        grams = batch.quantity / 1000.0;
    } else if (batch.gramsPerUnit > 0) {
        grams = batch.quantity * batch.gramsPerUnit;
    }

    batch.totalWeightGrams = grams;
}

            if (batch.batchId <= 0 ||
                batch.name.empty() ||
                batch.unit.empty() ||
                !std::isfinite(batch.quantity) ||
                batch.quantity <= 0 ||
                !std::isfinite(batch.gramsPerUnit) ||
                batch.gramsPerUnit < 0 ||
                !std::isfinite(batch.totalWeightGrams) ||
                batch.totalWeightGrams < 0) {
                return fail(ErrorCode::InvalidData, "库存批次数据无效");
            }

            if (!batch.expiryDate.empty()) {
                const QDate date = QDate::fromString(
                    QString::fromStdString(batch.expiryDate),
                    Qt::ISODate
                );

                if (!date.isValid() ||
                    date.toString(Qt::ISODate).toStdString()
                        != batch.expiryDate) {
                    return fail(ErrorCode::InvalidData, "库存到期日期无效");
                }
            }

            result.value.batches.push_back(batch);
        }

        query.finish();

        if (!db.commit()) {
            return fail(ErrorCode::StorageFailure, "提交库存读取事务失败");
        }

        result.outcome = {
            OperationStatus::Succeeded,
            ErrorCode::None,
            "库存批次读取成功"
        };

        return result;
    }

    QueryResult<std::vector<IngredientAvailability>>
    evaluateRequirements(const EvaluationRequest&) const override;

    QueryResult<ConsumptionResult>
    getConsumption(const EventId&) const override;

    ConsumptionPreview
    previewConsumption(const ConsumptionRequest&) const override;

    ConsumptionResult
    consumeForCooking(
        const ConsumptionRequest&,
        const std::string& previewToken
    ) override;

private:
    InventoryManager& inventory_;
};



QueryResult<std::vector<IngredientAvailability>>
InventoryContractAdapter::evaluateRequirements(
    const EvaluationRequest& request) const
{
    QueryResult<std::vector<IngredientAvailability>> result;

    auto fail = [&](ErrorCode code, const std::string& message) {
        result.outcome = {
            OperationStatus::Failed, code, message
        };
        return result;
    };

    const QString dateText =
        QString::fromStdString(request.referenceDate);
    const QDate referenceDate =
        QDate::fromString(dateText, Qt::ISODate);

    if (!referenceDate.isValid() ||
        referenceDate.toString(Qt::ISODate) != dateText) {
        return fail(ErrorCode::InvalidArgument,
                    "Invalid reference date");
    }

    QMap<QString, double> required;

    for (const auto& item : request.requirements) {
        const QString name =
            QString::fromUtf8(item.foodName.c_str());

        if (name.trimmed().isEmpty() ||
            !std::isfinite(item.requiredGrams) ||
            item.requiredGrams <= 0) {
            return fail(ErrorCode::InvalidArgument,
                        "Invalid ingredient requirement");
        }

        required[name] += item.requiredGrams;

        if (!std::isfinite(required[name])) {
            return fail(ErrorCode::InvalidArgument,
                        "Ingredient amount overflow");
        }
    }

    QMap<QString, double> available;
    QMap<QString, bool> unknown;
    QMap<QString, bool> expiring;

    for (const auto& batch : request.inventory.batches) {
        const QString name =
            QString::fromUtf8(batch.name.c_str());

        if (!required.contains(name))
            continue;

        if (!std::isfinite(batch.quantity) ||
            !std::isfinite(batch.totalWeightGrams) ||
            batch.quantity <= 0 ||
            batch.totalWeightGrams < 0) {
            return fail(ErrorCode::InvalidData,
                        "Invalid inventory batch");
        }

        QDate expiry;

        if (!batch.expiryDate.empty()) {
            const QString expiryText =
                QString::fromStdString(batch.expiryDate);

            expiry = QDate::fromString(
                expiryText, Qt::ISODate
            );

            if (!expiry.isValid() ||
                expiry.toString(Qt::ISODate) != expiryText) {
                return fail(ErrorCode::InvalidData,
                            "Invalid expiry date");
            }

            if (expiry < referenceDate)
                continue;
        }

        if (batch.totalWeightGrams <= 0) {
            unknown[name] = true;
            continue;
        }

        available[name] += batch.totalWeightGrams;

        if (!std::isfinite(available[name])) {
            return fail(ErrorCode::InvalidData,
                        "Available amount overflow");
        }

        if (expiry.isValid() &&
            expiry <= referenceDate.addDays(3)) {
            expiring[name] = true;
        }
    }

    for (auto it = required.cbegin();
         it != required.cend(); ++it) {

        IngredientAvailability item;

        item.foodName = it.key().toUtf8().toStdString();
        item.requiredGrams = it.value();
        item.knownAvailableGrams = available.value(it.key());
        item.knownGapGrams = std::max(
            0.0,
            item.requiredGrams - item.knownAvailableGrams
        );
        item.hasUnknownWeight = unknown.value(it.key());
        item.hasExpiringStock = expiring.value(it.key());

        if (item.knownAvailableGrams >=
            item.requiredGrams) {
            item.state = AvailabilityState::Enough;
        } else if (item.hasUnknownWeight) {
            item.state = AvailabilityState::Unknown;
        } else {
            item.state = AvailabilityState::Insufficient;
        }

        result.value.push_back(item);
    }

    result.outcome = {
        OperationStatus::Succeeded,
        ErrorCode::None,
        "Ingredient requirements evaluated"
    };

    return result;
}


QueryResult<ConsumptionResult>
InventoryContractAdapter::getConsumption(
    const EventId& eventId) const
{
    QueryResult<ConsumptionResult> result;

    auto fail = [&](ErrorCode code, const std::string& message) {
        result.outcome = {
            OperationStatus::Failed, code, message
        };
        return result;
    };

    if (eventId.empty()) {
        return fail(ErrorCode::InvalidArgument,
                    "Event ID cannot be empty");
    }

    if (!inventory_.database.isOpen()) {
        return fail(ErrorCode::StorageFailure,
                    "Inventory database is not open");
    }

    QSqlQuery query(inventory_.database);

    if (!query.prepare(
        "SELECT result_data "
        "FROM shixu_consumption_events "
        "WHERE event_id = :event_id"
    )) {
        return fail(ErrorCode::StorageFailure,
                    "Failed to prepare event query");
    }

    query.bindValue(
        ":event_id",
        QString::fromStdString(eventId)
    );

    if (!query.exec()) {
        return fail(ErrorCode::StorageFailure,
                    "Failed to query consumption event");
    }

    if (!query.next()) {
        return fail(ErrorCode::NotFound,
                    "Consumption event not found");
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            query.value(0).toString().toUtf8(),
            &parseError
        );

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return fail(ErrorCode::InvalidData,
                    "Stored consumption result is invalid");
    }

    const QJsonObject object = document.object();

    if (!object.contains("eventId") ||
        !object.value("eventId").isString() ||
        !object.contains("consumedExpiring") ||
        !object.value("consumedExpiring").isBool() ||
        !object.contains("consumed") ||
        !object.value("consumed").isArray()) {
        return fail(ErrorCode::InvalidData,
                    "Stored consumption result is incomplete");
    }

    result.value.eventId =
        object.value("eventId").toString().toStdString();

    result.value.consumedExpiring =
        object.value("consumedExpiring").toBool();

    if (result.value.eventId != eventId) {
        return fail(ErrorCode::InvalidData,
                    "Stored event ID does not match");
    }

    const QJsonArray consumedArray =
        object.value("consumed").toArray();

    for (const QJsonValue& value : consumedArray) {
        if (!value.isObject()) {
            return fail(ErrorCode::InvalidData,
                        "Invalid consumed batch");
        }

        const QJsonObject batchObject = value.toObject();

        BatchConsumption batch;

        if (!batchObject.value("batchId").isDouble() ||
            !batchObject.value("foodName").isString() ||
            !batchObject.value("consumedGrams").isDouble() ||
            !batchObject.value("quantityBefore").isDouble() ||
            !batchObject.value("quantityAfter").isDouble() ||
            !batchObject.value("weightBeforeGrams").isDouble() ||
            !batchObject.value("weightAfterGrams").isDouble() ||
            !batchObject.value("wasExpiring").isBool()) {
            return fail(ErrorCode::InvalidData,
                        "Incomplete consumed batch");
        }

        batch.batchId =
            static_cast<BatchId>(
                batchObject.value("batchId").toDouble()
            );

        batch.foodName =
            batchObject.value("foodName").toString()
                .toUtf8().toStdString();

        batch.consumedGrams =
            batchObject.value("consumedGrams").toDouble();

        batch.quantityBefore =
            batchObject.value("quantityBefore").toDouble();

        batch.quantityAfter =
            batchObject.value("quantityAfter").toDouble();

        batch.weightBeforeGrams =
            batchObject.value("weightBeforeGrams").toDouble();

        batch.weightAfterGrams =
            batchObject.value("weightAfterGrams").toDouble();

        batch.wasExpiring =
            batchObject.value("wasExpiring").toBool();

        result.value.consumed.push_back(batch);
    }


        if (!object.value("availability").isArray()) {
        return fail(
            ErrorCode::InvalidData,
            "Stored availability is missing"
        );
    }

    const QJsonArray availabilityArray =
        object.value("availability").toArray();

    for (const QJsonValue& value : availabilityArray) {
        if (!value.isObject()) {
            return fail(
                ErrorCode::InvalidData,
                "Invalid availability record"
            );
        }

        const QJsonObject item = value.toObject();

        if (!item.value("foodName").isString() ||
            !item.value("requiredGrams").isDouble() ||
            !item.value("knownAvailableGrams").isDouble() ||
            !item.value("knownGapGrams").isDouble() ||
            !item.value("hasUnknownWeight").isBool() ||
            !item.value("hasExpiringStock").isBool() ||
            !item.value("state").isDouble()) {
            return fail(
                ErrorCode::InvalidData,
                "Incomplete availability record"
            );
        }

        const int state = item.value("state").toInt();

        if (state < static_cast<int>(AvailabilityState::Enough) ||
            state > static_cast<int>(AvailabilityState::Unknown)) {
            return fail(
                ErrorCode::InvalidData,
                "Invalid availability state"
            );
        }

        IngredientAvailability food;

        food.foodName =
            item.value("foodName").toString().toUtf8().toStdString();

        food.requiredGrams =
            item.value("requiredGrams").toDouble();

        food.knownAvailableGrams =
            item.value("knownAvailableGrams").toDouble();

        food.knownGapGrams =
            item.value("knownGapGrams").toDouble();

        food.hasUnknownWeight =
            item.value("hasUnknownWeight").toBool();

        food.hasExpiringStock =
            item.value("hasExpiringStock").toBool();

        food.state =
            static_cast<AvailabilityState>(state);

        result.value.availability.push_back(food);
    }


    result.value.outcome = {
        OperationStatus::Succeeded,
        ErrorCode::None,
        "Consumption record found"
    };

    result.outcome = result.value.outcome;

    return result;
}




namespace {

struct PlannedInventoryBatch {
    InventoryBatch batch;
    QDate expiry;
};

bool isValidDate(const std::string& text, QDate& date)
{
    const QString value = QString::fromStdString(text);
    date = QDate::fromString(value, Qt::ISODate);

    return date.isValid() &&
           date.toString(Qt::ISODate) == value;
}

bool expiresEarlier(
    const PlannedInventoryBatch& a,
    const PlannedInventoryBatch& b)
{
    const bool aValid = a.expiry.isValid();
    const bool bValid = b.expiry.isValid();

    if (aValid && bValid) {
        if (a.expiry != b.expiry)
            return a.expiry < b.expiry;
    } else if (aValid != bValid) {
        return aValid;
    }

    return a.batch.batchId < b.batch.batchId;
}

} // anonymous namespace



ConsumptionPreview
InventoryContractAdapter::previewConsumption(
    const ConsumptionRequest& request) const
{
    ConsumptionPreview preview;

    auto fail = [&](ErrorCode code, const std::string& message) {
        preview.outcome = {
            OperationStatus::Failed, code, message
        };
        return preview;
    };

    if (request.eventId.empty()) {
        return fail(ErrorCode::InvalidArgument,
                    "Event ID cannot be empty");
    }

    const QString completedAt =
        QString::fromStdString(request.completedAt);

    const QDateTime completed =
        QDateTime::fromString(
            completedAt, "yyyy-MM-dd HH:mm:ss"
        );

    if (!completed.isValid() ||
        completed.toString("yyyy-MM-dd HH:mm:ss") != completedAt) {
        return fail(ErrorCode::InvalidArgument,
                    "Invalid completion time");
    }

    if (request.requirements.empty()) {
        return fail(ErrorCode::InvalidArgument,
                    "No ingredient requirements");
    }

    const auto snapshot = getInventoryBatches();

    if (snapshot.outcome.status != OperationStatus::Succeeded) {
        preview.outcome = snapshot.outcome;
        return preview;
    }

    EvaluationRequest evaluation;
    evaluation.inventory = snapshot.value;
    evaluation.referenceDate =
        completed.date().toString(Qt::ISODate).toStdString();
    evaluation.requirements = request.requirements;

    const auto evaluated = evaluateRequirements(evaluation);

    if (evaluated.outcome.status != OperationStatus::Succeeded) {
        preview.outcome = evaluated.outcome;
        return preview;
    }

    preview.availability = evaluated.value;


    // Check external ingredient confirmations.
    QSet<QString> externalNames;
    QSet<QString> requiredNames;

    for (const auto& item : preview.availability) {
        requiredNames.insert(
            QString::fromUtf8(item.foodName.c_str())
        );
    }

    for (const auto& name : request.externallyPreparedFoods) {
        const QString foodName =
            QString::fromUtf8(name.c_str());

        if (foodName.isEmpty() ||
            !requiredNames.contains(foodName) ||
            externalNames.contains(foodName)) {
            return fail(ErrorCode::InvalidArgument,
                        "Invalid external ingredient confirmation");
        }

        externalNames.insert(foodName);
    }

    for (const auto& item : preview.availability) {
        if (item.knownGapGrams > 1e-6 &&
            !externalNames.contains(
                QString::fromUtf8(item.foodName.c_str())
            )) {
            preview.outcome = {
                OperationStatus::NeedsConfirmation,
                ErrorCode::UnconfirmedIngredients,
                "Some ingredients require confirmation"
            };
            return preview;
        }
    }

    // Plan consumption using FEFO: earliest expiry first.
    std::vector<PlannedInventoryBatch> batches;

    for (const auto& batch : snapshot.value.batches) {
        if (batch.quantity <= 0 ||
            batch.totalWeightGrams <= 0) {
            continue;
        }

        PlannedInventoryBatch planned;
        planned.batch = batch;

        if (!batch.expiryDate.empty()) {
            if (!isValidDate(batch.expiryDate, planned.expiry)) {
                return fail(ErrorCode::InvalidData,
                            "Invalid batch expiry date");
            }

            if (planned.expiry < completed.date()) {
                continue;
            }
        }

        batches.push_back(planned);
    }

    std::sort(batches.begin(), batches.end(), expiresEarlier);

    for (const auto& ingredient : preview.availability) {
        double remaining = ingredient.requiredGrams;

        for (const auto& item : batches) {
            if (remaining <= 1e-6)
                break;

            if (item.batch.name != ingredient.foodName)
                continue;

            const double availableWeight =
                item.batch.totalWeightGrams;

            const double consumedWeight =
                std::min(remaining, availableWeight);

            if (consumedWeight <= 1e-6)
                continue;

            BatchConsumption consumption;
            consumption.batchId = item.batch.batchId;
            consumption.foodName = item.batch.name;
            consumption.consumedGrams = consumedWeight;
            consumption.quantityBefore = item.batch.quantity;
            consumption.weightBeforeGrams = availableWeight;

            consumption.weightAfterGrams =
                std::max(0.0, availableWeight - consumedWeight);

            consumption.quantityAfter =
                item.batch.quantity *
                (consumption.weightAfterGrams / availableWeight);

            consumption.wasExpiring =
                item.expiry.isValid() &&
                item.expiry <= completed.date().addDays(3);

            preview.planned.push_back(consumption);

            remaining -= consumedWeight;
        }
    }


    preview.previewToken = makePreviewToken(
    request,
    QString::fromStdString(snapshot.value.revision)
);



    preview.outcome = {
    OperationStatus::Succeeded,
    ErrorCode::None,
    "Consumption preview generated"
};

return preview;
}



static QByteArray canonicalConsumptionRequest(
    const ConsumptionRequest& request)
{
    QMap<QString, double> requirements;

    for (const auto& item : request.requirements) {
        const QString name =
            QString::fromUtf8(item.foodName.c_str());

        requirements[name] += item.requiredGrams;
    }

    QJsonArray requirementsArray;

    for (auto it = requirements.cbegin();
         it != requirements.cend(); ++it) {

        QJsonObject ingredient;
        ingredient["name"] = it.key();
        ingredient["grams"] = it.value();

        requirementsArray.append(ingredient);
    }

    QStringList externalNames;

    for (const auto& name : request.externallyPreparedFoods) {
        externalNames.append(
            QString::fromUtf8(name.c_str())
        );
    }

    externalNames.sort();

    QJsonArray externalArray;

    for (const QString& name : externalNames) {
        externalArray.append(name);
    }

    QJsonObject object;

    object["eventId"] =
        QString::fromStdString(request.eventId);

    object["completedAt"] =
        QString::fromStdString(request.completedAt);

    object["requirements"] = requirementsArray;

    object["externallyPreparedFoods"] = externalArray;

    return QJsonDocument(object).toJson(
        QJsonDocument::Compact
    );
}

static std::string makePreviewToken(
    const ConsumptionRequest& request,
    const QString& revision)
{
    QJsonObject tokenData;

    tokenData["request"] = QString::fromUtf8(
        canonicalConsumptionRequest(request)
    );

    tokenData["revision"] = revision;

    const QByteArray data =
        QJsonDocument(tokenData).toJson(
            QJsonDocument::Compact
        );

    return QCryptographicHash::hash(
        data,
        QCryptographicHash::Sha256
    ).toHex().toStdString();
}



static QJsonObject consumptionResultToJson(
    const ConsumptionResult& result)
{
    QJsonObject object;

    object["eventId"] =
        QString::fromStdString(result.eventId);

    object["consumedExpiring"] =
        result.consumedExpiring;

    QJsonArray consumedArray;

    for (const auto& batch : result.consumed) {
        QJsonObject item;

        item["batchId"] =
            static_cast<double>(batch.batchId);
        item["foodName"] =
            QString::fromUtf8(batch.foodName.c_str());
        item["consumedGrams"] = batch.consumedGrams;
        item["quantityBefore"] = batch.quantityBefore;
        item["quantityAfter"] = batch.quantityAfter;
        item["weightBeforeGrams"] = batch.weightBeforeGrams;
        item["weightAfterGrams"] = batch.weightAfterGrams;
        item["wasExpiring"] = batch.wasExpiring;

        consumedArray.append(item);
    }

    object["consumed"] = consumedArray;


        // 保存扣减前的食材库存评估结果
    QJsonArray availabilityArray;

    for (const auto& food : result.availability) {
        QJsonObject item;

        item["foodName"] =
            QString::fromUtf8(food.foodName.c_str());
        item["requiredGrams"] = food.requiredGrams;
        item["knownAvailableGrams"] = food.knownAvailableGrams;
        item["knownGapGrams"] = food.knownGapGrams;
        item["hasUnknownWeight"] = food.hasUnknownWeight;
        item["hasExpiringStock"] = food.hasExpiringStock;
        item["state"] = static_cast<int>(food.state);

        availabilityArray.append(item);
    }

    object["availability"] = availabilityArray;


    return object;
}


ConsumptionResult
InventoryContractAdapter::consumeForCooking(
    const ConsumptionRequest& request,
    const std::string& previewToken)
{
    ConsumptionResult result;
    result.eventId = request.eventId;


        const QString completedAtText =
        QString::fromStdString(request.completedAt);

    const QDateTime completedAt =
        QDateTime::fromString(
            completedAtText,
            "yyyy-MM-dd HH:mm:ss"
        );

    if (!completedAt.isValid() ||
        completedAt.toString("yyyy-MM-dd HH:mm:ss") != completedAtText) {
        result.outcome = {
            OperationStatus::Failed,
            ErrorCode::InvalidArgument,
            "Invalid completion time"
        };
        return result;
    }

    if (request.requirements.empty()) {
        result.outcome = {
            OperationStatus::Failed,
            ErrorCode::InvalidArgument,
            "No ingredient requirements"
        };
        return result;
    }

    QSet<QString> requirementNames;

    for (const auto& item : request.requirements) {
        const QString name =
            QString::fromUtf8(item.foodName.c_str());

        if (name.trimmed().isEmpty() ||
            !std::isfinite(item.requiredGrams) ||
            item.requiredGrams <= 0) {
            result.outcome = {
                OperationStatus::Failed,
                ErrorCode::InvalidArgument,
                "Invalid ingredient requirement"
            };
            return result;
        }

        requirementNames.insert(name);
    }

    QSet<QString> externalNames;

    for (const auto& name : request.externallyPreparedFoods) {
        const QString foodName =
            QString::fromUtf8(name.c_str());

        if (!requirementNames.contains(foodName) ||
            externalNames.contains(foodName)) {
            result.outcome = {
                OperationStatus::Failed,
                ErrorCode::InvalidArgument,
                "Invalid external ingredient confirmation"
            };
            return result;
        }

        externalNames.insert(foodName);
    }




    auto fail = [&](OperationStatus status,
                    ErrorCode code,
                    const std::string& message) {
        result.outcome = {status, code, message};
        return result;
    };

    if (request.eventId.empty() || previewToken.empty()) {
        return fail(OperationStatus::Failed,
                    ErrorCode::InvalidArgument,
                    "Event ID or preview token is empty");
    }

    if (!inventory_.database.isOpen()) {
        return fail(OperationStatus::Failed,
                    ErrorCode::StorageFailure,
                    "Inventory database is not open");
    }

    QSqlDatabase& db = inventory_.database;

    if (!db.transaction()) {
        return fail(OperationStatus::Failed,
                    ErrorCode::StorageFailure,
                    "Cannot start consumption transaction");
    }

    auto rollback = [&](OperationStatus status,
                        ErrorCode code,
                        const std::string& message) {
        db.rollback();
        return fail(status, code, message);
    };

    const QByteArray requestData =
        canonicalConsumptionRequest(request);

    QSqlQuery eventQuery(db);

    if (!eventQuery.prepare(
        "SELECT request_data, result_data FROM shixu_consumption_events "
        "WHERE event_id = :event_id"
    )) {
        return rollback(OperationStatus::Failed,
                        ErrorCode::StorageFailure,
                        "Cannot prepare event query");
    }

    eventQuery.bindValue(
        ":event_id",
        QString::fromStdString(request.eventId)
    );

    if (!eventQuery.exec()) {
        return rollback(OperationStatus::Failed,
                        ErrorCode::StorageFailure,
                        "Cannot query consumption event");
    }

        if (eventQuery.next()) {
        const QByteArray previousRequest =
            eventQuery.value(0).toString().toUtf8();

        eventQuery.finish();

        if (previousRequest != requestData) {
            return rollback(
                OperationStatus::Failed,
                ErrorCode::EventConflict,
                "Event ID belongs to another request"
            );
        }

        if (!db.rollback()) {
            return fail(
                OperationStatus::Failed,
                ErrorCode::StorageFailure,
                "Cannot close duplicate event transaction"
            );
        }

        const auto previous = getConsumption(request.eventId);

        if (previous.outcome.status != OperationStatus::Succeeded) {
            return fail(
                OperationStatus::Failed,
                previous.outcome.errorCode,
                "Cannot restore previous consumption result"
            );
        }

        ConsumptionResult restored = previous.value;

        restored.outcome = {
            OperationStatus::AlreadyApplied,
            ErrorCode::None,
            "Consumption event was already applied"
        };

        return restored;
    }

    eventQuery.finish();



    QSqlQuery revisionQuery(db);

    if (!revisionQuery.exec(
        "SELECT revision FROM shixu_inventory_meta WHERE id = 1"
    )) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot read inventory revision"
        );
    }

    if (!revisionQuery.next()) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::InvalidData,
            "Inventory revision record is missing"
        );
    }

    const QString currentRevision =
        revisionQuery.value(0).toString();

    revisionQuery.finish();

        QList<InventoryBatch> currentBatches;

    QSqlQuery batchQuery(db);

    if (!batchQuery.exec(
        "SELECT id, name, quantity, unit, expiryDate, "
        "gramsPerUnit, totalWeightGrams "
        "FROM foods WHERE quantity > 0 ORDER BY id"
    )) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot read current inventory batches"
        );
    }

    while (batchQuery.next()) {
        InventoryBatch batch;

        batch.batchId = batchQuery.value(0).toLongLong();
        batch.name =
            batchQuery.value(1).toString().toUtf8().toStdString();
        batch.quantity = batchQuery.value(2).toDouble();
        batch.unit =
            batchQuery.value(3).toString().toUtf8().toStdString();
        batch.expiryDate =
            batchQuery.value(4).toString().toStdString();
        batch.gramsPerUnit = batchQuery.value(5).toDouble();
        batch.totalWeightGrams = batchQuery.value(6).toDouble();

        currentBatches.append(batch);
    }

    batchQuery.finish();

        const std::string expectedToken =
        makePreviewToken(request, currentRevision);

    if (previewToken != expectedToken) {
        return rollback(
            OperationStatus::NeedsConfirmation,
            ErrorCode::InventoryChanged,
            "Inventory changed; please preview again"
        );
    }


    const QDate cookingDate = QDateTime::fromString(
        QString::fromStdString(request.completedAt),
        "yyyy-MM-dd HH:mm:ss"
    ).date();

    if (!cookingDate.isValid()) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::InvalidArgument,
            "Invalid cooking date"
        );
    }

    std::vector<PlannedInventoryBatch> usableBatches;

    for (const InventoryBatch& batch : currentBatches) {
        if (batch.quantity <= 0 || batch.totalWeightGrams <= 0)
            continue;

        PlannedInventoryBatch item;
        item.batch = batch;

        if (!batch.expiryDate.empty()) {
            if (!isValidDate(batch.expiryDate, item.expiry)) {
                return rollback(
                    OperationStatus::Failed,
                    ErrorCode::InvalidData,
                    "Invalid inventory expiry date"
                );
            }

            if (item.expiry < cookingDate)
                continue;
        }

        usableBatches.push_back(item);
    }

    std::sort(
        usableBatches.begin(),
        usableBatches.end(),
        expiresEarlier
    );

    std::vector<BatchConsumption> plannedConsumption;
    QMap<QString, double> requiredGrams;

    for (const auto& requirement : request.requirements) {
        requiredGrams[
            QString::fromUtf8(requirement.foodName.c_str())
        ] += requirement.requiredGrams;
    }

    for (auto it = requiredGrams.cbegin();
         it != requiredGrams.cend(); ++it) {

        double remaining = it.value();
        const std::string foodName =
            it.key().toUtf8().toStdString();

        for (const auto& item : usableBatches) {
            if (remaining <= 1e-6)
                break;

            if (item.batch.name != foodName)
                continue;

            const double beforeWeight =
                item.batch.totalWeightGrams;

            const double consumed =
                std::min(remaining, beforeWeight);

            if (consumed <= 1e-6)
                continue;

            BatchConsumption record;
            record.batchId = item.batch.batchId;
            record.foodName = foodName;
            record.consumedGrams = consumed;
            record.quantityBefore = item.batch.quantity;
            record.weightBeforeGrams = beforeWeight;
            record.weightAfterGrams =
                std::max(0.0, beforeWeight - consumed);

            record.quantityAfter =
                item.batch.quantity *
                (record.weightAfterGrams / beforeWeight);

            record.wasExpiring =
                item.expiry.isValid() &&
                item.expiry <= cookingDate.addDays(3);

            plannedConsumption.push_back(record);

            remaining -= consumed;
        }
    }



        for (auto it = requiredGrams.cbegin();
         it != requiredGrams.cend(); ++it) {

        const QString foodName = it.key();
        double plannedGrams = 0.0;

        for (const auto& item : plannedConsumption) {
            if (QString::fromUtf8(item.foodName.c_str()) == foodName) {
                plannedGrams += item.consumedGrams;
            }
        }

        const double gapGrams = it.value() - plannedGrams;

        if (gapGrams > 1e-6 &&
            !externalNames.contains(foodName)) {

            return rollback(
                OperationStatus::NeedsConfirmation,
                ErrorCode::UnconfirmedIngredients,
                "Ingredient shortage requires confirmation"
            );
        }
    }



        QSqlQuery updateQuery(db);

    if (!updateQuery.prepare(
        "UPDATE foods "
        "SET quantity = :quantity, "
        "totalWeightGrams = :weight "
        "WHERE id = :id "
        "AND quantity = :oldQuantity "
        "AND totalWeightGrams = :oldWeight"
    )) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot prepare inventory update"
        );
    }

    for (const auto& item : plannedConsumption) {
        updateQuery.bindValue(":quantity", item.quantityAfter);
        updateQuery.bindValue(":weight", item.weightAfterGrams);
        updateQuery.bindValue(":id", static_cast<qlonglong>(item.batchId));
        updateQuery.bindValue(":oldQuantity", item.quantityBefore);
        updateQuery.bindValue(":oldWeight", item.weightBeforeGrams);

        if (!updateQuery.exec()) {
            return rollback(
                OperationStatus::Failed,
                ErrorCode::StorageFailure,
                "Inventory update failed"
            );
        }

        if (updateQuery.numRowsAffected() != 1) {
            return rollback(
                OperationStatus::NeedsConfirmation,
                ErrorCode::InventoryChanged,
                "Inventory batch changed"
            );
        }

        updateQuery.finish();
    }


        EvaluationRequest evaluationRequest;
    evaluationRequest.inventory.revision =
        currentRevision.toStdString();

    evaluationRequest.inventory.batches.reserve(
        currentBatches.size()
    );

    for (const auto& batch : currentBatches) {
        evaluationRequest.inventory.batches.push_back(batch);
    }

    evaluationRequest.requirements = request.requirements;
    evaluationRequest.referenceDate =
        QString::fromStdString(request.completedAt)
            .left(10)
            .toStdString();

    const auto evaluation =
        evaluateRequirements(evaluationRequest);

    if (evaluation.outcome.status != OperationStatus::Succeeded) {
        return rollback(
            OperationStatus::Failed,
            evaluation.outcome.errorCode,
            "Cannot evaluate inventory before consumption"
        );
    }

    result.availability = evaluation.value;








        result.consumed = plannedConsumption;

    result.consumedExpiring = false;

    for (const auto& item : result.consumed) {
        if (item.wasExpiring) {
            result.consumedExpiring = true;
            break;
        }
    }

    result.outcome = {
        OperationStatus::Succeeded,
        ErrorCode::None,
        "Consumption completed"
    };

    const QByteArray resultData =
        QJsonDocument(
            consumptionResultToJson(result)
        ).toJson(QJsonDocument::Compact);

    QSqlQuery insertEvent(db);

    if (!insertEvent.prepare(
        "INSERT INTO shixu_consumption_events "
        "(event_id, request_data, result_data) "
        "VALUES (:event_id, :request_data, :result_data)"
    )) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot prepare consumption event"
        );
    }

    insertEvent.bindValue(
        ":event_id",
        QString::fromStdString(request.eventId)
    );

    insertEvent.bindValue(
        ":request_data",
        QString::fromUtf8(requestData)
    );

    insertEvent.bindValue(
        ":result_data",
        QString::fromUtf8(resultData)
    );

    if (!insertEvent.exec()) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot save consumption event"
        );
    }

    insertEvent.finish();

    QSqlQuery updateRevision(db);

    if (!updateRevision.exec(
        "UPDATE shixu_inventory_meta "
        "SET revision = revision + 1 "
        "WHERE id = 1"
    ) || updateRevision.numRowsAffected() != 1) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot update inventory revision"
        );
    }

    updateRevision.finish();

    if (!db.commit()) {
        return rollback(
            OperationStatus::Failed,
            ErrorCode::StorageFailure,
            "Cannot commit consumption transaction"
        );
    }

    return result;
}

    std::unique_ptr<IInventoryService> createInventoryService(
    InventoryManager& inventory)
{
    return std::make_unique<InventoryContractAdapter>(inventory);
}






}// namespace Shixu::Contracts::V1
