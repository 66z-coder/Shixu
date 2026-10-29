#pragma once

#include <QVector>
#include <QWidget>
#include "contracts/ActivityContracts.h"

class QLabel;
class RewardSystem;

class StatisticsWidget : public QWidget
{
public:
    explicit StatisticsWidget(RewardSystem* rewards, QWidget* parent = nullptr,
        Shixu::Contracts::V1::ICookingActivityService* activity = nullptr);
    void refresh();

private:
    Shixu::Contracts::V1::ICookingActivityService* m_activity = nullptr;
    QLabel* m_error = nullptr;
    QLabel* m_expiring = nullptr;
    QLabel* m_cookingStreak = nullptr;
    QLabel* m_recentMeals = nullptr;
    QLabel* m_legacyMeals = nullptr;
    QLabel* m_cookingAchievements = nullptr;

    RewardSystem* m_rewards;
    QLabel* m_total = nullptr;
    QLabel* m_cooking = nullptr;
    QLabel* m_streak = nullptr;
    QLabel* m_points = nullptr;
    QLabel* m_achievements = nullptr;
    QVector<QLabel*> m_recentDays;
};
