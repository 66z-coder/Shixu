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
        script.write(R"PY(import sys,json,time,os
from pathlib import Path
if sys.platform=='win32':
    import ctypes
    assert not ctypes.windll.kernel32.GetConsoleWindow(), 'OCR must not attach a console'
count=0
for line in sys.stdin:
    request=json.loads(line)
    mode=Path(request['image']).read_text()
    count+=1
    if mode=='slow': time.sleep(30)
    if mode=='crash': sys.exit(3)
    result={'success':True,'lines':[{'text':str(os.getpid())+':'+str(count),'confidence':1.0,'box':[[0,0],[1,0],[1,1],[0,1]]}]}
    if mode=='failure': result={'success':False,'errorMessage':'invalid image'}
    content=json.dumps(result)
    if mode=='invalid': content='not JSON'
    if mode=='large': content='x'*(9*1024*1024)
    if mode!='missing': Path(request['output']).write_text(content,encoding='utf-8')
    print('library stdout log',flush=True)
    print('library stderr log',file=sys.stderr,flush=True)
    print('SHIXU_OCR_DONE stale',flush=True)
    message='SHIXU_OCR_DONE '+request['id']+'\n'
    if mode=='wrong': message='SHIXU_OCR_DONE wrong\n'
    sys.stdout.write(message[:8]); sys.stdout.flush()
    time.sleep(0.01)
    sys.stdout.write(message[8:]); sys.stdout.flush()
)PY");
        script.close();
        qputenv("SHIXU_OCR_SCRIPT", script.fileName().toUtf8());
        if (qEnvironmentVariableIsEmpty("SHIXU_OCR_PYTHON")) {
            qputenv("SHIXU_OCR_PYTHON", QDir(QString::fromUtf8(SHIXU_PROJECT_DIR))
                .filePath(".venv-ocr/Scripts/python.exe").toUtf8());
        }
        auto run = [&](ReceiptOcrService& service, const char* mode, bool cancel = false) {
            QFile imageFile(image);
            check(imageFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "image fixture mode");
            imageFile.write(mode);
            imageFile.close();
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
        ReceiptOcrService service(nullptr, 4000);
        const auto first = run(service, "ok");
        const auto second = run(service, "ok");
        check(first.success && second.success, "async success");
        check(first.lines[0].text.section(':', 0, 0) == second.lines[0].text.section(':', 0, 0)
              && second.lines[0].text.endsWith(":2"), "reuse worker between requests");
        check(!run(service, "failure").success, "image failure");
        check(run(service, "ok").success, "reuse after image failure");
        check(!run(service, "invalid").success, "invalid result");
        check(!run(service, "large").success, "bounded result file");
        check(!run(service, "missing").success, "missing result file");
        check(!run(service, "crash").success, "nonzero exit");
        check(run(service, "ok").success, "restart after worker crash");
        check(run(service, "slow", true).errorMessage.contains(QStringLiteral("取消")), "cancel");
        check(run(service, "ok").success, "restart after cancel");
        ReceiptOcrService quick(nullptr, 500);
        check(run(quick, "slow").errorMessage.contains(QStringLiteral("超时")), "timeout");
        check(run(quick, "ok").success, "restart after timeout");
        check(run(quick, "wrong").errorMessage.contains(QStringLiteral("超时")), "reject stale request identifier");
        qputenv("SHIXU_OCR_PYTHON", temporary.filePath("missing.exe").toUtf8());
        check(!run(service, "ok").success, "missing environment");
        std::cout << "PASS: OCR protocol, reusable worker, split messages, restart, timeout, cancel, failure\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
