#include "ui/RecommendationWidget.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

using Mood = Shixu::RecommendationEngine::Mood;

QLabel* makeLabel(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setTextFormat(Qt::PlainText);
    return label;
}

void addScoreRow(QVBoxLayout* layout, QWidget* parent, const QString& title, double score)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    auto* name = makeLabel(title, QStringLiteral("scoreName"), parent);
    name->setFixedWidth(72);
    row->addWidget(name);

    auto* bar = new QProgressBar(parent);
    bar->setObjectName(QStringLiteral("scoreBar"));
    bar->setRange(0, 100);
    bar->setValue(static_cast<int>(std::lround(std::clamp(score, 0.0, 100.0))));
    bar->setTextVisible(false);
    row->addWidget(bar, 1);

    auto* value = makeLabel(QStringLiteral("%1").arg(score, 0, 'f', 1),
                            QStringLiteral("scoreValue"), parent);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setFixedWidth(44);
    row->addWidget(value);
    layout->addLayout(row);
}

QWidget* makeResultCard(const RecommendationViewItem& item, int rank, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("recommendationCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);

    auto* heading = new QHBoxLayout;
    heading->setSpacing(10);
    auto* rankLabel = makeLabel(QString::number(rank), QStringLiteral("rankBadge"), card);
    rankLabel->setAlignment(Qt::AlignCenter);
    rankLabel->setFixedSize(30, 30);
    heading->addWidget(rankLabel);

    auto* name = makeLabel(item.dishName, QStringLiteral("dishName"), card);
    name->setWordWrap(true);
    heading->addWidget(name, 1);

    auto* total = makeLabel(QStringLiteral("%1 分").arg(item.totalScore, 0, 'f', 1),
                            QStringLiteral("totalScore"), card);
    heading->addWidget(total);
    layout->addLayout(heading);

    auto* timeInfo = makeLabel(
        QStringLiteral("今天的心情：%1  ·  最多 %2 分钟  ·  这道菜约 %3 分钟")
            .arg(item.moodName)
            .arg(item.effectiveMaxPreparationMinutes)
            .arg(item.cookingTimeMinutes),
        QStringLiteral("timeInfo"), card);
    timeInfo->setWordWrap(true);
    layout->addWidget(timeInfo);

    auto* breakdown = makeLabel(QStringLiteral("推荐依据"),
                                QStringLiteral("recommendationSection"), card);
    layout->addWidget(breakdown);
    addScoreRow(layout, card, QStringLiteral("食材匹配"), item.ingredientMatchScore);
    addScoreRow(layout, card, QStringLiteral("临期利用"), item.expiryUrgencyScore);
    addScoreRow(layout, card, QStringLiteral("心情匹配"), item.moodMatchScore);
    if (item.nutritionScoreAvailable) {
        addScoreRow(layout, card, QStringLiteral("能量目标"), item.nutritionScore);
    }
    addScoreRow(layout, card, QStringLiteral("制作时间"), item.preparationTimeScore);

    auto* timeReference = makeLabel(
        QStringLiteral("制作时间得分按 %1 分钟基准计算。")
            .arg(item.preparationTimeReferenceMinutes),
        QStringLiteral("timeReference"), card);
    layout->addWidget(timeReference);

    if (!item.missingIngredients.isEmpty()) {
        auto* missing = makeLabel(
            QStringLiteral("还缺食材：%1").arg(item.missingIngredients.join(QStringLiteral("、"))),
            QStringLiteral("missingIngredients"), card);
        missing->setWordWrap(true);
        layout->addWidget(missing);
    }

    auto* reasonsTitle = makeLabel(QStringLiteral("推荐理由"),
                                   QStringLiteral("recommendationSection"), card);
    layout->addWidget(reasonsTitle);
    if (item.reasons.isEmpty()) {
        layout->addWidget(makeLabel(QStringLiteral("暂无详细理由"),
                                    QStringLiteral("recommendationReason"), card));
    } else {
        for (const QString& text : item.reasons) {
            auto* reason = makeLabel(QStringLiteral("• %1").arg(text),
                                     QStringLiteral("recommendationReason"), card);
            reason->setWordWrap(true);
            layout->addWidget(reason);
        }
    }
    return card;
}

} // namespace

RecommendationWidget::RecommendationWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* body = new QWidget(scroll);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 24, 24, 24);
    bodyLayout->setSpacing(16);
    scroll->setWidget(body);

    auto* controls = new QFrame(body);
    controls->setObjectName(QStringLiteral("recommendationControls"));
    auto* controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(22, 20, 22, 20);
    controlsLayout->setSpacing(12);
    controlsLayout->addWidget(makeLabel(QStringLiteral("今天想怎么做饭？"),
                                         QStringLiteral("controlsTitle"), controls));
    controlsLayout->addWidget(makeLabel(
        QStringLiteral("先选一个心情。它会决定推荐菜谱的最长制作时间。"),
        QStringLiteral("controlsDescription"), controls));

    m_moodButtons = new QButtonGroup(this);
    m_moodButtons->setExclusive(true);
    auto* moodRow = new QHBoxLayout;
    moodRow->setSpacing(10);
    const struct { Mood mood; const char* name; int minutes; } choices[] = {
        {Mood::ReadyToCook, "有心情做饭", 60},
        {Mood::Neutral, "状态一般", 30},
        {Mood::Tired, "疲惫、想省事", 15},
    };
    for (const auto& choice : choices) {
        auto* button = new QPushButton(
            QStringLiteral("%1\n最多 %2 分钟")
                .arg(QString::fromUtf8(choice.name))
                .arg(choice.minutes),
            controls);
        button->setObjectName(QStringLiteral("moodChoice"));
        button->setCheckable(true);
        button->setMinimumHeight(68);
        button->setCursor(Qt::PointingHandCursor);
        m_moodButtons->addButton(button, static_cast<int>(choice.mood));
        moodRow->addWidget(button, 1);
        connect(button, &QPushButton::clicked, this, [this] {
            m_selectionHint->hide();
        });
    }
    controlsLayout->addLayout(moodRow);

    auto* limitRow = new QHBoxLayout;
    limitRow->addWidget(makeLabel(QStringLiteral("额外时间上限"),
                                  QStringLiteral("limitLabel"), controls));
    m_customLimit = new QSpinBox(controls);
    m_customLimit->setObjectName(QStringLiteral("customTimeLimit"));
    m_customLimit->setRange(0, 240);
    m_customLimit->setSpecialValueText(QStringLiteral("不额外限制"));
    m_customLimit->setSuffix(QStringLiteral(" 分钟"));
    m_customLimit->setValue(0);
    limitRow->addWidget(m_customLimit);
    limitRow->addStretch();
    controlsLayout->addLayout(limitRow);
    controlsLayout->addWidget(makeLabel(
        QStringLiteral("填写后，会与心情对应的时间上限取较短值。"),
        QStringLiteral("limitHelp"), controls));

    m_selectionHint = makeLabel(QStringLiteral("请先选择今天的心情。"),
                                 QStringLiteral("selectionHint"), controls);
    m_selectionHint->hide();
    controlsLayout->addWidget(m_selectionHint);

    auto* generateButton = new QPushButton(QStringLiteral("生成推荐"), controls);
    generateButton->setObjectName(QStringLiteral("generateRecommendations"));
    generateButton->setCursor(Qt::PointingHandCursor);
    generateButton->setMinimumHeight(42);
    controlsLayout->addWidget(generateButton);
    connect(generateButton, &QPushButton::clicked, this, [this] {
        const int selected = m_moodButtons->checkedId();
        if (selected < 0) {
            m_selectionHint->show();
            return;
        }
        m_selectionHint->hide();
        emit recommendationRequested(static_cast<Mood>(selected), m_customLimit->value());
    });
    bodyLayout->addWidget(controls);

    m_warning = makeLabel({}, QStringLiteral("recommendationWarnings"), body);
    m_warning->setWordWrap(true);
    m_warning->hide();
    bodyLayout->addWidget(m_warning);

    m_summary = makeLabel({}, QStringLiteral("recommendationSummary"), body);
    m_summary->hide();
    bodyLayout->addWidget(m_summary);

    m_emptyState = new QWidget(body);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(10);
    m_emptyTitle = makeLabel(QStringLiteral("还没有生成推荐"),
                             QStringLiteral("emptyTitle"), m_emptyState);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    m_emptyDescription = makeLabel(QStringLiteral("选择今天的心情，再点击“生成推荐”。"),
                                   QStringLiteral("emptyDescription"), m_emptyState);
    m_emptyDescription->setAlignment(Qt::AlignCenter);
    m_emptyDescription->setWordWrap(true);
    emptyLayout->addWidget(m_emptyTitle);
    emptyLayout->addWidget(m_emptyDescription);
    bodyLayout->addWidget(m_emptyState, 1);

    auto* results = new QWidget(body);
    m_resultsLayout = new QVBoxLayout(results);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(14);
    bodyLayout->addWidget(results);
    bodyLayout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QFrame#recommendationControls, QFrame#recommendationCard {
            background: #F9FCFD; border: 1px solid #DCEAF0; border-radius: 12px;
        }
        QLabel#controlsTitle { color: #244E6A; font-size: 21px; font-weight: 700; }
        QLabel#controlsDescription, QLabel#limitHelp, QLabel#timeReference {
            color: #6B8797; font-size: 13px;
        }
        QLabel#limitLabel { color: #31566D; font-size: 14px; }
        QPushButton#moodChoice {
            text-align: center; color: #31566D; background: #F0F7FA;
            border: 1px solid #D6E7EF; border-radius: 10px; padding: 8px;
            font-size: 14px;
        }
        QPushButton#moodChoice:hover { background: #E8F3F8; }
        QPushButton#moodChoice:checked {
            color: #185D88; background: #DCEEF8; border: 2px solid #62A9D3;
            font-weight: 700;
        }
        QPushButton#generateRecommendations {
            color: white; background: #358FCA; border: 0; border-radius: 9px;
            font-size: 15px; font-weight: 700;
        }
        QPushButton#generateRecommendations:hover { background: #267DB6; }
        QLabel#selectionHint { color: #C05239; font-size: 13px; }
        QLabel#recommendationWarnings {
            color: #805926; background: #FFF6E6; border: 1px solid #F0DDB4;
            border-radius: 8px; padding: 12px; font-size: 13px;
        }
        QLabel#recommendationSummary { color: #607F91; font-size: 14px; }
        QLabel#emptyTitle { color: #31566D; font-size: 22px; font-weight: 600; }
        QLabel#emptyDescription { color: #7893A0; font-size: 14px; }
        QLabel#rankBadge {
            background: #DCEEF8; color: #1B6297; border-radius: 15px; font-weight: 700;
        }
        QLabel#dishName { color: #26465B; font-size: 19px; font-weight: 600; }
        QLabel#totalScore { color: #1B6297; font-size: 18px; font-weight: 700; }
        QLabel#timeInfo { color: #2B719D; font-size: 14px; font-weight: 600; }
        QLabel#recommendationSection { color: #435F70; font-size: 14px; font-weight: 600; }
        QLabel#scoreName, QLabel#recommendationReason, QLabel#missingIngredients {
            color: #536F7F; font-size: 13px;
        }
        QLabel#scoreValue { color: #1B6297; font-size: 13px; }
        QProgressBar#scoreBar {
            background: #E6F0F4; border: 0; border-radius: 4px; max-height: 8px;
        }
        QProgressBar#scoreBar::chunk { background: #63A9D2; border-radius: 4px; }
    )"));
}

void RecommendationWidget::setWarnings(const QStringList& warnings)
{
    m_warning->setVisible(!warnings.isEmpty());
    m_warning->setText(warnings.isEmpty()
        ? QString()
        : QStringLiteral("库存提示：\n• %1").arg(warnings.join(QStringLiteral("\n• "))));
}

void RecommendationWidget::setRecommendations(const QVector<RecommendationViewItem>& items)
{
    while (QLayoutItem* child = m_resultsLayout->takeAt(0)) {
        if (QWidget* widget = child->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete child;
    }

    m_emptyState->setVisible(items.isEmpty());
    m_summary->setVisible(!items.isEmpty());
    if (items.isEmpty()) {
        m_emptyTitle->setText(QStringLiteral("没有符合条件的菜谱"));
        m_emptyDescription->setText(
            QStringLiteral("试试调整心情或额外时间上限，也可以先录入更多食材。"));
        return;
    }

    m_summary->setText(QStringLiteral("为你找到 %1 道推荐菜谱").arg(items.size()));
    for (int index = 0; index < items.size(); ++index) {
        m_resultsLayout->addWidget(makeResultCard(items.at(index), index + 1, this));
    }
}
