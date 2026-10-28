
#include "inventory/inventory.h"
#include "contracts/CoreContracts.h"
#include "contracts/ServiceFactories.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

#include <QSqlQuery>
#include <QSqlDatabase>


#include <algorithm>

#include <cmath>
#include <memory>

using namespace Shixu::Contracts::V1;

static bool approximatelyEqual(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString originalPath = QDir::currentPath();
    QTemporaryDir directory(
        originalPath + QStringLiteral("/inventory-contract-test-XXXXXX")
    );

    if (!directory.isValid() ||
        !QDir::setCurrent(directory.path())) {
        qCritical() << "Cannot create temporary test directory";
        return 1;
    }

    int failures = 0;

    auto check = [&](bool condition, const char* message) {
        if (condition) {
            qInfo() << "PASS:" << message;
        } else {
            qCritical() << "FAIL:" << message;
            ++failures;
        }
    };

    {
        InventoryManager inventory;

        FoodCandidate apple;
        apple.name = QStringLiteral("苹果");
        apple.quantity = 3.0;
        apple.unit = QStringLiteral("个");
        apple.expiryDate = QDate(2026, 10, 30);
        apple.gramsPerUnit = 100.0;
        apple.totalWeightGrams = 300.0;

        inventory.addFood(apple);

        auto service = createInventoryService(inventory);

        check(service != nullptr, "Factory creates inventory service");

        if (service) {
            const auto before = service->getInventoryBatches();

            check(
                before.outcome.status == OperationStatus::Succeeded,
                "Inventory snapshot succeeds"
            );

            ConsumptionRequest request;
            request.eventId =
                "123e4567-e89b-42d3-a456-426614174000";
            request.completedAt = "2026-10-28 12:00:00";
            request.requirements.push_back({"苹果", 150.0});

            const auto preview = service->previewConsumption(request);

            check(
                preview.outcome.status == OperationStatus::Succeeded,
                "Consumption preview succeeds"
            );

            check(
                !preview.previewToken.empty(),
                "Preview returns a token"
            );

            if (preview.outcome.status == OperationStatus::Succeeded &&
                !preview.previewToken.empty()) {

                const auto first =
                    service->consumeForCooking(
                        request, preview.previewToken
                    );

                check(
                    first.outcome.status == OperationStatus::Succeeded,
                    "First consumption succeeds"
                );

                check(
                    !first.availability.empty(),
                    "Consumption includes availability"
                );

                const auto after = service->getInventoryBatches();

                check(
                    after.outcome.status == OperationStatus::Succeeded,
                    "Inventory snapshot after consumption succeeds"
                );

                if (before.outcome.status == OperationStatus::Succeeded &&
                    after.outcome.status == OperationStatus::Succeeded &&
                    !before.value.batches.empty() &&
                    !after.value.batches.empty()) {

                    const double weightBefore =
                        before.value.batches.front().totalWeightGrams;

                    const double weightAfter =
                        after.value.batches.front().totalWeightGrams;

                    check(
                        approximatelyEqual(
                            weightBefore - weightAfter, 150.0
                        ),
                        "Consumption deducts 150 grams"
                    );
                }

                const auto repeated =
                    service->consumeForCooking(
                        request, preview.previewToken
                    );

                check(
                    repeated.outcome.status ==
                        OperationStatus::AlreadyApplied,
                    "Duplicate event is already applied"
                );

                check(
                    !repeated.availability.empty(),
                    "Duplicate event restores availability"
                );

                const auto history =
                    service->getConsumption(request.eventId);

                check(
                    history.outcome.status == OperationStatus::Succeeded,
                    "Committed event can be queried"
                );

                check(
                    !history.value.availability.empty(),
                    "History restores availability"
                );

                const auto afterRepeat =
                    service->getInventoryBatches();

                if (after.outcome.status == OperationStatus::Succeeded &&
                    afterRepeat.outcome.status == OperationStatus::Succeeded &&
                    !after.value.batches.empty() &&
                    !afterRepeat.value.batches.empty()) {

                    check(
                        approximatelyEqual(
                            after.value.batches.front().totalWeightGrams,
                            afterRepeat.value.batches.front().totalWeightGrams
                        ),
                        "Duplicate event does not deduct twice"
                    );
                }
            }
        }
    }



    // 边界测试：库存不足、过期食材和失败回滚
    {
        InventoryManager inventory;

        FoodCandidate milk;
        milk.name = QStringLiteral("牛奶");
        milk.quantity = 1.0;
        milk.unit = QStringLiteral("盒");
        milk.expiryDate = QDate(2026, 11, 5);
        milk.gramsPerUnit = 200.0;
        milk.totalWeightGrams = 200.0;

        FoodCandidate expiredEgg;
        expiredEgg.name = QStringLiteral("鸡蛋");
        expiredEgg.quantity = 2.0;
        expiredEgg.unit = QStringLiteral("个");
        expiredEgg.expiryDate = QDate(2026, 10, 20);
        expiredEgg.gramsPerUnit = 50.0;
        expiredEgg.totalWeightGrams = 100.0;

        check(inventory.addFoods({milk, expiredEgg}),
              "Boundary test data inserted");

        auto service = createInventoryService(inventory);

        if (service) {
            const auto before = service->getInventoryBatches();

            ConsumptionRequest shortage;
            shortage.eventId =
                "123e4567-e89b-42d3-a456-426614174001";
            shortage.completedAt = "2026-10-28 12:00:00";
            shortage.requirements.push_back({"牛奶", 300.0});

            const auto shortagePreview =
                service->previewConsumption(shortage);

            check(
                shortagePreview.outcome.status ==
                    OperationStatus::NeedsConfirmation,
                "Shortage requires confirmation"
            );

            check(
                shortagePreview.previewToken.empty(),
                "Unconfirmed shortage has no preview token"
            );

            ConsumptionRequest expired;
            expired.eventId =
                "123e4567-e89b-42d3-a456-426614174002";
            expired.completedAt = "2026-10-28 12:00:00";
            expired.requirements.push_back({"鸡蛋", 50.0});

            const auto expiredPreview =
                service->previewConsumption(expired);

            check(
                expiredPreview.outcome.status ==
                    OperationStatus::NeedsConfirmation,
                "Expired stock cannot satisfy requirement"
            );

            ConsumptionRequest invalid;
            invalid.eventId =
                "123e4567-e89b-42d3-a456-426614174003";
            invalid.completedAt = "2026-10-28 12:00:00";
            invalid.requirements.push_back({"牛奶", -50.0});

            const auto invalidResult =
                service->consumeForCooking(invalid, "invalid-token");

            check(
                invalidResult.outcome.status ==
                    OperationStatus::Failed,
                "Invalid consumption is rejected"
            );

            const auto after = service->getInventoryBatches();

            if (before.outcome.status == OperationStatus::Succeeded &&
                after.outcome.status == OperationStatus::Succeeded) {

                check(
                    before.value.revision == after.value.revision,
                    "Rejected operations do not change revision"
                );

                check(
                    before.value.batches.size() ==
                        after.value.batches.size(),
                    "Rejected operations do not change batch count"
                );

                bool unchanged = true;

                for (const auto& oldBatch : before.value.batches) {
                    bool found = false;

                    for (const auto& newBatch : after.value.batches) {
                        if (oldBatch.batchId == newBatch.batchId) {
                            found = true;

                            if (!approximatelyEqual(
                                    oldBatch.quantity,
                                    newBatch.quantity) ||
                                !approximatelyEqual(
                                    oldBatch.totalWeightGrams,
                                    newBatch.totalWeightGrams)) {
                                unchanged = false;
                            }

                            break;
                        }
                    }

                    if (!found) {
                        unchanged = false;
                    }
                }

                check(
                    unchanged,
                    "Rejected operations preserve inventory"
                );
            } else {
                check(false, "Boundary inventory snapshots succeed");
            }
        } else {
            check(false, "Boundary inventory service created");
        }
    }


    // 旧库存重量换算测试
    {
        InventoryManager inventory;

        FoodCandidate rice;
        rice.name = QStringLiteral("测试大米");
        rice.quantity = 500.0;
        rice.unit = QStringLiteral("g");
        rice.expiryDate = QDate(2026, 12, 31);
        rice.gramsPerUnit = 0.0;
        rice.totalWeightGrams = 0.0;

        inventory.addFood(rice);

        QSqlDatabase db =
    QSqlDatabase::database("ShixuInventoryConnection");

QSqlQuery legacyQuery(db);

check(
    legacyQuery.exec(
        "UPDATE foods SET totalWeightGrams = 0 "
        "WHERE name = '测试大米'"
    ),
    "Simulate legacy inventory with zero stored weight"
);

        auto service = createInventoryService(inventory);

        if (service) {
            const auto snapshot = service->getInventoryBatches();

            for (const auto& batch : snapshot.value.batches) {
    
}

            check(
                snapshot.outcome.status == OperationStatus::Succeeded,
                "Legacy inventory snapshot succeeds"
            );

            if (snapshot.outcome.status == OperationStatus::Succeeded) {
    bool found = false;

    for (const auto& batch : snapshot.value.batches) {
        if (batch.name == QStringLiteral("测试大米").toStdString()) {
            found = true;

            check(
                approximatelyEqual(batch.totalWeightGrams, 500.0),
                "Legacy 500 g converts to 500 grams"
            );
        }
    }

    check(found, "Legacy rice batch found");
}
        }

        ConsumptionRequest request;
request.eventId = "123e4567-e89b-42d3-a456-426614174004";
request.completedAt = "2026-10-28 12:00:00";
request.requirements.push_back({"测试大米", 200.0});

const auto preview = service->previewConsumption(request);

check(
    preview.outcome.status == OperationStatus::Succeeded,
    "Legacy inventory preview succeeds"
);

if (preview.outcome.status == OperationStatus::Succeeded) {
    const auto consumed =
        service->consumeForCooking(request, preview.previewToken);

    check(
        consumed.outcome.status == OperationStatus::Succeeded,
        "Legacy inventory consumption succeeds"
    );

    const auto after = service->getInventoryBatches();

    bool verified = false;

    for (const auto& batch : after.value.batches) {
        if (batch.name == "测试大米") {
            verified = approximatelyEqual(
                batch.totalWeightGrams, 300.0
            );
        }
    }

    check(verified, "Legacy inventory remaining weight is 300 g");
}


    }

    // 无到期日期批次排序测试
    {
        InventoryManager inventory;

        FoodCandidate first;
        first.name = QStringLiteral("排序测试大米");
        first.quantity = 100;
        first.unit = QStringLiteral("g");
        first.gramsPerUnit = 0;
        first.totalWeightGrams = 100;

        FoodCandidate second = first;

        inventory.addFood(first);
        inventory.addFood(second);

        auto service = createInventoryService(inventory);

        if (service) {
            const auto snapshot = service->getInventoryBatches();

            std::vector<long long> ids;

            for (const auto& batch : snapshot.value.batches) {
                if (batch.name == "排序测试大米") {
                    ids.push_back(batch.batchId);
                }
            }

            check(ids.size() == 2,
                  "Two undated batches exist");

            if (ids.size() == 2) {
                ConsumptionRequest request;
                request.eventId =
                    "123e4567-e89b-42d3-a456-426614174005";
                request.completedAt = "2026-10-28 12:00:00";
                request.requirements.push_back(
                    {"排序测试大米", 50.0}
                );

                const auto preview =
                    service->previewConsumption(request);

                check(
                    preview.outcome.status ==
                        OperationStatus::Succeeded,
                    "Undated batch preview succeeds"
                );

                if (!preview.planned.empty()) {
                    check(
                        preview.planned.front().batchId ==
                            *std::min_element(ids.begin(), ids.end()),
                        "Undated batches use smallest ID first"
                    );
                } else {
                    check(false, "Undated consumption plan exists");
                }
            }
        }
    }

    QDir::setCurrent(originalPath);

    qInfo() << "Test failures:" << failures;

    return failures == 0 ? 0 : 2;
}
