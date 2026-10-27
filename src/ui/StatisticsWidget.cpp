#include "ui/StatisticsWidget.h"

#include "reward/RewardSystem.h"

#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel* addMetric(QGridLayout* grid, int row, int column, const QString& caption, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("statCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    auto* title = new QLabel(caption, card);
    title->setObjectName(QStringLiteral("statCaption"));
    auto* value = new QLabel(card);
    value->setObjectName(QStringLiteral("statValue"));
    layout->addWidget(title);
    layout->addWidget(value);
    grid->addWidget(card, row, column);
    return value;
}

} // namespace

StatisticsWidget::StatisticsWidget(RewardSystem* rewards, QWidget* parent)
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

    auto* summary = new QGridLayout;
    summary->setSpacing(12);
    m_total = addMetric(summary, 0, 0, QStringLiteral("累计打卡"), body);
    m_streak = addMetric(summary, 0, 1, QStringLiteral("当前连续打卡"), body);
    m_points = addMetric(summary, 1, 0, QStringLiteral("当前积分"), body);
    m_achievements = addMetric(summary, 1, 1, QStringLiteral("已获得成就"), body);
    layout->addLayout(summary);

    auto* recentCard = new QFrame(body);
    recentCard->setObjectName(QStringLiteral("statCard"));
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(20, 18, 20, 18);
    recentLayout->setSpacing(14);
    auto* recentTitle = new QLabel(QStringLiteral("近七天打卡"), recentCard);
    recentTitle->setObjectName(QStringLiteral("statSectionTitle"));
    recentLayout->addWidget(recentTitle);
    auto* days = new QHBoxLayout;
    for (int i = 0; i < 7; ++i) {
        auto* label = new QLabel(recentCard);
        label->setObjectName(QStringLiteral("statDay"));
        label->setAlignment(Qt::AlignCenter);
        days->addWidget(label, 1);
        m_recentDays.append(label);
    }
    recentLayout->addLayout(days);
    layout->addWidget(recentCard);

    auto* cookingCard = new QFrame(body);
    cookingCard->setObjectName(QStringLiteral("statCard"));
    auto* cookingLayout = new QVBoxLayout(cookingCard);
    cookingLayout->setContentsMargins(20, 18, 20, 18);
    auto* cookingTitle = new QLabel(QStringLiteral("做饭次数"), cookingCard);
    cookingTitle->setObjectName(QStringLiteral("statSectionTitle"));
    auto* cookingNote = new QLabel(QStringLiteral("接入菜谱模块后显示做饭记录"), cookingCard);
    cookingLayout->addWidget(cookingTitle);
    cookingLayout->addWidget(cookingNote);
    layout->addWidget(cookingCard);
    layout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QFrame#statCard { background: #F8FAF7; border: 1px solid #E4EAE2; border-radius: 10px; }
        QLabel#statCaption { color: #77867B; font-size: 13px; }
        QLabel#statValue { color: #246B45; font-size: 22px; font-weight: 700; }
        QLabel#statSectionTitle { color: #26352C; font-size: 17px; font-weight: 600; }
        QLabel#statDay { color: #43534A; font-size: 14px; }
    )"));

    connect(m_rewards, &RewardSystem::changed, this, [this] { refresh(); });
    auto* timer = new QTimer(this);
    timer->setInterval(60'000);
    connect(timer, &QTimer::timeout, this, [this] { refresh(); });
    timer->start();
    refresh();
}

void StatisticsWidget::refresh()
{
    m_total->setText(QStringLiteral("%1 天").arg(m_rewards->totalCheckIns()));
    m_streak->setText(QStringLiteral("%1 天").arg(m_rewards->currentStreak()));
    m_points->setText(QString::number(m_rewards->points()));
    m_achievements->setText(QString::number(m_rewards->unlockedAchievementCount()));

    const QDate today = QDate::currentDate();
    for (int i = 0; i < 7; ++i) {
        const QDate date = today.addDays(i - 6);
        m_recentDays.at(i)->setText(date.toString(QStringLiteral("MM/dd"))
                                    + QStringLiteral("\n")
                                    + (m_rewards->hasCheckedIn(date) ? QStringLiteral("✓") : QStringLiteral("○")));
    }
}
