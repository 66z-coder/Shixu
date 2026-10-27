#include "reward/RewardSystem.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>

#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void saveDates(const QString& path, const QStringList& dates)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("checkIn/dates"), dates);
    settings.sync();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/reward-test-XXXXXX"));
    if (!expect(directory.isValid(), "Temporary directory unavailable")) {
        return 1;
    }

    const QString freshPath = directory.filePath(QStringLiteral("fresh.ini"));
    {
        RewardSystem rewards(nullptr, freshPath);
        if (!expect(rewards.totalCheckIns() == 0, "Fresh count should be zero")
            || !expect(rewards.checkIn(), "First check-in should succeed")
            || !expect(!rewards.checkIn(), "Duplicate check-in should fail")
            || !expect(rewards.totalCheckIns() == 1, "Duplicate changed total")
            || !expect(rewards.points() == 5, "First check-in should earn five points")) {
            return 1;
        }
    }
    {
        RewardSystem restored(nullptr, freshPath);
        if (!expect(restored.hasCheckedIn(QDate::currentDate()), "Check-in was not saved")
            || !expect(restored.points() == 5, "Points were not restored")) {
            return 1;
        }
    }

    const QDate today = QDate::currentDate();
    const QString streakPath = directory.filePath(QStringLiteral("streak.ini"));
    saveDates(streakPath, {
        today.addDays(-3).toString(Qt::ISODate),
        today.addDays(-2).toString(Qt::ISODate),
        today.addDays(-1).toString(Qt::ISODate),
    });
    RewardSystem streak(nullptr, streakPath);
    if (!expect(streak.currentStreak() == 3, "Yesterday's streak should remain active")
        || !expect(streak.unlockedAchievementCount() == 2, "Three-day achievement missing")
        || !expect(streak.checkIn(), "Today's check-in should succeed")
        || !expect(streak.currentStreak() == 4, "Today should extend the streak")
        || !expect(streak.points() == 20, "Four check-ins should earn twenty points")) {
        return 1;
    }

    QStringList oldStreak;
    for (int offset = -20; offset <= -14; ++offset) {
        oldStreak.append(today.addDays(offset).toString(Qt::ISODate));
    }
    const QString oldPath = directory.filePath(QStringLiteral("old.ini"));
    saveDates(oldPath, oldStreak);
    RewardSystem old(nullptr, oldPath);
    if (!expect(old.currentStreak() == 0, "Old streak should not be current")
        || !expect(old.bestStreak() == 7, "Best streak should remain seven")
        || !expect(old.unlockedAchievementCount() == 3, "Earned achievements should remain unlocked")) {
        return 1;
    }

    return 0;
}
