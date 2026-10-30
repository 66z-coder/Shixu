#include "inventory/inventory.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlDatabase>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString originalPath = QDir::currentPath();
    QTemporaryDir directory(originalPath + QStringLiteral("/inventory-test-XXXXXX"));
    if (!directory.isValid() || !QDir::setCurrent(directory.path())) {
        qWarning() << "Temporary database directory unavailable:" << directory.errorString() << directory.path();
        return 1;
    }

    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "legacy-fixture");
        db.setDatabaseName("shixu.db");
        if (!db.open()) return 3;
        QSqlQuery q(db);
        if (!q.exec("CREATE TABLE foods (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,quantity REAL NOT NULL,unit TEXT NOT NULL,expiryDate TEXT,category TEXT NOT NULL DEFAULT '')") ||
            !q.exec(QStringLiteral("INSERT INTO foods(name,quantity,unit) VALUES('番茄',1,'个')")) ||
            !q.exec(QStringLiteral("INSERT INTO foods(name,quantity,unit,category) VALUES('青菜',1,'个','其他'),('肉',1,'斤','其他')"))) return 4;
        q.finish(); db.close();
    }
    QSqlDatabase::removeDatabase("legacy-fixture");
    bool passed = false;
    {
    InventoryManager inventory;
    const QDate today(2026, 10, 20);
    FoodCandidate apple{QStringLiteral("苹果"), 3, QStringLiteral("个"), today.addDays(2)};
    apple.category = QStringLiteral("水果");
    FoodCandidate milk{QStringLiteral("牛奶"), 1, QStringLiteral("盒"), today.addDays(-1)};
    milk.category = QStringLiteral("蛋奶");
    inventory.addFood(apple);
    inventory.addFood(milk);

    auto all = inventory.getAllItems();
    if (all.size() != 5 || all[0].category != QStringLiteral("其他") ||
        all[1].category != QStringLiteral("其他") || all[2].category != QStringLiteral("其他") ||
        all[3].category != QStringLiteral("水果") || all[4].category != QStringLiteral("蛋奶")) return 5;
    const qint64 firstId = all[0].batchId;
    QSqlQuery revision(QSqlDatabase::database("ShixuInventoryConnection"));
    if (!revision.exec("SELECT revision FROM shixu_inventory_meta WHERE id=1") || !revision.next()) return 9;
    const auto previousRevision = revision.value(0).toLongLong();
    revision.finish();
    FoodCandidate edit = all[0];
    edit.name = QStringLiteral("苹果"); edit.category = QStringLiteral("其他");
    edit.quantity = 2; edit.unit = QStringLiteral("个"); edit.gramsPerUnit = 120;
    QString error;
    if (!inventory.updateFood(edit, &error)) return 6;
    if (!revision.exec("SELECT revision FROM shixu_inventory_meta WHERE id=1") || !revision.next() ||
        revision.value(0).toLongLong() != previousRevision + 1) return 10;
    revision.finish();
    all = inventory.getAllItems();
    if (all[0].batchId != firstId || all[0].category != QStringLiteral("其他") ||
        all[0].totalWeightGrams != 240 || all[3].quantity != 3) return 7;
    edit.quantity = 0;
    if (inventory.updateFood(edit, &error) || inventory.getAllItems()[0].quantity != 2) return 8;
    const auto expiring = inventory.getExpiringItems(today, 3);
    const auto expired = inventory.getExpiredItems(today);
    passed = all.size() == 5 && expiring.size() == 1 && expired.size() == 1
        && expiring.front().name == QStringLiteral("苹果")
        && expired.front().name == QStringLiteral("牛奶");
    }
    {
        InventoryManager reopened;
        const auto items = reopened.getAllItems();
        if (items.size() != 5 || items[0].category != QStringLiteral("其他") ||
            items[1].category != QStringLiteral("其他") || items[2].category != QStringLiteral("其他") ||
            items[3].category != QStringLiteral("水果")) return 11;
    }
    QDir::setCurrent(originalPath);
    return passed ? 0 : 2;
}
