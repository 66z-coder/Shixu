#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <memory>
#include "model/ReceiptOcrResult.h"

class ReceiptOcrService : public QObject
{
    Q_OBJECT
public:
    explicit ReceiptOcrService(QObject* parent = nullptr, int timeoutMs = 120000);
    ~ReceiptOcrService() override;
    bool isBusy() const { return busy_; }
    void start(const QString& imagePath);
    void cancel();
    static ReceiptOcrResult parseResult(const QByteArray& json);

signals:
    void completed(const ReceiptOcrResult& result);

private:
    void finish(const ReceiptOcrResult& result);
    void collectDiagnostics();
    void sendRequest();
    QProcess process_;
    QTimer timer_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QByteArray diagnostics_;
    QByteArray protocolBuffer_;
    QByteArray pendingRequest_;
    QByteArray requestId_;
    QString workerKey_;
    quint64 nextRequestId_ = 0;
    bool busy_ = false;
    bool cancelled_ = false;
    bool timedOut_ = false;
    int timeoutMs_;
};
