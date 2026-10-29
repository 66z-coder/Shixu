#pragma once
#include <QWidget>
#include "contracts/CoreContracts.h"
class QLabel;
class QPushButton;
class QVBoxLayout;
class CookingConfirmationWidget : public QWidget {
    Q_OBJECT
public:
    explicit CookingConfirmationWidget(QWidget* parent = nullptr);
    void setPreview(const Shixu::Contracts::V1::CookingCompletionPreview& preview);
    void setProgress(const Shixu::Contracts::V1::CookingCompletionResult& result);
    void setBusy(bool busy);
signals:
    void previewRequested(const Shixu::Contracts::V1::CookingCompletionRequest& request);
    void completionRequested(const Shixu::Contracts::V1::CookingCompletionRequest& request,
                             const QString& confirmationToken);
    void retryRequested(const QString& eventId);
    void cancelPreparedRequested(const QString& eventId);
private:
    Shixu::Contracts::V1::CookingCompletionRequest m_request;
    QString m_token;
    bool m_busy = false;
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QVBoxLayout* m_missing = nullptr;
    QPushButton* m_confirm = nullptr;
    QPushButton* m_retry = nullptr;
    QPushButton* m_cancelPrepared = nullptr;
};
