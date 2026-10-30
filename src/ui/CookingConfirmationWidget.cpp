#include "ui/CookingConfirmationWidget.h"
#include "core/PantryStaples.h"
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
namespace { QString text(const std::string& value) { return QString::fromUtf8(value.c_str()); } }
CookingConfirmationWidget::CookingConfirmationWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("cookingConfirmationRoot"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    m_title = new QLabel(QStringLiteral("确认这次做饭"), this);
    m_title->setObjectName(QStringLiteral("confirmTitle"));
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    m_status = new QLabel(QStringLiteral("正在准备预览…"), this);
    m_status->setObjectName(QStringLiteral("confirmStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *card = new QFrame(this); card->setObjectName(QStringLiteral("confirmCard"));
    m_missingCard = card;
    auto *cardLayout = new QVBoxLayout(card); cardLayout->setContentsMargins(20, 20, 20, 20); cardLayout->setSpacing(10);
    m_missingHeading = new QLabel(QStringLiteral("核对食材"), card); m_missingHeading->setObjectName(QStringLiteral("confirmSection"));
    m_missingHeading->setWordWrap(true); cardLayout->addWidget(m_missingHeading);
    auto *scroll = new QScrollArea(card); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *list = new QWidget(scroll); list->setObjectName(QStringLiteral("confirmList"));
    m_missing = new QVBoxLayout(list); m_missing->setContentsMargins(0, 0, 0, 0); m_missing->setSpacing(8);
    scroll->setWidget(list); scroll->setMinimumHeight(100); scroll->setMaximumHeight(260);
    cardLayout->addWidget(scroll); layout->addWidget(card);
    auto* actions = new QHBoxLayout;
    m_confirm = new QPushButton(QStringLiteral("确认完成并记录"), this);
    m_retry = new QPushButton(QStringLiteral("继续完成未结算步骤"), this);
    m_cancelPrepared = new QPushButton(QStringLiteral("取消待处理记录"), this);
    m_close = new QPushButton(QStringLiteral("关闭（未完成记录会保留）"), this);
    m_close->setObjectName(QStringLiteral("confirmClose"));
    m_confirm->setObjectName(QStringLiteral("confirmPrimary"));
    m_confirm->setProperty("primary", true);
    actions->addWidget(m_confirm);
    actions->addWidget(m_retry);
    actions->addWidget(m_cancelPrepared);
    actions->addStretch();
    layout->addLayout(actions);
    layout->addWidget(m_close, 0, Qt::AlignLeft);
    m_confirm->setEnabled(false);
    m_retry->hide();
    m_cancelPrepared->hide();
    connect(m_confirm, &QPushButton::clicked, this, [this] {
        if (!m_busy && !m_token.isEmpty()) emit completionRequested(m_request, m_token);
    });
    connect(m_retry, &QPushButton::clicked, this, [this] {
        if (!m_busy) emit retryRequested(text(m_request.eventId));
    });
    connect(m_cancelPrepared, &QPushButton::clicked, this, [this] {
        if (!m_busy) emit cancelPreparedRequested(text(m_request.eventId));
    });
    connect(m_close, &QPushButton::clicked, this, &CookingConfirmationWidget::closeRequested);
    layout->addStretch();
    setStyleSheet(QStringLiteral("QWidget#cookingConfirmationRoot { background:#EDF7F9; } QLabel#confirmTitle { font-size:24px; font-weight:700; } QLabel#confirmStatus { background:white; border:1px solid #DCE8EF; border-radius:14px; padding:18px; font-size:15px; color:#385E72; } QFrame#confirmCard { background:white; border:1px solid #DCE8EF; border-radius:14px; } QWidget#confirmList { background:white; } QLabel#confirmSection { background:transparent; font-size:18px; font-weight:700; } QCheckBox { background:#F3F8FC; padding:10px; border-radius:7px; }"));
}
void CookingConfirmationWidget::setPreview(const Shixu::Contracts::V1::CookingCompletionPreview& preview)
{
    namespace V = Shixu::Contracts::V1;
    m_request = preview.snapshot.request;
    m_recipeName = text(preview.snapshot.recipe.name);
    m_calories = preview.snapshot.nutrition.calories;
    m_confirm->show();
    m_close->setText(QStringLiteral("关闭（未完成记录会保留）"));
    m_missingCard->show();
    m_allowChoices = preview.outcome.status == V::OperationStatus::NeedsConfirmation;
    m_token = preview.outcome.status == V::OperationStatus::Succeeded
        ? text(preview.confirmationToken) : QString();
    // A fresh successful preview replaces the previous retry state.
    if (!m_token.isEmpty()) {
        m_retry->hide();
        m_cancelPrepared->hide();
    }
    m_title->setText(QStringLiteral("确认做完：%1 · %2 份")
        .arg(m_recipeName).arg(m_request.servings));
    m_status->setText(preview.outcome.status == V::OperationStatus::Succeeded
        ? QStringLiteral("请核对食材与记录，确认后会扣减库存并记入成长。")
        : text(preview.outcome.message));
    while (auto* item = m_missing->takeAt(0)) { delete item->widget(); delete item; }
    int missingCount = 0;
    for (const auto& item : preview.inventory.availability) {
        if (item.state == V::AvailabilityState::Enough) continue;
        const bool prepared = std::find(m_request.externallyPreparedFoods.begin(),
            m_request.externallyPreparedFoods.end(), item.foodName) != m_request.externallyPreparedFoods.end();
        if (prepared && Shixu::isPantryStaple(item.foodName)) continue;
        ++missingCount;
        auto* check = new QCheckBox(QStringLiteral("%1 · 需 %2 克 · %3")
            .arg(text(item.foodName)).arg(item.requiredGrams, 0, 'f', 0)
            .arg(item.state == V::AvailabilityState::Unknown ? QStringLiteral("重量未知")
                : QStringLiteral("还差 %1 克").arg(item.knownGapGrams, 0, 'f', 0)), this);
        check->setChecked(prepared);
        check->setEnabled(preview.outcome.status == V::OperationStatus::NeedsConfirmation && !m_busy);
        m_missing->addWidget(check);
        connect(check, &QCheckBox::toggled, this, [this, food = item.foodName](bool checked) {
            auto& foods = m_request.externallyPreparedFoods;
            foods.erase(std::remove(foods.begin(), foods.end(), food), foods.end());
            if (checked) foods.push_back(food);
            m_token.clear();
            m_confirm->setEnabled(false);
            emit previewRequested(m_request);
        });
    }
    m_missingHeading->setText(missingCount ? QStringLiteral("请确认缺少或重量未知的食材已在外部备好")
                                           : QStringLiteral("食材已齐全，常备调料默认已备好"));
    m_missingHeading->setVisible(preview.outcome.status == V::OperationStatus::Succeeded ||
                                 preview.outcome.status == V::OperationStatus::NeedsConfirmation);
    m_missingCard->setVisible(preview.outcome.status == V::OperationStatus::Succeeded ||
                              preview.outcome.status == V::OperationStatus::NeedsConfirmation);
    setBusy(m_busy);
}
void CookingConfirmationWidget::setProgress(const Shixu::Contracts::V1::CookingCompletionResult& result)
{
    namespace V = Shixu::Contracts::V1;
    const bool completed = result.stage == V::CompletionStage::Completed;
    const bool pending = result.stage >= V::CompletionStage::Prepared && !completed
        && result.stage != V::CompletionStage::Cancelled;
    m_status->setText(completed ? QStringLiteral("已完成：库存、饮食记录和成长积分已结算，今日已打卡。")
        : result.stage == V::CompletionStage::Cancelled ? QStringLiteral("待处理记录已取消。")
        : QStringLiteral("进度 %1：%2").arg(static_cast<int>(result.stage)).arg(text(result.outcome.message)));
    m_token.clear();
    if (completed) {
        m_title->setText(QStringLiteral("✓  做饭完成\n%1 · 约 %2 千卡")
                             .arg(m_recipeName).arg(m_calories, 0, 'f', 0));
        m_confirm->hide();
        m_close->setText(QStringLiteral("完成"));
    } else if (result.stage == V::CompletionStage::Cancelled) {
        m_confirm->hide();
        m_close->setText(QStringLiteral("关闭"));
    }
    if (completed || result.stage == V::CompletionStage::Cancelled) m_missingCard->hide();
    m_confirm->setEnabled(false);
    m_retry->setVisible(pending);
    m_cancelPrepared->setVisible(result.stage == V::CompletionStage::Prepared);
    setBusy(m_busy);
}
void CookingConfirmationWidget::setBusy(bool busy)
{
    m_busy = busy;
    for (auto* check : findChildren<QCheckBox*>()) check->setEnabled(!busy && m_allowChoices && m_retry->isHidden());
    m_confirm->setEnabled(!busy && !m_token.isEmpty() && !m_retry->isVisible());
    m_retry->setEnabled(!busy);
    m_cancelPrepared->setEnabled(!busy);
}
