#include "recognition/PackagingEntryWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QUrl>
#include <iostream>
#include <stdexcept>
void check(bool b, const char *m) {
    if (!b)
        throw std::runtime_error(m);
}
bool drop(QWidget *target, const QList<QUrl> &urls) {
    QMimeData mime;
    mime.setUrls(urls);
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted())
        return false;
    QDropEvent finish(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &finish);
    return finish.isAccepted();
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        QTemporaryDir dir(QDir::currentPath() + "/drop-XXXXXX");
        check(dir.isValid(), "temporary directory");
        auto path = dir.filePath(QStringLiteral("包装 图片.png"));
        QImage image(160, 80, QImage::Format_RGB32);
        image.fill(Qt::red);
        check(image.save(path), "write image");
        PackagingEntryWidget entry;
        entry.show();
        entry.findChild<QPushButton*>("openDateRecognition")->click();
        QApplication::processEvents();
        auto *area = entry.findChild<QWidget *>("packageDropArea");
        auto *preview = entry.findChild<QLabel *>("packagePreview");
        auto *raw = entry.findChild<QPlainTextEdit *>("packageText");
        check(area && preview && raw, "drop targets");
        check(drop(preview, {QUrl::fromLocalFile(path)}), "valid local image drop");
        check(entry.loadImageFile(path), "shared picker import path");
        raw->setPlainText("preserve this text");
        auto invalid = dir.filePath("fake.jpg");
        QFile file(invalid);
        check(file.open(QIODevice::WriteOnly), "invalid fixture");
        file.write("not an image");
        file.close();
        for (const auto &urls :
             {QList<QUrl>{QUrl::fromLocalFile(path), QUrl::fromLocalFile(path)},
              QList<QUrl>{QUrl("https://example.com/image.png")},
              QList<QUrl>{QUrl::fromLocalFile(dir.path())}, QList<QUrl>{QUrl::fromLocalFile(invalid)}}) {
            check(!drop(area, urls), "invalid drop refused");
            check(raw->toPlainText() == "preserve this text", "invalid drop preserves form");
        }
        check(!entry.loadImageFile(invalid) && raw->toPlainText() == "preserve this text",
              "picker invalid image preserves form");
        check(drop(raw->viewport(), {QUrl::fromLocalFile(path)}) && raw->toPlainText().isEmpty(),
              "drop over child editor uses same reset path");
        // Start OCR but do not run the event loop: import must be rejected while its job is active.
        QPushButton *recognize = nullptr;
        QPushButton *cancel = nullptr;
        for (auto *button : entry.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("识别包装文字"))
                recognize = button;
            if (button->text() == QStringLiteral("取消识别"))
                cancel = button;
        }
        check(recognize && cancel, "OCR controls");
        recognize->click();
        check(!entry.loadImageFile(path), "busy OCR blocks direct replacement");
        check(!drop(preview, {QUrl::fromLocalFile(path)}), "busy OCR blocks drop");
        cancel->click();
        // Use another widget for a pending submission with no acknowledgement.
        PackagingEntryWidget pending;
        pending.show();
        pending.findChild<QLineEdit *>("foodName")->setText(QStringLiteral("番茄"));
        pending.findChild<QComboBox *>("foodUnit")->setCurrentText("g");
        pending.findChild<QComboBox *>("dateMode")->setCurrentIndex(3);
        pending.findChild<QCheckBox *>("confirmed")->setChecked(true);
        pending.findChild<QPushButton *>("saveFood")->click();
        pending.findChild<QPushButton *>("submitCandidate")->click();
        check(!pending.isEnabled() && !pending.loadImageFile(path), "submission blocks replacement");
        pending.completeSubmission(false);
        check(pending.loadImageFile(path), "replacement restored after failed submission");
        std::cout << "Packaging local image drop and preservation checks PASS\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
