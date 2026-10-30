#include "recognition/ReceiptOcrService.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <cmath>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
ReceiptOcrResult failure(const QString& message)
{
    ReceiptOcrResult result;
    result.errorMessage = message;
    return result;
}

QString setting(const char* name, const QString& fallback)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}
}

ReceiptOcrService::ReceiptOcrService(QObject* parent, int timeoutMs)
    : QObject(parent), timeoutMs_(timeoutMs)
{
#ifdef Q_OS_WIN
    // Run the console-based OCR engine silently while retaining its output pipes.
    process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this] {
        timedOut_ = true;
        process_.kill();
    });
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &ReceiptOcrService::collectDiagnostics);
    connect(&process_, &QProcess::readyReadStandardError,
            this, &ReceiptOcrService::collectDiagnostics);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && busy_) {
            finish(failure(QStringLiteral("无法启动本地 OCR：%1").arg(process_.errorString())));
        }
    });
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
        if (!busy_) return;
        collectDiagnostics();
        if (cancelled_) {
            finish(failure(QStringLiteral("已取消识别")));
            return;
        }
        if (timedOut_) {
            finish(failure(QStringLiteral("识别超时，请重试或使用更清晰的包装图片")));
            return;
        }
        QFile output(temporary_->filePath("result.json"));
        if (status != QProcess::NormalExit || !output.open(QIODevice::ReadOnly)
            || output.size() > 8 * 1024 * 1024) {
            finish(failure(QStringLiteral("本地 OCR 未正常返回结果。%1")
                .arg(QString::fromUtf8(diagnostics_).right(1500))));
            return;
        }
        auto result = parseResult(output.readAll());
        if (code != 0 && result.success) {
            result = failure(QStringLiteral("本地 OCR 异常退出"));
        }
        finish(result);
    });
}

ReceiptOcrService::~ReceiptOcrService()
{
    disconnect(&process_, nullptr, this, nullptr);
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(3000);
    }
}

void ReceiptOcrService::collectDiagnostics()
{
    diagnostics_ += process_.readAllStandardOutput();
    diagnostics_ += process_.readAllStandardError();
    diagnostics_ = diagnostics_.right(16000);
}

void ReceiptOcrService::start(const QString& imagePath)
{
    if (busy_) return;
    const QDir appDir(QCoreApplication::applicationDirPath());
    const bool bundled = QFileInfo(appDir.filePath("ocr/receipt_ocr.py")).isFile();
    const QDir root(bundled ? appDir : QDir(QString::fromUtf8(SHIXU_PROJECT_DIR)));
    const QString python = setting("SHIXU_OCR_PYTHON", root.filePath(bundled ? "runtime/python/python.exe" : ".venv-ocr/Scripts/python.exe"));
    const QString script = setting("SHIXU_OCR_SCRIPT", root.filePath("ocr/receipt_ocr.py"));
    const QString models = setting("SHIXU_OCR_MODELS", root.filePath("ocr/models"));
    if (!QFileInfo(imagePath).isFile()) {
        emit completed(failure(QStringLiteral("包装图片不存在，请重新导入")));
        return;
    }
    if (!QFileInfo(python).isFile() || !QFileInfo(script).isFile()) {
        emit completed(failure(bundled
            ? QStringLiteral("尚未安装离线图片识别扩展。请将扩展包中的 runtime 和 ocr 文件夹解压到 Shixu.exe 所在目录；也可以继续手动录入食材。")
            : QStringLiteral("本地 OCR 环境未准备好，请按 docs/local_ocr.md 安装")));
        return;
    }
    temporary_ = std::make_unique<QTemporaryDir>();
    if (!temporary_->isValid()) {
        emit completed(failure(QStringLiteral("无法创建 OCR 临时结果目录")));
        return;
    }
    diagnostics_.clear();
    busy_ = true;
    cancelled_ = timedOut_ = false;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONUTF8", "1");
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("PADDLE_PDX_DISABLE_MODEL_SOURCE_CHECK", "True");
    environment.insert("HF_HUB_OFFLINE", "1");
    process_.setProcessEnvironment(environment);
    process_.setWorkingDirectory(root.absolutePath());
    process_.start(python, {script, "--image", QFileInfo(imagePath).absoluteFilePath(),
                           "--models", models, "--output", temporary_->filePath("result.json")});
    timer_.start(timeoutMs_);
}

void ReceiptOcrService::cancel()
{
    if (busy_) {
        cancelled_ = true;
        process_.kill();
    }
}

void ReceiptOcrService::finish(const ReceiptOcrResult& result)
{
    timer_.stop();
    busy_ = false;
    temporary_.reset();
    emit completed(result);
}

ReceiptOcrResult ReceiptOcrService::parseResult(const QByteArray& json)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return failure(QStringLiteral("OCR 返回的结果不是有效 JSON"));
    const auto object = document.object();
    if (!object.value("success").isBool())
        return failure(QStringLiteral("OCR 结果缺少成功状态"));
    if (!object.value("success").toBool()) {
        const QString message = object.value("errorMessage").toString();
        return failure(message.isEmpty() ? QStringLiteral("本地 OCR 识别失败") : message);
    }
    if (!object.value("lines").isArray())
        return failure(QStringLiteral("OCR 结果缺少文字行"));
    ReceiptOcrResult result;
    for (const auto& value : object.value("lines").toArray()) {
        const auto line = value.toObject();
        const auto text = line.value("text");
        const auto confidence = line.value("confidence");
        const auto box = line.value("box");
        if (!text.isString() || text.toString().trimmed().isEmpty() ||
            !confidence.isDouble() || !std::isfinite(confidence.toDouble()) ||
            confidence.toDouble() < 0 || confidence.toDouble() > 1 ||
            !box.isArray() || box.toArray().size() != 4)
            return failure(QStringLiteral("OCR 文字行格式不正确"));
        ReceiptOcrLine entry;
        entry.text = text.toString();
        entry.confidence = confidence.toDouble();
        for (const auto& point : box.toArray()) {
            const auto coordinates = point.toArray();
            if (coordinates.size() != 2 || !coordinates[0].isDouble() || !coordinates[1].isDouble())
                return failure(QStringLiteral("OCR 文字位置格式不正确"));
            entry.box << QPointF(coordinates[0].toDouble(), coordinates[1].toDouble());
        }
        result.lines.push_back(entry);
    }
    result.success = true; // 空白图片也是一次成功执行，由界面提示未发现文字。
    return result;
}
