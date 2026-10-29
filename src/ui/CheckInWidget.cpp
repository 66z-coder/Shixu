#include "core/AppClock.h"
#include "ui/CheckInWidget.h"

#include "reward/RewardSystem.h"

#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QFrame* makeCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("rewardCard"));
    return card;
}

QLabel* makeMetric(const QString& label, QVBoxLayout* layout, QWidget* parent)
{
    auto* caption = new QLabel(label, parent);
    caption->setObjectName(QStringLiteral("metricCaption"));
    auto* value = new QLabel(parent);
    value->setObjectName(QStringLiteral("metricValue"));
    layout->addWidget(caption);
    layout->addWidget(value);
    return value;
}

} // namespace

CheckInWidget::CheckInWidget(RewardSystem* rewards, QWidget* parent)
    : QWidget(parent)
    , m_rewards(rewards)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* body = new QWidget(scroll);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    scroll->setWidget(body);

    auto* todayCard = makeCard(body);
    auto* todayLayout = new QVBoxLayout(todayCard);
    todayLayout->setContentsMargins(24, 20, 24, 20);
    todayLayout->setSpacing(10);
    auto* todayHeading = new QLabel(QStringLiteral("今日打卡"), todayCard);
    todayHeading->setObjectName(QStringLiteral("sectionTitle"));
    m_status = new QLabel(todayCard);
    m_checkInButton = new QPushButton(QStringLiteral("立即打卡 · +5 积分"), todayCard);
    m_checkInButton->setObjectName(QStringLiteral("checkInButton"));
    m_checkInButton->setMinimumHeight(42);
    todayLayout->addWidget(todayHeading);
    todayLayout->addWidget(m_status);
    todayLayout->addWidget(m_checkInButton, 0, Qt::AlignLeft);
    layout->addWidget(todayCard);

    auto* metricsCard = makeCard(body);
    auto* metricsLayout = new QHBoxLayout(metricsCard);
    metricsLayout->setContentsMargins(24, 20, 24, 20);
    metricsLayout->setSpacing(32);
    auto addMetric = [metricsCard, metricsLayout](const QString& label) {
        auto* column = new QVBoxLayout;
        metricsLayout->addLayout(column, 1);
        return makeMetric(label, column, metricsCard);
    };
    m_streak = addMetric(QStringLiteral("连续打卡"));
    m_total = addMetric(QStringLiteral("累计打卡"));
    m_points = addMetric(QStringLiteral("当前积分"));
    layout->addWidget(metricsCard);

    auto* weekCard = makeCard(body);
    auto* weekLayout = new QVBoxLayout(weekCard);
    weekLayout->setContentsMargins(24, 20, 24, 20);
    weekLayout->setSpacing(14);
    auto* weekHeading = new QLabel(QStringLiteral("本周打卡"), weekCard);
    weekHeading->setObjectName(QStringLiteral("sectionTitle"));
    weekLayout->addWidget(weekHeading);
    auto* daysLayout = new QHBoxLayout;
    for (int day = 0; day < 7; ++day) {
        auto* label = new QLabel(weekCard);
        label->setObjectName(QStringLiteral("weekDay"));
        label->setAlignment(Qt::AlignCenter);
        daysLayout->addWidget(label, 1);
        m_weekDays.append(label);
    }
    weekLayout->addLayout(daysLayout);
    layout->addWidget(weekCard);

    auto* historyCard = makeCard(body);
    auto* historyLayout = new QVBoxLayout(historyCard);
    historyLayout->setContentsMargins(24, 20, 24, 20);
    historyLayout->setSpacing(10);
    auto* historyHeading = new QLabel(QStringLiteral("近期记录"), historyCard);
    historyHeading->setObjectName(QStringLiteral("sectionTitle"));
    m_recentRecords = new QLabel(historyCard);
    m_recentRecords->setWordWrap(true);
    historyLayout->addWidget(historyHeading);
    historyLayout->addWidget(m_recentRecords);
    layout->addWidget(historyCard);

    auto* achievementCard = makeCard(body);
    auto* achievementLayout = new QVBoxLayout(achievementCard);
    achievementLayout->setContentsMargins(24, 20, 24, 20);
    achievementLayout->setSpacing(10);
    auto* achievementHeading = new QLabel(QStringLiteral("成就"), achievementCard);
    achievementHeading->setObjectName(QStringLiteral("sectionTitle"));
    achievementLayout->addWidget(achievementHeading);
    for (int i = 0; i < 3; ++i) {
        auto* label = new QLabel(achievementCard);
        m_achievements.append(label);
        achievementLayout->addWidget(label);
    }
    layout->addWidget(achievementCard);
    layout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QFrame#rewardCard { background: #F8FAF7; border: 1px solid #E4EAE2; border-radius: 10px; }
        QLabel#sectionTitle { color: #26352C; font-size: 17px; font-weight: 600; }
        QLabel#metricCaption { color: #77867B; font-size: 13px; }
        QLabel#metricValue { color: #246B45; font-size: 22px; font-weight: 700; }
        QLabel#weekDay { color: #43534A; font-size: 14px; }
        QPushButton#checkInButton {
            background: #2E8051; border: 0; border-radius: 8px;
            color: white; font-weight: 600; padding: 8px 18px;
        }
        QPushButton#checkInButton:hover { background: #246B45; }
        QPushButton#checkInButton:disabled { background: #DDE9DF; color: #55705E; }
    )"));

    connect(m_checkInButton, &QPushButton::clicked, this, [this] {
        if (!m_rewards->checkIn()) {
            QMessageBox::warning(this, QStringLiteral("打卡失败"), QStringLiteral("打卡记录未能保存，请稍后重试。"));
        }
    });
    connect(m_rewards, &RewardSystem::changed, this, [this] { refresh(); });
    auto* timer = new QTimer(this);
    timer->setInterval(60'000);
    connect(timer, &QTimer::timeout, this, [this] { refresh(); });
    timer->start();
    refresh();
}

void CheckInWidget::refresh()
{
    const QDate today = Shixu::appNow().date();
    const bool checkedIn = m_rewards->hasCheckedIn(today);
    m_status->setText(checkedIn ? QStringLiteral("今天已完成打卡，明天继续保持！")
                                : QStringLiteral("记录今天的饮食生活，从一次打卡开始。"));
    m_checkInButton->setEnabled(!checkedIn);
    m_checkInButton->setText(checkedIn ? QStringLiteral("今日已打卡")
                                    : QStringLiteral("立即打卡 · +5 积分"));
    m_streak->setText(QStringLiteral("%1 天").arg(m_rewards->currentStreak()));
    m_total->setText(QStringLiteral("%1 天").arg(m_rewards->totalCheckIns()));
    m_points->setText(QString::number(m_rewards->points()));

    const QStringList dayNames{QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
                               QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
                               QStringLiteral("日")};
    const QDate monday = today.addDays(1 - today.dayOfWeek());
    for (int day = 0; day < 7; ++day) {
        const QDate date = monday.addDays(day);
        const QString mark = m_rewards->hasCheckedIn(date) ? QStringLiteral("✓") : QStringLiteral("○");
        m_weekDays.at(day)->setText(dayNames.at(day) + QStringLiteral("\n") + mark);
    }

    QStringList records;
    for (const QDate& date : m_rewards->recentCheckIns(5)) {
        records.append(date.toString(QStringLiteral("yyyy/MM/dd")) + QStringLiteral("  每日打卡  +5 积分"));
    }
    m_recentRecords->setText(records.isEmpty() ? QStringLiteral("暂无打卡记录")
                                               : records.join(QStringLiteral("\n")));

    const int best = m_rewards->bestStreak();
    m_achievements.at(0)->setText(m_rewards->totalCheckIns() >= 1
                                     ? QStringLiteral("✓ 初次打卡") : QStringLiteral("○ 初次打卡"));
    m_achievements.at(1)->setText(best >= 3
                                     ? QStringLiteral("✓ 连续打卡 3 天") : QStringLiteral("○ 连续打卡 3 天"));
    m_achievements.at(2)->setText(best >= 7
                                     ? QStringLiteral("✓ 连续打卡 7 天") : QStringLiteral("○ 连续打卡 7 天"));
}
