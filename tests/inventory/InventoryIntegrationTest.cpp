#include "inventory/inventory.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString originalPath = QDir::currentPath();
    QTemporaryDir directory(originalPath + QStringLiteral("/inventory-test-XXXXXX"));
    if (!directory.isValid() || !QDir::setCurrent(directory.path())) {
        qWarning() << "Temporary database directory unavailable:" << directory.errorString() << directory.path();
        return 1;
    }

    InventoryManager inventory;
    const QDate today(2026, 10, 20);
    inventory.addFood({QStringLiteral("苹果"), 3, QStringLiteral("个"), today.addDays(2)});
    inventory.addFood({QStringLiteral("牛奶"), 1, QStringLiteral("盒"), today.addDays(-1)});

    const auto all = inventory.getAllItems();
    const auto expiring = inventory.getExpiringItems(today, 3);
    const auto expired = inventory.getExpiredItems(today);
    const bool passed = all.size() == 2 && expiring.size() == 1 && expired.size() == 1
        && expiring.front().name == QStringLiteral("苹果")
        && expired.front().name == QStringLiteral("牛奶");
    QDir::setCurrent(originalPath);
    return passed ? 0 : 2;
}
