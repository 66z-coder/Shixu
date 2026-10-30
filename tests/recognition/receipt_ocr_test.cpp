#include "recognition/ReceiptOcrService.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QDir>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QByteArray valid = R"({"success":true,"lines":[{"text":"牛奶 2盒 12.00","confidence":0.98,"box":[[1,2],[30,2],[30,12],[1,12]]}]})";
        auto parsed = ReceiptOcrService::parseResult(valid);
        check(parsed.success && parsed.lines.size() == 1 && parsed.lines[0].box.size() == 4,
              "valid OCR text and positions");
        check(!ReceiptOcrService::parseResult("log noise").success, "reject invalid JSON");
        check(!ReceiptOcrService::parseResult(R"({"success":true,"lines":[{}]})").success, "reject invalid line");
        check(ReceiptOcrService::parseResult(R"({"success":true,"lines":[]})").success, "blank image");
        check(!ReceiptOcrService::parseResult(R"({"success":false,"errorMessage":"missing model"})").success, "engine failure");

        // Process fixture: no model/network dependency, exercising actual async process lifecycle.
        QTemporaryDir temporary;
        check(temporary.isValid(), "temporary directory");
        const auto image = temporary.filePath(QStringLiteral("中文 小票.png"));
        QFile imageFile(image);
        check(imageFile.open(QIODevice::WriteOnly), "image fixture");
        imageFile.write("fixture");
        imageFile.close();
        QFile script(temporary.filePath("engine.py"));
        check(script.open(QIODevice::WriteOnly), "engine fixture");
        script.write("import sys,json,time,os\nfrom pathlib import Path\n"
                     "if sys.platform=='win32':\n"
                     "    import ctypes\n"
                     "    assert not ctypes.windll.kernel32.GetConsoleWindow(), 'OCR must not attach a console'\n"
                     "mode=os.environ.get('OCR_TEST_MODE','ok')\n"
                     "if mode=='slow': time.sleep(30)\n"
                     "if mode=='crash': sys.exit(3)\n"
                     "out=sys.argv[sys.argv.index('--output')+1]\n"
                     "Path(out).write_text(json.dumps({'success':True,'lines':[]}),encoding='utf-8')\n");
        script.close();
        qputenv("SHIXU_OCR_SCRIPT", script.fileName().toUtf8());
        if (qEnvironmentVariableIsEmpty("SHIXU_OCR_PYTHON")) {
            qputenv("SHIXU_OCR_PYTHON", QDir(QString::fromUtf8(SHIXU_PROJECT_DIR))
                .filePath(".venv-ocr/Scripts/python.exe").toUtf8());
        }
        auto run = [&](const char* mode, int timeout, bool cancel) {
            qputenv("OCR_TEST_MODE", mode);
            ReceiptOcrService service(nullptr, timeout);
            QEventLoop loop;
            ReceiptOcrResult result;
            int completions = 0;
            QObject::connect(&service, &ReceiptOcrService::completed, &loop,
                [&](const ReceiptOcrResult& value) { result = value; ++completions; loop.quit(); });
            service.start(image);
            if (service.isBusy()) {
                service.start(image); // Duplicate requests must not launch another process.
                if (cancel) QTimer::singleShot(20, &service, &ReceiptOcrService::cancel);
                QTimer::singleShot(10000, &loop, &QEventLoop::quit);
                loop.exec();
            }
            check(completions == 1 && !service.isBusy(), "single completion and idle state");
            return result;
        };
        check(run("ok", 5000, false).success, "async success");
        check(run("slow", 100, false).errorMessage.contains(QStringLiteral("超时")), "timeout");
        check(run("slow", 5000, true).errorMessage.contains(QStringLiteral("取消")), "cancel");
        check(!run("crash", 5000, false).success, "nonzero exit");
        qputenv("SHIXU_OCR_PYTHON", temporary.filePath("missing.exe").toUtf8());
        check(!run("ok", 5000, false).success, "missing environment");
        std::cout << "PASS: OCR protocol, async success, duplicate request, timeout, cancel, failure\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
