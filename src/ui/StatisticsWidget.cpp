#include "ui/StatisticsWidget.h"
#include "core/AppClock.h"
#include <algorithm>

#include "reward/RewardSystem.h"

#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *addMetric(QGridLayout *grid, int row, int column, const QString &caption, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("statCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    auto *title = new QLabel(caption, card);
    title->setObjectName(QStringLiteral("statCaption"));
    auto *value = new QLabel(card);
    value->setObjectName(QStringLiteral("statValue"));
    value->setProperty("statMetric", true);
    layout->addWidget(title);
    layout->addWidget(value);
    grid->addWidget(card, row, column);
    return value;
}

} // namespace

StatisticsWidget::StatisticsWidget(RewardSystem *rewards, QWidget *parent,
                                   Shixu::Contracts::V1::ICookingActivityService *activity)
    : QWidget(parent), m_rewards(rewards) {
    m_activity = activity;
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *body = new QWidget(scroll);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    scroll->setWidget(body);
    m_error = new QLabel(body);
    m_error->setObjectName("growthError");
    m_error->setWordWrap(true);
    m_error->setTextFormat(Qt::PlainText);
    layout->addWidget(m_error);

    auto *summary = new QGridLayout;
    summary->setSpacing(12);
    m_total = addMetric(summary, 0, 0, QStringLiteral("累计打卡"), body);
    m_cooking = addMetric(summary, 2, 0, QStringLiteral("累计做饭"), body);
    m_streak = addMetric(summary, 0, 1, QStringLiteral("当前连续打卡"), body);
    m_points = addMetric(summary, 1, 0, QStringLiteral("当前积分"), body);
    m_achievements = addMetric(summary, 1, 1, QStringLiteral("已获得成就"), body);
    m_expiring = addMetric(summary, 2, 1, QStringLiteral("临期利用次数"), body);
    m_cookingStreak = addMetric(summary, 3, 0, QStringLiteral("当前连续做饭"), body);
    m_cooking->setObjectName("verifiedCookingCount");
    m_expiring->setObjectName("expiringCookingCount");
    m_points->setObjectName("growthPoints");
    m_total->setObjectName("totalCheckIns");
    m_achievements->setObjectName("achievementCount");
    layout->addLayout(summary);

    auto *recentCard = new QFrame(body);
    recentCard->setObjectName(QStringLiteral("statCard"));
    auto *recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(20, 18, 20, 18);
    recentLayout->setSpacing(14);
    auto *recentTitle = new QLabel(QStringLiteral("近七天打卡"), recentCard);
    recentTitle->setObjectName(QStringLiteral("statSectionTitle"));
    recentLayout->addWidget(recentTitle);
    auto *days = new QHBoxLayout;
    for (int i = 0; i < 7; ++i) {
        auto *label = new QLabel(recentCard);
        label->setObjectName(QStringLiteral("statDay"));
        label->setAlignment(Qt::AlignCenter);
        days->addWidget(label, 1);
        m_recentDays.append(label);
    }
    recentLayout->addLayout(days);
    layout->addWidget(recentCard);

    auto section = [&](const QString &title, const char *name) {
        auto *card = new QFrame(body);
        card->setObjectName("statCard");
        auto *box = new QVBoxLayout(card);
        box->setContentsMargins(20, 18, 20, 18);
        auto *heading = new QLabel(title, card);
        heading->setObjectName("statSectionTitle");
        box->addWidget(heading);
        auto *content = new QLabel(card);
        content->setObjectName(name);
        content->setTextFormat(Qt::PlainText);
        content->setWordWrap(true);
        content->setTextInteractionFlags(Qt::TextSelectableByMouse);
        box->addWidget(content);
        layout->addWidget(card);
        return content;
    };
    m_cookingAchievements = section(QStringLiteral("烹饪成就"), "cookingAchievements");
    m_recentMeals = section(QStringLiteral("最近做饭记录 · 已全部结算 · 最近10条"), "recentCookingMeals");
    m_legacyMeals =
        section(QStringLiteral("旧饮食记录 · 仅展示，不计入统计与成就 · 最近10条"), "legacyCookingMeals");

    layout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QFrame#statCard { background: #F8FAF7; border: 1px solid #E4EAE2; border-radius: 10px; }
        QLabel#statCaption { color: #77867B; font-size: 13px; }
        QLabel[statMetric="true"] { color: #246B45; font-size: 22px; font-weight: 700; }
        QLabel#statSectionTitle { color: #26352C; font-size: 17px; font-weight: 600; }
        QLabel#statDay { color: #43534A; font-size: 14px; }
    )"));

    connect(m_rewards, &RewardSystem::changed, this, [this] { refresh(); });
    auto *timer = new QTimer(this);
    timer->setInterval(60'000);
    connect(timer, &QTimer::timeout, this, [this] { refresh(); });
    timer->start();
    refresh();
}

void StatisticsWidget::refresh() {
    const auto growth =
        m_rewards->getGrowthSnapshot(Shixu::appNow().date().toString(Qt::ISODate).toStdString());
    if (growth.outcome.status != Shixu::Contracts::V1::OperationStatus::Succeeded) {
        m_error->setText(QString::fromUtf8(growth.outcome.message.c_str()));
        for (auto *label : {m_total, m_cooking, m_streak, m_points, m_achievements, m_expiring,
                            m_cookingStreak, m_recentMeals, m_legacyMeals, m_cookingAchievements})
            label->setText(QStringLiteral("暂不可用"));
        for (auto *label : m_recentDays)
            label->setText(QStringLiteral("—"));
        return;
    }
    m_error->clear();
    m_total->setText(QStringLiteral("%1 天").arg(growth.value.totalCheckIns));
    m_streak->setText(QStringLiteral("%1 天").arg(growth.value.currentStreak));
    m_points->setText(QString::number(growth.value.totalPoints));
    m_achievements->setText(QString::number(growth.value.unlockedAchievements.size()));

    const QDate today = Shixu::appNow().date();
    for (int i = 0; i < 7; ++i) {
        const QDate date = today.addDays(i - 6);
        m_recentDays.at(i)->setText(
            date.toString(QStringLiteral("MM/dd")) + QStringLiteral("\n") +
            (m_rewards->hasCheckedIn(date) ? QStringLiteral("✓") : QStringLiteral("○")));
    }
    if (!m_activity) {
        m_error->setText(QStringLiteral("做饭统计服务尚未连接。"));
        for (auto *label :
             {m_cooking, m_expiring, m_cookingStreak, m_recentMeals, m_legacyMeals, m_cookingAchievements})
            label->setText(QStringLiteral("暂不可用"));
        return;
    }
    const auto activity = m_activity->refresh(today.toString(Qt::ISODate).toStdString());
    if (activity.outcome.status != Shixu::Contracts::V1::OperationStatus::Succeeded) {
        m_error->setText(QString::fromUtf8(activity.outcome.message.c_str()));
        for (auto *label : {m_cooking, m_expiring, m_cookingStreak, m_recentMeals, m_legacyMeals,
                            m_cookingAchievements, m_achievements})
            label->setText(QStringLiteral("暂不可用"));
        return;
    }
    m_cooking->setText(QStringLiteral("%1 次").arg(activity.value.totalCookingCount));
    m_expiring->setText(QStringLiteral("%1 次").arg(activity.value.expiringCookingCount));
    m_cookingStreak->setText(QStringLiteral("%1 天").arg(activity.value.currentCookingStreak));
    m_achievements->setText(
        QString::number(growth.value.unlockedAchievements.size() + activity.value.achievements.size()));
    auto history = [](const std::vector<MealRecord> &meals) {
        QStringList rows;
        for (size_t i = 0; i < std::min<size_t>(10, meals.size()); ++i) {
            const auto &m = meals[i];
            rows << QStringLiteral("%1 · %2\n估计营养：%3 千卡 · 蛋白质 %4 克 · 碳水 %5 克 · 脂肪 %6 克")
                        .arg(QString::fromUtf8(m.recipeName.c_str()), QString::fromStdString(m.time))
                        .arg(m.nutrition.calories, 0, 'f', 0)
                        .arg(m.nutrition.protein, 0, 'f', 1)
                        .arg(m.nutrition.carbohydrates, 0, 'f', 1)
                        .arg(m.nutrition.fat, 0, 'f', 1);
        }
        return rows.empty() ? QStringLiteral("暂无记录") : rows.join("\n\n");
    };
    m_recentMeals->setText(history(activity.value.recentMeals));
    m_legacyMeals->setText(history(activity.value.legacyMeals));
    const struct {
        const char *code;
        const char *name;
        const char *condition;
    } definitions[] = {{"first_cooking", "厨房新手", "首次完成一道菜"},
                       {"cooking_streak_3", "连续开火", "连续3天完成做饭"},
                       {"expiring_3", "节约达人", "累计3次成功利用临期食材"},
                       {"recipes_5", "厨房探索家", "完成5道不同菜谱"}};
    QStringList badges;
    for (const auto &d : definitions) {
        QString when;
        for (const auto &a : activity.value.achievements)
            if (a.code == d.code)
                when = QString::fromStdString(a.unlockedAt);
        badges << QStringLiteral("%1 · %2\n%3")
                      .arg(QString::fromUtf8(d.name),
                           when.isEmpty() ? QStringLiteral("未解锁") : QStringLiteral("已解锁 · ") + when,
                           QString::fromUtf8(d.condition));
    }
    m_cookingAchievements->setText(badges.join("\n\n"));
}
