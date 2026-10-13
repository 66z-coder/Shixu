#include "recommendation/RecommendationEngine.h"

#include <QApplication>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

using namespace Shixu::RecommendationEngine;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("食序 - D 同学推荐模块演示"));
    window.resize(640, 520);

    auto* layout = new QVBoxLayout(&window);
    auto* note = new QLabel(QStringLiteral(
        "下面是模拟菜谱和库存计算出的结果。它还没有连接组员 B、C 的真实数据。"));
    note->setWordWrap(true);
    auto* output = new QTextEdit;
    output->setReadOnly(true);
    layout->addWidget(note);
    layout->addWidget(output);

    const UserContext context = makeDemoUserContext();
    const std::vector<Recommendation> recommendations = recommend(context);

    if (recommendations.empty()) {
        output->setPlainText(QStringLiteral("没有符合条件的推荐菜品。"));
    } else {
        for (int index = 0; index < static_cast<int>(recommendations.size()); ++index) {
            const Recommendation& item = recommendations[static_cast<std::size_t>(index)];
            const ExplainResult explanation = explainRecommendation(item);
            output->append(QStringLiteral("%1. %2   综合得分：%3/100")
                               .arg(index + 1)
                               .arg(item.dishName)
                               .arg(item.totalScore, 0, 'f', 1));
            output->append(QStringLiteral(
                "食材匹配 %1 | 临期利用 %2 | 口味偏好 %3 | 营养 %4 | 时间 %5")
                               .arg(item.ingredientMatchScore, 0, 'f', 0)
                               .arg(item.expiryUrgencyScore, 0, 'f', 0)
                               .arg(item.preferenceMatchScore, 0, 'f', 0)
                               .arg(item.nutritionScore, 0, 'f', 0)
                               .arg(item.preparationTimeScore, 0, 'f', 0));
            output->append(QStringLiteral("推荐理由：%1")
                               .arg(explanation.reasons.join(QStringLiteral("；"))));
            output->append(QString());
        }
    }

    window.show();
    return app.exec();
}
