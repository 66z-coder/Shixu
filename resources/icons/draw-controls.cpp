// Development-only generator for QSS control glyphs; not linked into the application.
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QDir>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 2) return 1;
    const QDir output(QString::fromLocal8Bit(argv[1]));
    for (const QString name : {QStringLiteral("chevron-down"), QStringLiteral("chevron-up"), QStringLiteral("check")}) {
        QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(name == "check" ? Qt::white : QColor("#527D94"), 3.5,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        if (name == "check") { path.moveTo(7,16); path.lineTo(13,22); path.lineTo(25,10); }
        else if (name == "chevron-down") { path.moveTo(8,12); path.lineTo(16,20); path.lineTo(24,12); }
        else { path.moveTo(8,20); path.lineTo(16,12); path.lineTo(24,20); }
        painter.drawPath(path);
        painter.end();
        if (!image.save(output.filePath(name + ".png"))) return 2;
    }
}
