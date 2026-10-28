#include "ui/CookingConfirmationWidget.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
namespace { QString text(const std::string& value) { return QString::fromUtf8(value.c_str()); } }
CookingConfirmationWidget::CookingConfirmationWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    m_title = new QLabel(QStringLiteral("确认这次做饭"), this);
    m_title->setObjectName(QStringLiteral("confirmTitle"));
    layout->addWidget(m_title);
    m_status = new QLabel(QStringLiteral("正在准备预览…"), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    layout->addWidget(new QLabel(QStringLiteral("缺少或重量未知的食材，请确认是否已在外部备好："), this));
    m_missing = new QVBoxLayout;
    layout->addLayout(m_missing);
    auto* actions = new QHBoxLayout;
    m_confirm = new QPushButton(QStringLiteral("确认完成并记录"), this);
    m_retry = new QPushButton(QStringLiteral("继续完成未结算步骤"), this);
    m_cancelPrepared = new QPushButton(QStringLiteral("取消待处理记录"), this);
    actions->addWidget(m_confirm);
    actions->addWidget(m_retry);
    actions->addWidget(m_cancelPrepared);
    layout->addLayout(actions);
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
    layout->addStretch();
    setStyleSheet(QStringLiteral("QWidget { color:#24455c; background:#f1f9fc; } QLabel#confirmTitle { font-size:22px; font-weight:700; } QPushButton { background:#358fca; color:white; padding:9px; border-radius:7px; } QPushButton:disabled { background:#b4c7d2; }"));
}
void CookingConfirmationWidget::setPreview(const Shixu::Contracts::V1::CookingCompletionPreview& preview)
{
    namespace V = Shixu::Contracts::V1;
    m_request = preview.snapshot.request;
    m_token = preview.outcome.status == V::OperationStatus::Succeeded
        ? text(preview.confirmationToken) : QString();
    m_title->setText(QStringLiteral("确认做完：%1 · %2 份")
        .arg(text(preview.snapshot.recipe.name)).arg(m_request.servings));
    m_status->setText(preview.outcome.status == V::OperationStatus::Succeeded
        ? QStringLiteral("请核对食材与记录，确认后会扣减库存并记入成长。")
        : text(preview.outcome.message));
    while (auto* item = m_missing->takeAt(0)) { delete item->widget(); delete item; }
    for (const auto& item : preview.inventory.availability) {
        if (item.state == V::AvailabilityState::Enough) continue;
        const bool prepared = std::find(m_request.externallyPreparedFoods.begin(),
            m_request.externallyPreparedFoods.end(), item.foodName) != m_request.externallyPreparedFoods.end();
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
    m_confirm->setEnabled(!m_busy && !m_token.isEmpty());
}
void CookingConfirmationWidget::setProgress(const Shixu::Contracts::V1::CookingCompletionResult& result)
{
    namespace V = Shixu::Contracts::V1;
    const bool completed = result.stage == V::CompletionStage::Completed;
    const bool pending = result.stage >= V::CompletionStage::Prepared && !completed
        && result.stage != V::CompletionStage::Cancelled;
    m_status->setText(completed ? QStringLiteral("已完成：库存、饮食记录和成长积分已结算。")
        : result.stage == V::CompletionStage::Cancelled ? QStringLiteral("待处理记录已取消。")
        : QStringLiteral("进度 %1：%2").arg(static_cast<int>(result.stage)).arg(text(result.outcome.message)));
    m_token.clear();
    m_confirm->setEnabled(false);
    m_retry->setVisible(pending);
    m_cancelPrepared->setVisible(result.stage == V::CompletionStage::Prepared);
    setBusy(m_busy);
}
void CookingConfirmationWidget::setBusy(bool busy)
{
    m_busy = busy;
    m_confirm->setEnabled(!busy && !m_token.isEmpty() && !m_retry->isVisible());
    m_retry->setEnabled(!busy);
    m_cancelPrepared->setEnabled(!busy);
}
