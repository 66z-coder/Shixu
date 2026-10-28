#include "contracts/ServiceFactories.h"
#include "reward/RewardSystem.h"

#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <iostream>

namespace V1 = Shixu::Contracts::V1;

namespace {
bool expect(bool condition, const char* message)
{
    if (!condition) std::cerr << message << '\n';
    return condition;
}

V1::CookingRewardRequest event(const char* id, const std::string& time, bool expiring)
{
    return {id, time, expiring};
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/reward-v1-XXXXXX"));
    if (!expect(directory.isValid(), "Temporary directory unavailable")) return 1;

    const auto first = event("5f778a96-91c7-4e42-a5ae-d7ad189d6101", "2026-10-28 18:00:00", true);
    const auto second = event("5f778a96-91c7-4e42-a5ae-d7ad189d6102", "2026-10-28 20:00:00", false);
    const QString path = directory.filePath(QStringLiteral("events.ini"));
    {
        RewardSystem rewards(nullptr, path);
        auto service = V1::createRewardService(rewards);
        int changes = 0;
        QObject::connect(&rewards, &RewardSystem::changed, [&changes] { ++changes; });
        const auto a = service->applyCookingRewardOnce(first);
        const auto b = service->applyCookingRewardOnce(second);
        const auto replay = service->applyCookingRewardOnce(first);
        const auto conflict = service->applyCookingRewardOnce(
            event(first.eventId.c_str(), "2026-10-28 18:00:00", false));
        const auto growth = service->getGrowthSnapshot("2026-10-28");
        if (!expect(a.outcome.status == V1::OperationStatus::Succeeded
                        && a.pointsAdded == 30 && a.newCheckIn
                        && a.totalCookingCount == 1 && a.totalPoints == 30,
                    "E01 first expiring cooking award")
            || !expect(b.outcome.status == V1::OperationStatus::Succeeded
                        && b.pointsAdded == 10 && !b.newCheckIn
                        && b.totalCookingCount == 2 && b.totalPoints == 40,
                    "E02 second cooking on same day")
            || !expect(replay.outcome.status == V1::OperationStatus::AlreadyApplied
                        && replay.pointsAdded == 30 && replay.totalPoints == 30
                        && replay.totalCookingCount == 1,
                    "E04 replay must return original snapshot")
            || !expect(conflict.outcome.errorCode == V1::ErrorCode::EventConflict,
                    "Same ID with different content must conflict")
            || !expect(growth.outcome.status == V1::OperationStatus::Succeeded
                        && growth.value.totalCookingCount == 2
                        && growth.value.totalCheckIns == 1
                        && growth.value.totalPoints == 40
                        && rewards.points() == 40 && changes == 2,
                    "E04 current growth snapshot must not reapply event")) return 1;
    }
    {
        RewardSystem restored(nullptr, path);
        auto service = V1::createRewardService(restored);
        const auto growth = service->getGrowthSnapshot("2026-10-28");
        const auto replay = service->applyCookingRewardOnce(first);
        if (!expect(growth.outcome.status == V1::OperationStatus::Succeeded
                        && growth.value.totalCookingCount == 2
                        && growth.value.totalPoints == 40
                        && growth.value.checkedInOnReferenceDate,
                    "Growth state must survive restart")
            || !expect(replay.outcome.status == V1::OperationStatus::AlreadyApplied
                        && replay.pointsAdded == 30,
                    "Idempotency must survive restart")) return 1;
    }

    const std::string today = QDate::currentDate().toString(Qt::ISODate).toStdString();
    {
        RewardSystem rewards(nullptr, directory.filePath(QStringLiteral("manual-first.ini")));
        auto service = V1::createRewardService(rewards);
        if (!expect(rewards.checkIn(), "Manual check-in should succeed")) return 1;
        const auto award = service->applyCookingRewardOnce(event(
            "5f778a96-91c7-4e42-a5ae-d7ad189d6103", today + " 18:00:00", false));
        if (!expect(award.pointsAdded == 10 && !award.newCheckIn
                        && rewards.points() == 15,
                    "E03 manual check-in before cooking must award daily points once")) return 1;
    }
    {
        RewardSystem rewards(nullptr, directory.filePath(QStringLiteral("cooking-first.ini")));
        auto service = V1::createRewardService(rewards);
        const auto award = service->applyCookingRewardOnce(event(
            "5f778a96-91c7-4e42-a5ae-d7ad189d6104", today + " 18:00:00", false));
        if (!expect(award.pointsAdded == 15 && award.newCheckIn
                        && !rewards.checkIn() && rewards.points() == 15,
                    "E03 cooking before manual check-in must award daily points once")) return 1;
    }
    {
        QFile blocker(directory.filePath(QStringLiteral("blocking-file")));
        if (!expect(blocker.open(QIODevice::WriteOnly), "Could not create blocking file")) return 1;
        blocker.close();
        RewardSystem rewards(nullptr, directory.filePath(QStringLiteral("blocking-file/rewards.ini")));
        auto service = V1::createRewardService(rewards);
        const auto award = service->applyCookingRewardOnce(first);
        if (!expect(award.outcome.status == V1::OperationStatus::Failed
                        && award.outcome.errorCode == V1::ErrorCode::StorageFailure
                        && rewards.points() == 0 && rewards.totalCookingCount() == 0,
                    "Failed storage must not update memory")) return 1;
    }
    return 0;
}