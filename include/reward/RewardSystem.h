#pragma once

#include <QDate>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class RewardSystem : public QObject
{
    Q_OBJECT

public:
    explicit RewardSystem(QObject* parent = nullptr, QString storageFile = {});

    bool checkIn();
    bool hasCheckedIn(const QDate& date) const;
    int totalCheckIns() const;
    int currentStreak() const;
    int bestStreak() const;
    int points() const;
    int unlockedAchievementCount() const;
    QVector<QDate> recentCheckIns(int limit) const;

signals:
    void changed();

private:
    QVector<QDate> sortedDates() const;

    QString m_storageFile;
    QSet<QString> m_dates;
};
