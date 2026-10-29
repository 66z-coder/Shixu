#include "core/AppClock.h"
#include "reward/RewardSystem.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {
namespace V1 = Shixu::Contracts::V1;
constexpr int kPointsPerCheckIn = 5;
constexpr int kPointsPerCooking = 10;
constexpr int kPointsPerExpiringCooking = 15;
constexpr auto kCheckInKey = "checkIn/dates";
constexpr auto kStateKey = "rewardV1/state";

QString dateKey(const QDate& date) { return date.toString(Qt::ISODate); }

bool validDate(const QString& value)
{
    const QDate date = QDate::fromString(value, Qt::ISODate);
    return date.isValid() && dateKey(date) == value;
}

bool validDateTime(const QString& value)
{
    const QDateTime dateTime = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dateTime.isValid()
        && dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) == value;
}

bool validEventId(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return pattern.match(value).hasMatch();
}

QVector<QDate> sorted(const QSet<QString>& dates)
{
    QVector<QDate> result;
    result.reserve(dates.size());
    for (const QString& value : dates) result.append(QDate::fromString(value, Qt::ISODate));
    std::sort(result.begin(), result.end());
    return result;
}

int streakAt(const QSet<QString>& dates, QDate reference)
{
    if (!dates.contains(dateKey(reference))) reference = reference.addDays(-1);
    int count = 0;
    while (dates.contains(dateKey(reference))) {
        ++count;
        reference = reference.addDays(-1);
    }
    return count;
}

int bestStreakFor(const QSet<QString>& dates)
{
    int best = 0;
    int current = 0;
    QDate previous;
    for (const QDate& date : sorted(dates)) {
        current = previous.isValid() && previous.addDays(1) == date ? current + 1 : 1;
        best = std::max(best, current);
        previous = date;
    }
    return best;
}

std::vector<std::string> achievementCodes(const QSet<QString>& dates)
{
    std::vector<std::string> codes;
    if (!dates.isEmpty()) codes.emplace_back("first_checkin");
    const int best = bestStreakFor(dates);
    if (best >= 3) codes.emplace_back("streak_3");
    if (best >= 7) codes.emplace_back("streak_7");
    return codes;
}

template<class Events>
int totalPoints(const QSet<QString>& dates, const Events& events)
{
    int points = dates.size() * kPointsPerCheckIn;
    for (const auto& event : events)
        points += kPointsPerCooking + (event.request.consumedExpiring ? kPointsPerExpiringCooking : 0);
    return points;
}
} // namespace

RewardSystem::RewardSystem(QObject* parent, QString storageFile)
    : QObject(parent), m_storageFile(std::move(storageFile))
{
    if (m_storageFile.isEmpty()) {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        m_storageFile = dataDirectory + QStringLiteral("/rewards.ini");
    }

    QSettings settings(m_storageFile, QSettings::IniFormat);
    if (!settings.contains(QLatin1String(kStateKey))) {
        const QStringList savedDates = settings.value(QLatin1String(kCheckInKey)).toStringList();
        for (const QString& value : savedDates) {
            if (validDate(value)) m_dates.insert(value);
            else m_loadError = true;
        }
        if (settings.status() != QSettings::NoError) {
            m_loadError = true;
            if (settings.status() == QSettings::AccessError) m_loadErrorCode = V1::ErrorCode::StorageFailure;
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        settings.value(QLatin1String(kStateKey)).toByteArray(), &parseError);
    if (settings.status() != QSettings::NoError || parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        m_loadError = true;
        if (settings.status() == QSettings::AccessError) m_loadErrorCode = V1::ErrorCode::StorageFailure;
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1
        || !root.value(QStringLiteral("dates")).isArray()
        || !root.value(QStringLiteral("events")).isArray()) {
        m_loadError = true;
        return;
    }

    QSet<QString> dates;
    QHash<QString, SavedEvent> events;
    for (const QJsonValue& value : root.value(QStringLiteral("dates")).toArray()) {
        if (!value.isString() || !validDate(value.toString())) { m_loadError = true; return; }
        dates.insert(value.toString());
    }
    for (const QJsonValue& value : root.value(QStringLiteral("events")).toArray()) {
        if (!value.isObject()) { m_loadError = true; return; }
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("eventId")).toString();
        const QString completedAt = object.value(QStringLiteral("completedAt")).toString();
        if (!validEventId(id) || !validDateTime(completedAt) || events.contains(id)
            || !object.value(QStringLiteral("consumedExpiring")).isBool()
            || !object.value(QStringLiteral("pointsAdded")).isDouble()
            || !object.value(QStringLiteral("newCheckIn")).isBool()
            || !object.value(QStringLiteral("totalCookingCount")).isDouble()
            || !object.value(QStringLiteral("totalPoints")).isDouble()
            || !object.value(QStringLiteral("achievements")).isArray()) {
            m_loadError = true;
            return;
        }
        SavedEvent event;
        event.request.eventId = id.toStdString();
        event.request.completedAt = completedAt.toStdString();
        event.request.consumedExpiring = object.value(QStringLiteral("consumedExpiring")).toBool();
        event.result.outcome = {V1::OperationStatus::Succeeded, V1::ErrorCode::None, {}};
        event.result.eventId = event.request.eventId;
        event.result.pointsAdded = object.value(QStringLiteral("pointsAdded")).toInt();
        event.result.newCheckIn = object.value(QStringLiteral("newCheckIn")).toBool();
        event.result.totalCookingCount = object.value(QStringLiteral("totalCookingCount")).toInt();
        event.result.totalPoints = object.value(QStringLiteral("totalPoints")).toInt();
        for (const QJsonValue& code : object.value(QStringLiteral("achievements")).toArray()) {
            if (!code.isString()) { m_loadError = true; return; }
            event.result.newlyUnlockedAchievements.push_back(code.toString().toStdString());
        }
        if (!dates.contains(completedAt.left(10))) { m_loadError = true; return; }
        events.insert(id, std::move(event));
    }
    m_dates = std::move(dates);
    m_events = std::move(events);
}

bool RewardSystem::persist(const QSet<QString>& dates,
                           const QHash<QString, SavedEvent>& events) const
{
    QStringList dateList = dates.values();
    dateList.sort();
    QJsonArray dateArray;
    for (const QString& date : dateList) dateArray.append(date);
    QStringList eventIds = events.keys();
    eventIds.sort();
    QJsonArray eventArray;
    for (const QString& id : eventIds) {
        const SavedEvent& event = events.value(id);
        QJsonArray achievements;
        for (const std::string& code : event.result.newlyUnlockedAchievements)
            achievements.append(QString::fromStdString(code));
        QJsonObject object;
        object.insert(QStringLiteral("eventId"), id);
        object.insert(QStringLiteral("completedAt"), QString::fromStdString(event.request.completedAt));
        object.insert(QStringLiteral("consumedExpiring"), event.request.consumedExpiring);
        object.insert(QStringLiteral("pointsAdded"), event.result.pointsAdded);
        object.insert(QStringLiteral("newCheckIn"), event.result.newCheckIn);
        object.insert(QStringLiteral("totalCookingCount"), event.result.totalCookingCount);
        object.insert(QStringLiteral("totalPoints"), event.result.totalPoints);
        object.insert(QStringLiteral("achievements"), achievements);
        eventArray.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("dates"), dateArray);
    root.insert(QStringLiteral("events"), eventArray);
    QSettings settings(m_storageFile, QSettings::IniFormat);
    settings.setAtomicSyncRequired(true);
    settings.setValue(QLatin1String(kStateKey), QJsonDocument(root).toJson(QJsonDocument::Compact));
    settings.setValue(QLatin1String(kCheckInKey), dateList);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool RewardSystem::checkIn()
{
    if (m_loadError) return false;
    const QString today = dateKey(Shixu::appNow().date());
    if (m_dates.contains(today)) return false;
    QSet<QString> nextDates = m_dates;
    nextDates.insert(today);
    if (!persist(nextDates, m_events)) return false;
    m_dates = std::move(nextDates);
    emit changed();
    return true;
}

bool RewardSystem::hasCheckedIn(const QDate& date) const
{
    return date.isValid() && m_dates.contains(dateKey(date));
}
int RewardSystem::totalCheckIns() const { return m_dates.size(); }
int RewardSystem::totalCookingCount() const { return m_events.size(); }
int RewardSystem::currentStreak() const { return streakAt(m_dates, Shixu::appNow().date()); }
int RewardSystem::bestStreak() const { return bestStreakFor(m_dates); }
int RewardSystem::points() const { return totalPoints(m_dates, m_events); }
int RewardSystem::unlockedAchievementCount() const
{
    return static_cast<int>(achievementCodes(m_dates).size());
}
QVector<QDate> RewardSystem::recentCheckIns(int limit) const
{
    QVector<QDate> dates = sortedDates();
    std::reverse(dates.begin(), dates.end());
    if (limit >= 0 && dates.size() > limit) dates.resize(limit);
    return dates;
}
QVector<QDate> RewardSystem::sortedDates() const { return sorted(m_dates); }

V1::CookingRewardResult RewardSystem::applyCookingRewardOnce(
    const V1::CookingRewardRequest& request)
{
    V1::CookingRewardResult result;
    result.eventId = request.eventId;
    if (m_loadError) {
        result.outcome = {V1::OperationStatus::Failed, m_loadErrorCode,
                          "Reward state could not be read"};
        return result;
    }
    const QString id = QString::fromStdString(request.eventId);
    const QString completedAt = QString::fromStdString(request.completedAt);
    if (!validEventId(id) || !validDateTime(completedAt)) {
        result.outcome = {V1::OperationStatus::Failed, V1::ErrorCode::InvalidArgument,
                          "Invalid event ID or completion time"};
        return result;
    }
    if (m_events.contains(id)) {
        const SavedEvent& saved = m_events[id];
        if (saved.request.completedAt != request.completedAt
            || saved.request.consumedExpiring != request.consumedExpiring) {
            result.outcome = {V1::OperationStatus::Failed, V1::ErrorCode::EventConflict,
                              "Event ID was used for a different cooking result"};
            return result;
        }
        result = saved.result;
        result.outcome.status = V1::OperationStatus::AlreadyApplied;
        return result;
    }
    QSet<QString> nextDates = m_dates;
    QHash<QString, SavedEvent> nextEvents = m_events;
    const QString date = completedAt.left(10);
    const bool newCheckIn = !nextDates.contains(date);
    const auto beforeAchievements = achievementCodes(m_dates);
    nextDates.insert(date);
    result.outcome = {V1::OperationStatus::Succeeded, V1::ErrorCode::None, {}};
    result.pointsAdded = kPointsPerCooking
        + (request.consumedExpiring ? kPointsPerExpiringCooking : 0)
        + (newCheckIn ? kPointsPerCheckIn : 0);
    result.newCheckIn = newCheckIn;
    result.totalCookingCount = nextEvents.size() + 1;
    result.totalPoints = totalPoints(nextDates, nextEvents)
        + kPointsPerCooking + (request.consumedExpiring ? kPointsPerExpiringCooking : 0);
    const auto afterAchievements = achievementCodes(nextDates);
    for (const std::string& code : afterAchievements) {
        if (std::find(beforeAchievements.begin(), beforeAchievements.end(), code)
            == beforeAchievements.end())
            result.newlyUnlockedAchievements.push_back(code);
    }
    nextEvents.insert(id, SavedEvent{request, result});
    if (!persist(nextDates, nextEvents)) {
        result.outcome = {V1::OperationStatus::Failed, V1::ErrorCode::StorageFailure,
                          "Reward state could not be saved"};
        result.pointsAdded = 0;
        return result;
    }
    m_dates = std::move(nextDates);
    m_events = std::move(nextEvents);
    emit changed();
    return result;
}

V1::QueryResult<V1::GrowthSnapshot> RewardSystem::getGrowthSnapshot(
    const V1::LocalDate& referenceDate) const
{
    V1::QueryResult<V1::GrowthSnapshot> query;
    if (m_loadError) {
        query.outcome = {V1::OperationStatus::Failed, m_loadErrorCode,
                         "Reward state could not be read"};
        return query;
    }
    const QString date = QString::fromStdString(referenceDate);
    if (!validDate(date)) {
        query.outcome = {V1::OperationStatus::Failed, V1::ErrorCode::InvalidArgument,
                         "Invalid reference date"};
        return query;
    }
    query.outcome = {V1::OperationStatus::Succeeded, V1::ErrorCode::None, {}};
    query.value.totalCookingCount = m_events.size();
    query.value.totalCheckIns = m_dates.size();
    query.value.totalPoints = points();
    query.value.currentStreak = streakAt(m_dates, QDate::fromString(date, Qt::ISODate));
    query.value.bestStreak = bestStreak();
    query.value.checkedInOnReferenceDate = m_dates.contains(date);
    query.value.unlockedAchievements = achievementCodes(m_dates);
    const QVector<QDate> dates = recentCheckIns(7);
    for (const QDate& checked : dates)
        query.value.recentCheckInDates.push_back(dateKey(checked).toStdString());
    return query;
}