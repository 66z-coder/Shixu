#pragma once

#include <QDate>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "contracts/CoreContracts.h"

class RewardSystem : public QObject
{
    Q_OBJECT

public:
    explicit RewardSystem(QObject* parent = nullptr, QString storageFile = {});

    bool checkIn();
    bool hasCheckedIn(const QDate& date) const;
    int totalCheckIns() const;
    int totalCookingCount() const;
    int currentStreak() const;
    int bestStreak() const;
    int points() const;
    int unlockedAchievementCount() const;
    QVector<QDate> recentCheckIns(int limit) const;

    Shixu::Contracts::V1::CookingRewardResult applyCookingRewardOnce(
        const Shixu::Contracts::V1::CookingRewardRequest& request);
    Shixu::Contracts::V1::QueryResult<Shixu::Contracts::V1::GrowthSnapshot>
    getGrowthSnapshot(const Shixu::Contracts::V1::LocalDate& referenceDate) const;

signals:
    void changed();

private:
    struct SavedEvent {
        Shixu::Contracts::V1::CookingRewardRequest request;
        Shixu::Contracts::V1::CookingRewardResult result;
    };

    bool persist(const QSet<QString>& dates, const QHash<QString, SavedEvent>& events) const;
    QVector<QDate> sortedDates() const;

    QString m_storageFile;
    QSet<QString> m_dates;
    QHash<QString, SavedEvent> m_events;
    bool m_loadError = false;
    Shixu::Contracts::V1::ErrorCode m_loadErrorCode = Shixu::Contracts::V1::ErrorCode::InvalidData;
};