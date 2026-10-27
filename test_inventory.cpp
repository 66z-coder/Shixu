
#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>

#include "inventory.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    InventoryManager manager;

    // 清理上一次测试留下的数据，避免重复
    QSqlQuery cleanup;
    cleanup.exec(
        "DELETE FROM foods WHERE name IN "
        "('测试香蕉', '测试牛奶', '测试面包')"
    );

    // 添加三条测试数据
    FoodCandidate banana;
    banana.name = "测试香蕉";
    banana.quantity = 2;
    banana.unit = "根";
    banana.expiryDate = QDate(2026, 10, 19);
    manager.addFood(banana);

    FoodCandidate milk;
    milk.name = "测试牛奶";
    milk.quantity = 1;
    milk.unit = "盒";
    milk.expiryDate = QDate(2026, 10, 22);
    manager.addFood(milk);

    FoodCandidate bread;
    bread.name = "测试面包";
    bread.quantity = 1;
    bread.unit = "袋";
    bread.expiryDate = QDate(2026, 10, 25);
    manager.addFood(bread);

    // 固定测试日期
    QDate currentDate(2026, 10, 20);

    QList<FoodCandidate> expired =
        manager.getExpiredItems(currentDate);

    QList<FoodCandidate> expiring =
        manager.getExpiringItems(currentDate, 3);

    qDebug() << "Expired count:" << expired.size();

    for (const FoodCandidate& item : expired)
    {
        qDebug() << "Expired:" << item.name
                 << item.expiryDate.toString(Qt::ISODate);
    }

    qDebug() << "Expiring count:" << expiring.size();

    for (const FoodCandidate& item : expiring)
    {
        qDebug() << "Expiring:" << item.name
                 << item.expiryDate.toString(Qt::ISODate);
    }

    return 0;
}
