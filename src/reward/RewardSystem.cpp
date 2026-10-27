#include "reward/RewardSystem.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {

constexpr int kPointsPerCheckIn = 5;
constexpr auto kCheckInKey = "checkIn/dates";

QString dateKey(const QDate& date)
{
    return date.toString(Qt::ISODate);
}

} // namespace

RewardSystem::RewardSystem(QObject* parent, QString storageFile)
    : QObject(parent)
    , m_storageFile(std::move(storageFile))
{
    if (m_storageFile.isEmpty()) {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        m_storageFile = dataDirectory + QStringLiteral("/rewards.ini");
    }

    QSettings settings(m_storageFile, QSettings::IniFormat);
    const QStringList savedDates = settings.value(QLatin1String(kCheckInKey)).toStringList();
    for (const QString& value : savedDates) {
        const QDate date = QDate::fromString(value, Qt::ISODate);
        if (date.isValid() && dateKey(date) == value) {
            m_dates.insert(value);
        }
    }
}

bool RewardSystem::checkIn()
{
    const QString today = dateKey(QDate::currentDate());
    if (m_dates.contains(today)) {
        return false;
    }

    m_dates.insert(today);
    QStringList dates = m_dates.values();
    dates.sort();

    QSettings settings(m_storageFile, QSettings::IniFormat);
    settings.setValue(QLatin1String(kCheckInKey), dates);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_dates.remove(today);
        return false;
    }

    emit changed();
    return true;
}

bool RewardSystem::hasCheckedIn(const QDate& date) const
{
    return date.isValid() && m_dates.contains(dateKey(date));
}

int RewardSystem::totalCheckIns() const
{
    return m_dates.size();
}

int RewardSystem::currentStreak() const
{
    QDate date = QDate::currentDate();
    if (!hasCheckedIn(date)) {
        date = date.addDays(-1);
    }

    int count = 0;
    while (hasCheckedIn(date)) {
        ++count;
        date = date.addDays(-1);
    }
    return count;
}

int RewardSystem::bestStreak() const
{
    const QVector<QDate> dates = sortedDates();
    int best = 0;
    int current = 0;
    QDate previous;
    for (const QDate& date : dates) {
        current = previous.isValid() && previous.addDays(1) == date ? current + 1 : 1;
        best = std::max(best, current);
        previous = date;
    }
    return best;
}

int RewardSystem::points() const
{
    return totalCheckIns() * kPointsPerCheckIn;
}

int RewardSystem::unlockedAchievementCount() const
{
    return (totalCheckIns() >= 1 ? 1 : 0)
        + (bestStreak() >= 3 ? 1 : 0)
        + (bestStreak() >= 7 ? 1 : 0);
}

QVector<QDate> RewardSystem::recentCheckIns(int limit) const
{
    QVector<QDate> dates = sortedDates();
    std::reverse(dates.begin(), dates.end());
    if (limit >= 0 && dates.size() > limit) {
        dates.resize(limit);
    }
    return dates;
}

QVector<QDate> RewardSystem::sortedDates() const
{
    QVector<QDate> dates;
    dates.reserve(m_dates.size());
    for (const QString& value : m_dates) {
        dates.append(QDate::fromString(value, Qt::ISODate));
    }
    std::sort(dates.begin(), dates.end());
    return dates;
}
