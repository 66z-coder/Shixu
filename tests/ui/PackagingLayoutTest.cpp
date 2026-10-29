#include "recognition/PackagingEntryWidget.h"
#include "ui/MainWindow.h"
#include "ui/RecommendationWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    // The offscreen platform has no system font discovery on Windows.
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == "offscreen") {
        const int font =
            QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/msyh.ttc");
        check(font >= 0, "load Chinese font for offscreen rendering");
        app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(), 10));
    }
#endif
    const QString output = qEnvironmentVariable("SHIXU_UI_CAPTURE_DIR");
    const QString original = QDir::currentPath();
    QTemporaryDir data;
    if (!data.isValid())
        return 1;
    QDir::setCurrent(data.path());
    int result = 0;
    try {
        MainWindow window(nullptr, data.filePath("reward.ini"));
        window.showPage(MainWindow::Page::FoodEntry);
        window.show();
        auto *entry = window.findChild<PackagingEntryWidget *>();
        auto *scroll = entry->findChild<QScrollArea *>("entryScroll");
        auto *list = entry->findChild<QListWidget *>("pendingFoods");
        auto *submit = entry->findChild<QPushButton *>("submitCandidate");
        for (const QString &name : {QStringLiteral("番茄"), QStringLiteral("牛奶"),
                                    QStringLiteral("这是一条用于验证自动换行的较长食材名称")}) {
            entry->findChild<QLineEdit *>("foodName")->setText(name);
            entry->findChild<QComboBox *>("foodUnit")->setCurrentText("盒");
            entry->findChild<QComboBox *>("dateMode")->setCurrentIndex(3);
            entry->findChild<QCheckBox *>("confirmed")->setChecked(true);
            entry->findChild<QPushButton *>("saveFood")->click();
        }
        list->setCurrentRow(0);
        check(list->count() == 3 && submit->isEnabled(), "queued foods ready to submit");
        for (QSize size : {QSize(1280, 800), QSize(980, 620)}) {
            window.resize(size);
            scroll->verticalScrollBar()->setValue(0);
            for (int i = 0; i < 5; ++i)
                app.processEvents();
            check(window.size() == size, "window must fit requested size");
            check(scroll->horizontalScrollBar()->maximum() == 0, "editor must fit viewport width");
            check(list->horizontalScrollBar()->maximum() == 0, "long list text must wrap");
            check(scroll->verticalScrollBar()->maximum() > 0, "long form must scroll");
            const QRect submitRect(submit->mapTo(entry, QPoint()), submit->size());
            check(entry->rect().contains(submitRect), "submit button must stay visible");
            for (auto *field : entry->findChildren<QComboBox *>()) {
                const QRect fieldRect(field->mapTo(scroll->widget(), QPoint()), field->size());
                check(fieldRect.right() < scroll->viewport()->width(),
                      "form field must not be clipped horizontally");
            }
            if (!output.isEmpty()) {
                QDir().mkpath(output);
                check(window.grab().save(output +
                                         QString("/entry-%1x%2.png").arg(size.width()).arg(size.height())),
                      "save screenshot");
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                app.processEvents();
                check(window.grab().save(
                          output + QString("/entry-form-%1x%2.png").arg(size.width()).arg(size.height())),
                      "save form screenshot");
            }
        }
        entry->findChild<QLabel *>("entryStatus")
            ->setText(
                QStringLiteral(
                    "包装日期无法确定，请核对完整年份和日期。请重新选择清晰图片，或者手动输入日期信息。")
                    .repeated(3));
        app.processEvents();
        check(scroll->horizontalScrollBar()->maximum() == 0, "long status must wrap");
        check(!window.findChild<QPushButton *>("pendingCookingButton"), "no global pending strip");
        auto *recommendation = window.findChild<RecommendationWidget *>();
        window.showPage(MainWindow::Page::Recommendations);
        recommendation->selectMoodV1(Shixu::Contracts::V1::Mood::Tired);
        recommendation->requestRecommendations();
        for (const auto page :
             {MainWindow::Page::Recommendations, MainWindow::Page::CheckIn, MainWindow::Page::Statistics,
              MainWindow::Page::Profile, MainWindow::Page::Inventory}) {
            window.showPage(page);
            for (QSize size : {QSize(980, 620), QSize(1280, 800)}) {
                window.resize(size);
                for (int i = 0; i < 5; ++i)
                    app.processEvents();
                check(window.size() == size, "all pages fit target window");
                for (auto *area : window.findChildren<QScrollArea *>())
                    if (area->isVisible())
                        check(area->horizontalScrollBar()->maximum() == 0,
                              "page must not scroll horizontally");
                if (!output.isEmpty())
                    check(window.grab().save(output +
                                             QString("/page-%1-%2.png").arg(int(page)).arg(size.width())),
                          "save page screenshot");
            }
        }
        window.findChild<QLineEdit *>("profileNickname")->setText(QStringLiteral("测试厨师"));
        window.findChild<QPushButton *>("saveProfile")->click();
        check(QFileInfo::exists(data.filePath("profile.json")), "profile saved through UI");
        std::cout << "Packaging layout checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    QDir::setCurrent(original);
    return result;
}
