#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString rewardFile;
    const QDir appDir(QCoreApplication::applicationDirPath());
    if (QFileInfo(appDir.filePath("portable.flag")).isFile()) {
        const QString dataDir = appDir.filePath("data");
        if (!QDir().mkpath(dataDir) || !QDir::setCurrent(dataDir)) {
            QMessageBox::critical(nullptr, QStringLiteral("无法启动"), QStringLiteral("无法创建数据目录，请将完整运行包解压到可写文件夹后重试。"));
            return 1;
        }
        rewardFile = QDir(dataDir).filePath("rewards.ini");
    }
    MainWindow window(nullptr, rewardFile);
    window.show();


    return app.exec();
}
