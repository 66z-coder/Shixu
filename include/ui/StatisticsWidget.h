#pragma once

#include <QVector>
#include <QWidget>

class QLabel;
class RewardSystem;

class StatisticsWidget : public QWidget
{
public:
    explicit StatisticsWidget(RewardSystem* rewards, QWidget* parent = nullptr);

private:
    void refresh();

    RewardSystem* m_rewards;
    QLabel* m_total = nullptr;
    QLabel* m_cooking = nullptr;
    QLabel* m_streak = nullptr;
    QLabel* m_points = nullptr;
    QLabel* m_achievements = nullptr;
    QVector<QLabel*> m_recentDays;
};
