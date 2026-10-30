#include "ui/StatisticsWidget.h"
#include "ui/AppTheme.h"
#include "core/AppClock.h"
#include "reward/RewardSystem.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
namespace {
namespace V = Shixu::Contracts::V1;
struct Badge {
    const char *code;
    const char *name;
    const char *condition;
};
const Badge definitions[] = {{"first_cooking", "厨房新手", "首次完成一道菜"},
                             {"cooking_streak_3", "连续开火", "连续 3 天完成做饭"},
                             {"expiring_3", "节约达人", "累计 3 次利用临期食材"},
                             {"recipes_5", "厨房探索家", "完成 5 道不同菜谱"}};
QLabel *label(const QString &text, QWidget *parent, const char *name = "") {
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setObjectName(name);
    return l;
}
QVBoxLayout *card(QVBoxLayout *outer, QWidget *parent, const QString &title) {
    auto *frame = new QFrame(parent);
    frame->setObjectName("growthCard");
    auto *box = new QVBoxLayout(frame);
    box->setContentsMargins(20, 20, 20, 20);
    box->setSpacing(10);
    if (!title.isEmpty())
        box->addWidget(label(title, frame, "sectionHeading"));
    outer->addWidget(frame);
    return box;
}
} // namespace
StatisticsWidget::StatisticsWidget(RewardSystem *rewards, QWidget *parent,
                                   V::ICookingActivityService *activity)
    : QWidget(parent), m_rewards(rewards), m_activity(activity),
      m_goalsStore(QFileInfo(rewards->storageFilePath()).dir().filePath(QStringLiteral("nutrition-goals.json"))) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setProperty("pageScroll", true);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto *body = new QWidget(scroll);
    body->setObjectName("pageBody");
    scroll->setWidget(body);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    m_error = label({}, body, "growthError");
    layout->addWidget(m_error);
    auto *today = card(layout, body, QStringLiteral("把每一餐，变成一点进步"));
    auto *todayRow = new QHBoxLayout;
    m_status = label({}, body, "todayGrowthStatus");
    todayRow->addWidget(m_status, 1);
    m_points = label({}, body, "growthPoints");
    todayRow->addWidget(m_points);
    m_checkIn = new QPushButton(QStringLiteral("今日打卡 · +5 积分"), body);
    m_checkIn->setObjectName("growthCheckIn");
    m_checkIn->setProperty("primary", true);
    todayRow->addWidget(m_checkIn);
    today->addLayout(todayRow);
    connect(m_checkIn, &QPushButton::clicked, this, [this] {
        if (!m_rewards->checkIn()) {
            refresh();
            m_error->setText(QStringLiteral("打卡保存失败，请稍后重试。"));
            m_error->show();
        }
    });
    auto *metrics = new QGridLayout;
    metrics->setSpacing(16);
    auto metric = [&](int row, int col, const QString &caption, const char *name) {
        auto *f = new QFrame(body);
        f->setObjectName("growthCard");
        auto *b = new QVBoxLayout(f);
        b->setContentsMargins(16, 12, 16, 12);
        b->addWidget(label(caption, f, "metricCaption"));
        auto *value = label({}, f, name);
        value->setProperty("metric", true);
        b->addWidget(value);
        metrics->addWidget(f, row, col);
        return value;
    };
    m_cooking = metric(0, 0, QStringLiteral("累计做饭"), "verifiedCookingCount");
    m_expiring = metric(0, 1, QStringLiteral("临期利用"), "expiringCookingCount");
    m_total = metric(1, 0, QStringLiteral("累计打卡"), "totalCheckIns");
    m_streak = metric(1, 1, QStringLiteral("连续打卡"), "checkInStreak");
    m_cookingStreak = label({}, m_cooking->parentWidget(), "cookingStreakHint");
    m_cooking->parentWidget()->layout()->addWidget(m_cookingStreak);
    layout->addLayout(metrics);
    auto *week = card(layout, body, QStringLiteral("近七天"));
    auto *days = new QHBoxLayout;
    for (int i = 0; i < 7; ++i) {
        auto *d = label({}, body, "growthDay");
        d->setAlignment(Qt::AlignCenter);
        days->addWidget(d, 1);
        m_recentDays.append(d);
    }
    week->addLayout(days);
    auto *badges = card(layout, body, QStringLiteral("我的成就"));
    m_achievements = label({}, body, "achievementCount");
    badges->addWidget(m_achievements);
    auto *grid = new QWidget(body);
    grid->setObjectName("achievementGrid");
    m_badgeLayout = new QGridLayout(grid);
    m_badgeLayout->setContentsMargins(0, 0, 0, 0);
    m_badgeLayout->setSpacing(10);
    badges->addWidget(grid);
    for (const auto &d : definitions) {
        auto *f = new QFrame(grid);
        f->setObjectName("achievementCard");
        auto *box = new QVBoxLayout(f);
        box->setContentsMargins(12, 12, 12, 12);
        auto *icon = label({}, f, "badgeIcon");
        icon->setAlignment(Qt::AlignCenter);
        box->addWidget(icon);
        auto *name = label(QString::fromUtf8(d.name), f, "badgeName");
        name->setAlignment(Qt::AlignCenter);
        box->addWidget(name);
        auto *condition = label(QString::fromUtf8(d.condition), f, "badgeCondition");
        condition->setAlignment(Qt::AlignCenter);
        box->addWidget(condition);
        auto *state = label({}, f, "badgeState");
        state->setAlignment(Qt::AlignCenter);
        box->addWidget(state);
        m_badges.append(f);
    }
    m_cookingAchievements = label({}, body, "cookingAchievements");
    m_cookingAchievements->hide();
    auto *nutritionToday = card(layout, body, QString());
    nutritionToday->setContentsMargins(20, 20, 20, 20);
    nutritionToday->setSpacing(16);
    auto *nutritionHeader = new QHBoxLayout;
    nutritionHeader->addWidget(label(QStringLiteral("今日饮食概览"), body, "nutritionHeading"));
    nutritionHeader->addStretch();
    m_todayDate = label({}, body, "nutritionDate");
    nutritionHeader->addWidget(m_todayDate);
    nutritionToday->addLayout(nutritionHeader);
    auto *nutritionMetrics = new QHBoxLayout;
    nutritionMetrics->setSpacing(12);
    auto nutritionMetric = [&](const QString &caption, const char *name) {
        auto *frame = new QFrame(body);
        frame->setObjectName("nutritionMetricCard");
        auto *box = new QVBoxLayout(frame);
        box->setContentsMargins(18, 14, 18, 14);
        box->addWidget(label(caption, frame, "nutritionCaption"));
        auto *value = label({}, frame, name);
        value->setObjectName("nutritionMetricValue");
        value->setProperty("metricName", QString::fromLatin1(name));
        box->addWidget(value);
        nutritionMetrics->addWidget(frame, 1);
        return value;
    };
    m_todayCount = nutritionMetric(QStringLiteral("今日记录"), "todayMealCount");
    m_todayCalories = nutritionMetric(QStringLiteral("累计热量"), "todayCalories");
    m_todayCount->setObjectName("todayMealCount");
    m_todayCalories->setObjectName("todayCalories");
    nutritionToday->addLayout(nutritionMetrics);
    nutritionToday->addWidget(label(QStringLiteral("今日已记录营养 · 每日目标"), body, "nutritionSubheading"));
    for (const auto &name : {QStringLiteral("热量"), QStringLiteral("蛋白质"), QStringLiteral("碳水化合物"), QStringLiteral("脂肪")}) {
        auto *row = new QHBoxLayout;
        row->addWidget(label(name, body, "macroCaption"));
        row->addStretch();
        auto *value = label({}, body, "macroValue");
        value->setWordWrap(false);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setObjectName(QStringLiteral("goalValue%1").arg(m_goalValues.size()));
        value->setProperty("goalValue", true);
        row->addWidget(value);
        m_goalValues.append(value);
        nutritionToday->addLayout(row);
        auto *bar = new QProgressBar(body);
        bar->setRange(0, 1000);
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        bar->setObjectName(QStringLiteral("goalBar%1").arg(m_goalBars.size()));
        m_goalBars.append(bar);
        nutritionToday->addWidget(bar);
    }
    auto *nutritionNote = label(QStringLiteral("根据已记录菜谱估算（含旧版记录），不代表个人实际摄入量。"), body, "nutritionNote");
    nutritionToday->addWidget(nutritionNote);
    m_goalStatus = label({}, body, "nutritionGoalsStatus");
    m_goalStatus->hide();
    nutritionToday->addWidget(m_goalStatus);
    m_goalButton = new QPushButton(QStringLiteral("设置每日目标"), body);
    m_goalButton->setObjectName(QStringLiteral("editNutritionGoals"));
    nutritionToday->addWidget(m_goalButton, 0, Qt::AlignLeft);
    QString goalError;
    if (!m_goalsStore.load(m_goals, goalError)) {
        m_goalsReadable = false;
        m_goalStatus->setText(goalError);
        m_goalStatus->show();
        m_goalButton->setEnabled(false);
    }
    connect(m_goalButton, &QPushButton::clicked, this, [this] { editNutritionGoals(); });
    auto *meals = card(layout, body, QString());
    m_mealHeading = label(QStringLiteral("最近做饭"), body, "recentMealsHeading");
    m_mealHeading->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;color:#28536E;"));
    meals->addWidget(m_mealHeading);
    meals->setContentsMargins(20, 20, 20, 20);
    meals->setSpacing(14);
    m_recentMeals = label({}, body, "recentCookingMeals");
    meals->addWidget(m_recentMeals);
    m_mealRows = new QVBoxLayout;
    m_mealRows->setSpacing(8);
    meals->addLayout(m_mealRows);
    m_details = new QPushButton(QStringLiteral("查看营养详情 ▾"), body);
    m_details->setObjectName(QStringLiteral("nutritionDetailsButton"));
    m_details->setCheckable(true);
    meals->addWidget(m_details, 0, Qt::AlignLeft);
    m_nutrition = label({}, body, "mealNutrition");
    m_nutrition->hide();
    meals->addWidget(m_nutrition);
    connect(m_details, &QPushButton::toggled, this, [this](bool open) {
        m_nutrition->setVisible(open);
        m_details->setText(open ? QStringLiteral("收起营养详情 ▴") : QStringLiteral("查看营养详情 ▾"));
    });
    layout->addStretch();
    setStyleSheet(QStringLiteral(
        R"(QWidget#pageBody {background:#EDF7F9;} QFrame#growthCard {background:white;border:1px solid #DCE8EF;border-radius:14px;} QLabel#sectionHeading {font-size:18px;font-weight:700;color:#28536E;} QLabel[metric="true"] {font-size:28px;font-weight:700;color:#287BA8;} QLabel#metricCaption,QLabel#badgeCondition {color:#698392;} QLabel#growthDay {background:#EFF7FC;border-radius:8px;padding:8px 2px;} QFrame#achievementCard {background:#F2F7FA;border:1px solid #E1EBF1;border-radius:10px;} QLabel#badgeName {font-size:16px;font-weight:600;} QLabel#badgeState {font-size:12px;color:#708794;} QLabel#growthError {color:#B5513D;}  QLabel#mealNutrition {color:#617B8C;font-size:14px;} QLabel#nutritionHeading {font-size:18px;font-weight:700;color:#244E6A;} QLabel#nutritionDate {color:#64839A;background:#EEF6FA;border-radius:10px;padding:5px 10px;} QFrame#nutritionMetricCard {background:#F0F7FB;border:0;border-radius:10px;} QLabel#nutritionCaption {font-size:14px;color:#64839A;background:transparent;} QLabel#todayMealCount,QLabel#todayCalories {font-size:32px;font-weight:700;color:#2479AC;background:transparent;} QLabel#nutritionSubheading {font-size:16px;font-weight:700;color:#315970;} QLabel#macroCaption {font-size:14px;color:#315970;} QLabel[goalValue="true"] {font-size:15px;font-weight:600;color:#244E6A;} QLabel#nutritionGoalsStatus {color:#527992;font-size:13px;} QLabel#nutritionNote {font-size:12px;color:#627E90;} QProgressBar {background:#E8F0F4;border:0;border-radius:4px;} QProgressBar#goalBar0::chunk {background:#4D91BC;border-radius:4px;} QProgressBar#goalBar1::chunk {background:#68A0B8;border-radius:4px;} QProgressBar#goalBar2::chunk {background:#C2A366;border-radius:4px;} QProgressBar#goalBar3::chunk {background:#8D89B9;border-radius:4px;} QFrame#mealRow {background:#F5F9FC;border:0;border-radius:9px;} QLabel#mealName {font-size:16px;font-weight:700;color:#284F69;background:transparent;} QLabel#mealTime {font-size:13px;color:#627E90;background:transparent;} QLabel#mealCalories {font-size:15px;font-weight:600;color:#287BA8;background:transparent;})"));
    findChild<QPushButton *>("nutritionDetailsButton")->setProperty("link", true);
    connect(m_rewards, &RewardSystem::changed, this, [this] { refresh(); });
    auto *timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, &QTimer::timeout, this, [this] { refresh(); });
    timer->start();
    arrangeBadges();
    refresh();
}
void StatisticsWidget::editNutritionGoals() {
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("nutritionGoalsDialog"));
    dialog.setWindowTitle(QStringLiteral("设置每日目标"));
    dialog.setMinimumWidth(380);
    auto *outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(16);
    auto *hint = label(QStringLiteral("手动填写每日目标，留空表示不设置。"), &dialog);
    outer->addWidget(hint);
    hint->setWordWrap(true);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    const QStringList names{QStringLiteral("热量（千卡）"), QStringLiteral("蛋白质（克）"),
                            QStringLiteral("碳水化合物（克）"), QStringLiteral("脂肪（克）")};
    QVector<QLineEdit *> fields;
    for (int i = 0; i < 4; ++i) {
        auto *input = new QLineEdit(&dialog);
        input->setObjectName(QStringLiteral("nutritionGoalInput%1").arg(i));
        input->setPlaceholderText(QStringLiteral("未设置"));
        input->setMaxLength(32);
        if (m_goals[i]) input->setText(QString::number(*m_goals[i], 'g', 17));
        form->addRow(names[i], input);
        fields.append(input);
    }
    outer->addLayout(form);
    auto *error = label({}, &dialog, "nutritionGoalsError");
    error->setStyleSheet(QStringLiteral("color:#B5513D;"));
    error->hide();
    outer->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    Shixu::markPrimary(buttons->button(QDialogButtonBox::Save));
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存目标"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    outer->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        NutritionGoals next{};
        for (int i = 0; i < 4; ++i) {
            const auto input = fields[i]->text().trimmed();
            if (input.isEmpty()) continue;
            bool ok = false;
            const double value = input.toDouble(&ok);
            if (!ok || !std::isfinite(value) || value <= 0) {
                error->setText(QStringLiteral("%1请输入大于零的有效数字，或留空。 ").arg(names[i]));
                error->show();
                fields[i]->setFocus();
                return;
            }
            next[i] = value;
        }
        QString message;
        if (!m_goalsStore.save(next, message)) {
            error->setText(message);
            error->show();
            return;
        }
        m_goals = next;
        m_goalStatus->setText(QStringLiteral("每日目标已保存。"));
        m_goalStatus->show();
        dialog.accept();
        refresh();
    });
    dialog.exec();
}
void StatisticsWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    arrangeBadges();
}
void StatisticsWidget::arrangeBadges() {
    int columns = width() >= 850 ? 4 : 2;
    for (int i = 0; i < m_badges.size(); ++i)
        m_badgeLayout->addWidget(m_badges[i], i / columns, i % columns);
}
void StatisticsWidget::refresh() {
    auto today = Shixu::appNow().date();
    auto growth = m_rewards->getGrowthSnapshot(today.toString(Qt::ISODate).toStdString());
    m_error->clear();
    m_error->hide();
    auto clearMeals = [&] {
        while (auto *item = m_mealRows->takeAt(0)) { delete item->widget(); delete item; }
    };
    auto unavailable = [&](const QString &message) {
        m_error->setText(message);
        m_error->show();
        for (auto *l : {m_cooking, m_expiring, m_cookingStreak, m_recentMeals, m_nutrition, m_achievements, m_todayCount, m_todayCalories})
            l->setText(QStringLiteral("暂不可用"));
        m_recentMeals->show();
        m_mealHeading->setText(QStringLiteral("最近做饭"));
        clearMeals();
        for (auto *value : m_goalValues) value->setText(QStringLiteral("暂不可用"));
        for (auto *bar : m_goalBars) bar->setValue(0);
        m_todayDate->setText(today.toString(QStringLiteral("M 月 d 日")));
        m_nutrition->hide();
        for (auto *badge : m_badges)
            badge->hide();
        m_details->setEnabled(false);
        m_cookingAchievements->clear();
    };
    if (growth.outcome.status != V::OperationStatus::Succeeded) {
        unavailable(QString::fromUtf8(growth.outcome.message.c_str()));
        for (auto *l : {m_total, m_streak, m_points, m_status})
            l->setText(QStringLiteral("暂不可用"));
        m_checkIn->setEnabled(false);
        for (auto *d : m_recentDays)
            d->setText(QStringLiteral("—"));
        return;
    }
    const auto &g = growth.value;
    m_status->setText(g.checkedInOnReferenceDate ? QStringLiteral("今天已打卡，继续享受好好吃饭的日常。 ")
                                                 : QStringLiteral("今天也给好好吃饭留个记录。"));
    m_checkIn->setEnabled(!g.checkedInOnReferenceDate);
    m_checkIn->setText(g.checkedInOnReferenceDate ? QStringLiteral("今日已完成")
                                                  : QStringLiteral("今日打卡 · +5 积分"));
    m_points->setText(QStringLiteral("积分  %1").arg(g.totalPoints));
    m_total->setText(QStringLiteral("%1 天").arg(g.totalCheckIns));
    m_streak->setText(QStringLiteral("%1 天").arg(g.currentStreak));
    for (int i = 0; i < 7; ++i) {
        auto date = today.addDays(i - 6);
        m_recentDays[i]->setText(
            date.toString("MM/dd") + "\n" +
            (m_rewards->hasCheckedIn(date) ? QStringLiteral("已打卡") : QStringLiteral("未打卡")));
    }
    if (!m_activity) {
        unavailable(QStringLiteral("做饭记录服务暂不可用。"));
        return;
    }
    auto activity = m_activity->refresh(today.toString(Qt::ISODate).toStdString());
    if (activity.outcome.status != V::OperationStatus::Succeeded) {
        unavailable(QString::fromUtf8(activity.outcome.message.c_str()));
        return;
    }
    const auto &a = activity.value;
    m_todayDate->setText(today.toString(QStringLiteral("M 月 d 日")));
    m_todayCount->setText(QStringLiteral("%1 次").arg(a.todayMealCount));
    m_todayCalories->setText(QStringLiteral("%1 千卡").arg(a.todayNutrition.calories, 0, 'f', 0));
    const double amounts[] = {a.todayNutrition.calories, a.todayNutrition.protein,
                              a.todayNutrition.carbohydrates, a.todayNutrition.fat};
    for (int i = 0; i < 4; ++i) {
        const auto amount = QString::number(amounts[i], 'f', i == 0 ? 0 : 1);
        const auto unit = i == 0 ? QStringLiteral("千卡") : QStringLiteral("克");
        if (!m_goalsReadable || !m_goals[i]) {
            m_goalValues[i]->setText(QStringLiteral("%1 %2 · %3").arg(amount, unit,
                m_goalsReadable ? QStringLiteral("未设置目标") : QStringLiteral("目标不可用")));
            m_goalBars[i]->setValue(0);
        } else {
            const double ratio = amounts[i] / *m_goals[i];
            const double percentage = ratio * 100.0;
            const QString percent = std::isfinite(percentage) ? QString::number(percentage, 'f', 0) + "%"
                                                             : QStringLiteral("超过目标");
            m_goalValues[i]->setText(QStringLiteral("%1 / %2 %3 · %4")
                .arg(amount, QString::number(*m_goals[i], 'g', 10), unit, percent));
            m_goalBars[i]->setValue(std::isfinite(ratio) ? qRound(std::clamp(ratio, 0.0, 1.0) * 1000) : 1000);
        }
    }
    m_cooking->setText(QStringLiteral("%1 次").arg(a.totalCookingCount));
    m_cookingStreak->setText(QStringLiteral("连续做饭 %1 天").arg(a.currentCookingStreak));
    m_expiring->setText(QStringLiteral("%1 次").arg(a.expiringCookingCount));
    m_achievements->setText(
        QStringLiteral("已获得 %1 / 4 枚成就").arg(a.achievements.size()));
    QStringList badgeText;
    for (int i = 0; i < 4; ++i) {
        const auto &d = definitions[i];
        QString when;
        for (const auto &earned : a.achievements)
            if (earned.code == d.code)
                when = QString::fromStdString(earned.unlockedAt);
        auto *f = m_badges[i];
        f->show();
        QIcon icon(QString(":/icons/%1.png").arg(d.code));
        f->findChild<QLabel *>("badgeIcon")
            ->setPixmap(icon.pixmap(QSize(64, 64), when.isEmpty() ? QIcon::Disabled : QIcon::Normal));
        QString state = when.isEmpty() ? QStringLiteral("未解锁") : QStringLiteral("已解锁\n") + when;
        f->findChild<QLabel *>("badgeState")->setText(state);
        badgeText << QString::fromUtf8(d.name) + " · " + state;
    }
    m_cookingAchievements->setText(badgeText.join("\n"));
    QStringList history, nutrition;
    clearMeals();
    for (size_t i = 0; i < std::min<size_t>(10, a.recentMeals.size()); ++i) {
        const auto &m = a.recentMeals[i];
        QString title = QString::fromUtf8(m.recipeName.c_str()) + " · " + QString::fromStdString(m.time);
        history << title + QStringLiteral(" · 约 %1 千卡").arg(m.nutrition.calories, 0, 'f', 0);
        auto *row = new QFrame(this);
        row->setObjectName("mealRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(16, 12, 16, 12);
        auto *description = new QVBoxLayout;
        description->setSpacing(4);
        description->addWidget(label(QString::fromUtf8(m.recipeName.c_str()), row, "mealName"));
        description->addWidget(label(QString::fromStdString(m.time), row, "mealTime"));
        rowLayout->addLayout(description, 1);
        rowLayout->addWidget(label(QStringLiteral("约 %1 千卡").arg(m.nutrition.calories, 0, 'f', 0), row, "mealCalories"));
        m_mealRows->addWidget(row);
        nutrition << title + QStringLiteral("\n蛋白质 %1 克 · 碳水 %2 克 · 脂肪 %3 克")
                                 .arg(m.nutrition.protein, 0, 'f', 1)
                                 .arg(m.nutrition.carbohydrates, 0, 'f', 1)
                                 .arg(m.nutrition.fat, 0, 'f', 1);
    }
    m_recentMeals->setText(history.isEmpty() ? QStringLiteral("还没有完成做饭，去挑一道喜欢的菜吧。 ")
                                             : QString());
    m_recentMeals->setVisible(history.isEmpty());
    m_mealHeading->setText(QStringLiteral("最近做饭 · 显示 %1 条").arg(history.size()));
    m_nutrition->setText(nutrition.join("\n\n"));
    m_details->setEnabled(!history.isEmpty());
    m_nutrition->setVisible(!history.isEmpty() && m_details->isChecked());
}
