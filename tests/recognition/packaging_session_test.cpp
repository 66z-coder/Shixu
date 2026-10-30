#include "recognition/PackagingEntryWidget.h"
#include "ui/AppTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QThread>
#include <QTransform>
#include <functional>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path); check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "write fixture");
    check(file.write(bytes) == bytes.size(), "complete fixture");
}
QByteArray readFile(const QString& path) {
    QFile file(path); check(file.open(QIODevice::ReadOnly), "read fixture"); return file.readAll();
}
void waitUntil(const std::function<bool()>& condition, const char* message, int timeout = 5000) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < timeout) {
        QApplication::processEvents(QEventLoop::AllEvents, 20); QThread::msleep(5);
    }
    check(condition(), message);
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == "offscreen") {
        const int font = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
    }
#endif
    Shixu::applyTheme();
    try {
        QTemporaryDir files; check(files.isValid(), "temporary fixtures");
        const auto script = files.filePath("worker.py");
        const auto control = files.filePath("mode.txt");
        const auto capture = files.filePath("request.json");
        writeFile(script, R"PY(import json,os,struct,sys,time
from pathlib import Path
for raw in sys.stdin:
    request=json.loads(raw)
    image=Path(request['image'])
    width,height=struct.unpack('!II',image.read_bytes()[16:24])
    mode=Path(os.environ['SHIXU_TEST_MODE']).read_text()
    capture=Path(os.environ['SHIXU_TEST_CAPTURE'])
    temp=capture.with_suffix('.tmp')
    temp.write_text(json.dumps({'image':str(image),'width':width,'height':height}),encoding='utf-8')
    os.replace(temp,capture)
    if mode=='slow': time.sleep(20)
    if mode=='hold':
        while Path(os.environ['SHIXU_TEST_MODE']).read_text()=='hold': time.sleep(0.01)
    def line(text,x,y,w,h): return {'text':text,'confidence':0.99,'box':[[x,y],[x+w,y],[x+w,y+h],[x,y+h]]}
    result={'success':True,'lines':[line('20260927',100,0,100,20),line('\u4fdd\u8d28\u671f\u81f3\uff1a',0,30,100,20),line('\uff1a20270326',100,30,100,20),line('\u751f\u4ea7\u65e5\u671f\uff1a',0,0,100,20)]}
    if mode=='fail': result={'success':False,'errorMessage':'fixture failure'}
    Path(request['output']).write_text(json.dumps(result),encoding='utf-8')
    print('SHIXU_OCR_DONE '+request['id'],flush=True)
)PY");
        const auto originalScript = qgetenv("SHIXU_OCR_SCRIPT");
        qputenv("SHIXU_OCR_SCRIPT", script.toUtf8());
        if (qEnvironmentVariableIsEmpty("SHIXU_OCR_PYTHON"))
            qputenv("SHIXU_OCR_PYTHON", QStandardPaths::findExecutable("python").toUtf8());
        qputenv("SHIXU_TEST_MODE", control.toUtf8()); qputenv("SHIXU_TEST_CAPTURE", capture.toUtf8());
        writeFile(control, "ok");
        QImage photo(160, 80, QImage::Format_RGB32); photo.fill(Qt::white);
        photo.setPixelColor(0, 0, Qt::red); photo.setPixelColor(159, 79, Qt::blue);
        const auto path = files.filePath("package.png"); check(photo.save(path), "image fixture");
        const auto source = readFile(path);
        PackagingEntryWidget entry; entry.show(); app.processEvents();
        auto button = [&](const char* name) { auto* b = entry.findChild<QPushButton*>(name); check(b, name); return b; };
        auto* open = button("openDateRecognition"); auto* left = button("rotatePackageLeft");
        auto* right = button("rotatePackageRight"); auto* recognize = button("recognizePackage");
        auto* stop = button("stopRecognition"); auto* exit = button("cancelDateRecognition");
        auto* apply = button("applyRecognizedDates");
        auto* dialog = entry.findChild<QDialog*>("packagingDateDialog");
        auto* raw = entry.findChild<QPlainTextEdit*>("packageText");
        auto* summary = entry.findChild<QLabel*>("recognizedDateSummary");
        auto* status = entry.findChild<QLabel*>("recognitionStatus");
        auto* preview = entry.findChild<QLabel*>("packagePreview");
        auto* scroll = entry.findChild<QScrollArea*>("recognitionScroll");
        auto* service = entry.findChild<ReceiptOcrService*>("packagingOcrService"); check(service, "OCR service");
        auto* name = entry.findChild<QLineEdit*>("foodName"); name->setText("牛奶");
        auto* unit = entry.findChild<QComboBox*>("foodUnit"); unit->setCurrentText("盒");
        auto* quantity = entry.findChild<QDoubleSpinBox*>("foodQuantity"); quantity->setValue(2);
        auto* net = entry.findChild<QDoubleSpinBox*>("gramsPerUnit"); net->setValue(250);
        auto* netUnit = entry.findChild<QComboBox*>("netContentUnit"); netUnit->setCurrentText("ml");
        entry.findChild<QComboBox*>("foodCategory")->setCurrentText("其他");
        auto* expiry = entry.findChild<QLineEdit*>("expiryDate");
        entry.findChild<QComboBox*>("dateMode")->setCurrentIndex(1); expiry->setText("2027-01-01");
        auto* confirmed = entry.findChild<QCheckBox*>("confirmed"); confirmed->setChecked(true);
        auto preserved = [&] {
            check(name->text()=="牛奶" && unit->currentText()=="盒" && quantity->value()==2 && net->value()==250
                && netUnit->currentText()=="ml" && expiry->text()=="2027-01-01" && confirmed->isChecked()
                && entry.findChild<QComboBox*>("foodCategory")->currentText()=="其他", "main draft unchanged");
        };
        auto clean = [&] {
            check(raw->toPlainText().isEmpty() && summary->text().isEmpty() && !preview->text().isEmpty()
                && !apply->isEnabled() && !recognize->isEnabled() && !left->isEnabled() && !right->isEnabled()
                && scroll->verticalScrollBar()->value()==0 && !status->text().contains("failure"), "clean recognition session");
        };
        for (int closeMode=0; closeMode<3; ++closeMode) {
            open->click(); app.processEvents(); check(entry.loadImageFile(path), "import before cancel");
            raw->setPlainText("无法识别的日期"); button("parsePackageDates")->click();
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            if (closeMode==0) exit->click();
            else if (closeMode==1) dialog->close();
            else { QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(dialog,&escape); }
            check(!dialog->isVisible(), "all exit paths close dialog"); clean(); preserved();
            open->click(); app.processEvents(); clean(); dialog->reject();
        }
        open->click(); check(entry.loadImageFile(path), "image for engine failure"); writeFile(control,"fail");
        recognize->click(); waitUntil([&] { return status->text()=="fixture failure"; }, "engine failure delivered");
        check(!stop->isEnabled() && exit->isEnabled() && !apply->isEnabled(), "failure still permits exit");
        exit->click(); clean(); preserved();
        open->click(); check(entry.loadImageFile(path), "image for rotation"); writeFile(control,"hold");
        QFile::remove(capture); left->click();
        check(!QFile::exists(capture) && raw->toPlainText().isEmpty(), "rotation does not start OCR");
        recognize->click(); check(!left->isEnabled() && !right->isEnabled() && stop->isEnabled(), "rotation disabled during OCR");
        waitUntil([&] { return QFile::exists(capture); }, "worker awaiting background interaction");
        dialog->showMinimized(); app.processEvents();
        check(dialog->isMinimized() && !dialog->isModal() && !QApplication::activeModalWidget(), "minimized recognition does not block main window");
        check(service->isBusy() && open->isEnabled(), "main entry can reopen a running recognition job");
        open->click(); app.processEvents();
        check(!dialog->isMinimized() && service->isBusy() && raw->toPlainText().isEmpty(), "return to running job preserves it");
        preserved(); dialog->showMinimized(); writeFile(control,"ok");
        waitUntil([&] { return apply->isEnabled(); }, "rotated result can be applied");
        check(dialog->isMinimized() && !raw->toPlainText().isEmpty(), "OCR completes and retains its result while minimized");
        open->click(); app.processEvents();
        check(dialog->isVisible() && !dialog->isMinimized() && apply->isEnabled(), "restore keeps completed recognition ready to apply");
        waitUntil([&] {
            return scroll->viewport()->rect().contains(QRect(summary->mapTo(scroll->viewport(),QPoint()),summary->size()));
        }, "all date result lines visible after OCR");
        auto recorded = QJsonDocument::fromJson(readFile(capture)).object();
        check(recorded["width"].toInt()==80 && recorded["height"].toInt()==160, "OCR sees full-resolution rotated image");
        check(QImage(recorded["image"].toString())==photo.transformed(QTransform().rotate(-90)), "left rotation pixels");
        check(readFile(path)==source, "source photograph untouched"); preserved();
        check(raw->toPlainText().contains("生产日期: 20260927") && raw->toPlainText().contains("保质期至: 20270326"), "geometry restored before parsing");
        right->click(); check(!apply->isEnabled() && raw->toPlainText().isEmpty(), "rotation invalidates old result");
        for (int i=0;i<4;++i) left->click();
        recognize->click(); waitUntil([&] { return apply->isEnabled(); }, "four turns can be recognized");
        recorded = QJsonDocument::fromJson(readFile(capture)).object();
        check(QImage(recorded["image"].toString())==photo, "four quarter turns preserve original pixels");
        apply->click(); check(expiry->text()=="2027-03-26" && !confirmed->isChecked(), "explicit apply updates dates");
        check(entry.findChild<QLineEdit*>("productionDate")->text()=="2026-09-27", "both dates applied");
        clean(); expiry->setText("2027-01-01"); confirmed->setChecked(true);
        for (bool closeWhileRunning : {false,true}) {
            open->click(); check(entry.loadImageFile(path), "image for running cancellation"); left->click();
            writeFile(control,"slow"); QFile::remove(capture); recognize->click();
            waitUntil([&] { return QFile::exists(capture); }, "slow worker started");
            const auto temporaryImage=QJsonDocument::fromJson(readFile(capture)).object()["image"].toString();
            if (closeWhileRunning) {
                exit->click(); check(!dialog->isVisible(), "running job does not block closing");
                if(service->isBusy()) check(QFile::exists(temporaryImage), "temporary image survives until job ends");
            } else stop->click();
            waitUntil([&] { return !service->isBusy() && open->isEnabled(); }, "cancellation completes");
            preserved();
            if (closeWhileRunning) { check(!QFile::exists(temporaryImage), "temporary image cleaned after cancelled job"); clean(); }
            else { check(left->isEnabled() && recognize->isEnabled() && preview->text().isEmpty(), "stop retains image for retry"); exit->click(); }
        }
        open->click(); ReceiptOcrResult late; late.success=true; late.lines.push_back({"有效期至 2099-01-01",0.99,{}});
        service->completed(late); clean(); preserved(); dialog->reject();
        // Optional local acceptance with a real photo and the installed OCR runtime.
        const auto realPhoto=qEnvironmentVariable("SHIXU_TEST_PACKAGING_PHOTO");
        if (!realPhoto.isEmpty()) {
            if (originalScript.isEmpty()) qunsetenv("SHIXU_OCR_SCRIPT"); else qputenv("SHIXU_OCR_SCRIPT",originalScript);
            open->click(); check(entry.loadImageFile(realPhoto), "real photograph imported"); left->click(); recognize->click();
            waitUntil([&] { return !service->isBusy(); }, "real OCR completed",120000);
            check(apply->isEnabled(), "real photo date result valid");
            waitUntil([&] {
                return scroll->viewport()->rect().contains(QRect(summary->mapTo(scroll->viewport(),QPoint()),summary->size()));
            }, "real date result fully visible");
            const auto output=qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
            if (!output.isEmpty()) { QDir().mkpath(output); app.processEvents(); dialog->grab().save(output+"/recognition-rotated-result.png"); }
            apply->click(); check(expiry->text()=="2027-03-26" && entry.findChild<QLineEdit*>("productionDate")->text()=="2026-09-27", "real photo dates correct");
            std::cout << "PASS: real photo production=2026-09-27 expiry=2027-03-26\n";
        }
        std::cout << "PASS: cancel/close/Escape, late results, temporary image lifetime, rotation and geometry\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
