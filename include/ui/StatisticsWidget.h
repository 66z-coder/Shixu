#pragma once
#include "contracts/ActivityContracts.h"
#include "core/NutritionGoals.h"
#include <QVector>
#include <QWidget>
class QLabel;
class QPushButton;
class QGridLayout;
class QVBoxLayout;
class QProgressBar;
class RewardSystem;
class StatisticsWidget : public QWidget {
  public:
    explicit StatisticsWidget(RewardSystem *, QWidget *parent = nullptr,
                              Shixu::Contracts::V1::ICookingActivityService *activity = nullptr);
    void refresh();

  protected:
    void resizeEvent(QResizeEvent *) override;

  private:
    void arrangeBadges();
    void editNutritionGoals();
    RewardSystem *m_rewards;
    Shixu::Contracts::V1::ICookingActivityService *m_activity;
    QLabel *m_error, *m_status, *m_points, *m_cooking, *m_expiring, *m_total, *m_streak, *m_cookingStreak,
        *m_achievements, *m_recentMeals, *m_nutrition, *m_cookingAchievements,
        *m_todayDate, *m_todayCount, *m_todayCalories, *m_mealHeading;
    QVector<QLabel *> m_goalValues;
    QVector<QProgressBar *> m_goalBars;
    QLabel *m_goalStatus = nullptr;
    QPushButton *m_goalButton = nullptr;
    NutritionGoals m_goals{};
    NutritionGoalsStore m_goalsStore;
    bool m_goalsReadable = true;
    QVBoxLayout *m_mealRows = nullptr;
    QPushButton *m_checkIn, *m_details;
    QGridLayout *m_badgeLayout;
    QVector<QWidget *> m_badges;
    QVector<QLabel *> m_recentDays;
};
