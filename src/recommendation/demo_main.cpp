#include "recommendation/RecommendationEngine.h"
#include "RecommendationText.h"

#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(QStringLiteral("食序 - 推荐算法示例（C 菜谱库）"));
    window.resize(760, 600);
    auto* layout = new QVBoxLayout(&window);
    auto* note = new QLabel(QStringLiteral(
        "本窗口使用固定示例库存，调用 C 的菜谱匹配和营养接口。\n"
        "要试用 B 的 SQLite 库存，请运行 RecommendationLiveDemo。"));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* output = new QPlainTextEdit;
    output->setReadOnly(true);
    layout->addWidget(output);
    output->setPlainText(recommendationText(
        Shixu::RecommendationEngine::recommend(Shixu::RecommendationEngine::makeDemoUserContext())));
    window.show();
    return app.exec();
}
