#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLabel label("食序");
    label.resize(400, 200);
    label.show();

    return app.exec();
}