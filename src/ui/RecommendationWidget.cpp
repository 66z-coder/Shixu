#include "ui/RecommendationWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QProgressBar>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

QLabel* makeLabel(const QString& text, const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
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

    auto* breakdown = makeLabel(QStringLiteral("推荐依据"),
                                QStringLiteral("recommendationSection"), card);
    layout->addWidget(breakdown);
    addScoreRow(layout, card, QStringLiteral("食材匹配"), item.ingredientMatchScore);
    addScoreRow(layout, card, QStringLiteral("临期利用"), item.expiryUrgencyScore);
    addScoreRow(layout, card, QStringLiteral("口味偏好"), item.preferenceMatchScore);
    addScoreRow(layout, card, QStringLiteral("营养评分"), item.nutritionScore);
    addScoreRow(layout, card, QStringLiteral("准备时间"), item.preparationTimeScore);

    auto* reasonsTitle = makeLabel(QStringLiteral("推荐理由"),
                                   QStringLiteral("recommendationSection"), card);
    layout->addWidget(reasonsTitle);
    if (item.reasons.isEmpty()) {
        auto* reason = makeLabel(QStringLiteral("暂无详细理由"),
                                 QStringLiteral("recommendationReason"), card);
        layout->addWidget(reason);
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

    m_summary = makeLabel({}, QStringLiteral("recommendationSummary"), body);
    bodyLayout->addWidget(m_summary);

    m_emptyState = new QWidget(body);
    auto* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(10);
    auto* emptyTitle = makeLabel(QStringLiteral("暂无推荐菜谱"),
                                 QStringLiteral("emptyTitle"), m_emptyState);
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto* emptyDescription = makeLabel(QStringLiteral("添加食材和菜谱后，这里会显示适合你的菜品。"),
                                       QStringLiteral("emptyDescription"), m_emptyState);
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setWordWrap(true);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyDescription);
    bodyLayout->addWidget(m_emptyState, 1);

    auto* results = new QWidget(body);
    m_resultsLayout = new QVBoxLayout(results);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(14);
    bodyLayout->addWidget(results);
    bodyLayout->addStretch();

    setStyleSheet(QStringLiteral(R"(
        QLabel#recommendationSummary { color: #607367; font-size: 14px; }
        QLabel#emptyTitle { color: #405347; font-size: 22px; font-weight: 600; }
        QLabel#emptyDescription { color: #77867B; font-size: 14px; }
        QFrame#recommendationCard {
            background: #F8FAF7; border: 1px solid #E4EAE2; border-radius: 10px;
        }
        QLabel#rankBadge {
            background: #E3F1E7; color: #246B45; border-radius: 15px; font-weight: 700;
        }
        QLabel#dishName { color: #26352C; font-size: 19px; font-weight: 600; }
        QLabel#totalScore { color: #246B45; font-size: 18px; font-weight: 700; }
        QLabel#recommendationSection { color: #43534A; font-size: 14px; font-weight: 600; }
        QLabel#scoreName, QLabel#recommendationReason { color: #53675A; font-size: 13px; }
        QLabel#scoreValue { color: #246B45; font-size: 13px; }
        QProgressBar#scoreBar {
            background: #E6ECE6; border: 0; border-radius: 4px; max-height: 8px;
        }
        QProgressBar#scoreBar::chunk { background: #59A875; border-radius: 4px; }
    )"));
    setRecommendations({});
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
        return;
    }

    m_summary->setText(QStringLiteral("为你找到 %1 道推荐菜谱").arg(items.size()));
    for (int index = 0; index < items.size(); ++index) {
        m_resultsLayout->addWidget(makeResultCard(items.at(index), index + 1, this));
    }
}
