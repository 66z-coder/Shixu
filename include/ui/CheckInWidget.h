#pragma once

#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class RewardSystem;

class CheckInWidget : public QWidget
{
public:
    explicit CheckInWidget(RewardSystem* rewards, QWidget* parent = nullptr);

private:
    void refresh();

    RewardSystem* m_rewards;
    QLabel* m_status = nullptr;
    QLabel* m_streak = nullptr;
    QLabel* m_points = nullptr;
    QLabel* m_total = nullptr;
    QLabel* m_recentRecords = nullptr;
    QPushButton* m_checkInButton = nullptr;
    QVector<QLabel*> m_weekDays;
    QVector<QLabel*> m_achievements;
};
