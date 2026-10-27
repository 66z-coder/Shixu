
#include <QApplication>
#include <QMessageBox>
#include <QStringList>

#include "recognition/PackagingEntryWidget.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    PackagingEntryWidget window;

    // 接收 A 模块提交的食材
    QObject::connect(
        &window,
        &PackagingEntryWidget::foodsConfirmed,
        &window,
        [&window](const std::vector<FoodCandidate>& foods) {

            QStringList lines;

            for (const auto& food : foods) {

                // 1. 校验食材名称
                if (food.name.trimmed().isEmpty()) {
                    QMessageBox::warning(
                        &window, "错误", "食材名称不能为空");
                    window.completeSubmission(false, "测试校验失败，未入库。");
                    return;
                }

                // 2. 校验数量
                if (food.quantity <= 0) {
                    QMessageBox::warning(
                        &window, "错误", "食材数量必须大于0");
                    window.completeSubmission(false, "测试校验失败，未入库。");
                    return;
                }

                // 3. 校验单位
                if (food.unit.trimmed().isEmpty()) {
                    QMessageBox::warning(
                        &window, "错误", "食材单位不能为空");
                    window.completeSubmission(false, "测试校验失败，未入库。");
                    return;
                }

                // 4. 获取到期日期
                QString dateText;

                if (food.expiryDate.isValid()) {
                    dateText =
                        food.expiryDate.toString("yyyy-MM-dd");
                } else {
                    dateText = "未知";
                }

                // 5. 生成显示内容
                lines << QString("%1 × %2 %3 | 到期：%4")
                             .arg(food.name)
                             .arg(food.quantity)
                             .arg(food.unit)
                             .arg(dateText)
                             + (food.totalWeightGrams > 0
                                ? QString(" | 总重量：%1 g").arg(food.totalWeightGrams, 0, 'f', 1)
                                : QStringLiteral(" | 总重量：未知"));
            }

            window.completeSubmission(true, "测试模式：已接收数据，未保存到库存。");

            // 6. 显示最终收到的数据
            QMessageBox::information(
                &window,
                "提交测试",
                QString("成功接收到 %1 条食材数据：\n\n%2\n\n"
                        "当前为测试模式，尚未保存到库存。")
                    .arg(static_cast<int>(foods.size()))
                    .arg(lines.join("\n"))
            );
        }
    );

    window.show();

    return app.exec();
}
