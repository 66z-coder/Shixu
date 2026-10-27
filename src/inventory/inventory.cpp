#include <QDir>
#include "inventory.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

InventoryManager::InventoryManager()
{
    database = QSqlDatabase::addDatabase("QSQLITE", "ShixuInventoryConnection");
    database.setDatabaseName("shixu.db");
    qDebug() << "Database path:" << database.databaseName();
    qDebug() << "Working directory:" << QDir::currentPath();




    if (!database.open())
    {
        qDebug() << "Database open failed:"
                 << database.lastError().text();
        return;
    }

    QSqlQuery query(database);

    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS foods ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "quantity REAL NOT NULL,"
        "unit TEXT NOT NULL,"
        "expiryDate TEXT"
        ")"
    );

    if (!success)
    {
        qDebug() << "Create foods table failed:"
                 << query.lastError().text();
    }
}

InventoryManager::~InventoryManager()
{
    if (database.isOpen())
    {
        database.close();
    }

    database = QSqlDatabase();
    QSqlDatabase::removeDatabase("ShixuInventoryConnection");
}

void InventoryManager::addFood(const FoodCandidate& candidate)
{
    if (!database.isOpen() ||
        candidate.name.trimmed().isEmpty() ||
        candidate.quantity <= 0)
    {
        return;
    }

    QSqlQuery query(database);
    query.prepare(
        "INSERT INTO foods (name, quantity, unit, expiryDate) "
        "VALUES (:name, :quantity, :unit, :expiryDate)"
    );

    query.bindValue(":name", candidate.name);
    query.bindValue(":quantity", candidate.quantity);
    query.bindValue(":unit", candidate.unit);

    if (candidate.expiryDate.isValid())
    {
        query.bindValue(
            ":expiryDate",
            candidate.expiryDate.toString(Qt::ISODate)
        );
    }
    else
    {
        query.bindValue(":expiryDate", QVariant());
    }

    if (!query.exec())
    {
        qDebug() << "Add food failed:" << query.lastError().text();
    }
}

QList<FoodCandidate> InventoryManager::getAllItems() const
{
    QList<FoodCandidate> result;

    if (!database.isOpen())
    {
        return result;
    }

    QSqlQuery query(database);

    if (!query.exec(
        "SELECT name, quantity, unit, expiryDate FROM foods ORDER BY id"
    ))
    {
        qDebug() << "Get all foods failed:"
                 << query.lastError().text();
        return result;
    }

    while (query.next())
    {
        FoodCandidate item;
        item.name = query.value(0).toString();
        item.quantity = query.value(1).toDouble();
        item.unit = query.value(2).toString();
        item.expiryDate = QDate::fromString(
            query.value(3).toString(),
            Qt::ISODate
        );

        result.append(item);
    }

    return result;
}

QList<FoodCandidate> InventoryManager::getExpiredItems(
    const QDate& currentDate) const
{
    QList<FoodCandidate> result;

    if (!database.isOpen())
    {
        return result;
    }

    QSqlQuery query(database);
    query.prepare(
        "SELECT name, quantity, unit, expiryDate "
        "FROM foods WHERE expiryDate IS NOT NULL "
        "AND expiryDate <> '' AND expiryDate < :currentDate "
        "ORDER BY id"
    );
    query.bindValue(
        ":currentDate",
        currentDate.toString(Qt::ISODate)
    );

    if (!query.exec())
    {
        qDebug() << "Get expired foods failed:"
                 << query.lastError().text();
        return result;
    }

    while (query.next())
    {
        FoodCandidate item;
        item.name = query.value(0).toString();
        item.quantity = query.value(1).toDouble();
        item.unit = query.value(2).toString();
        item.expiryDate = QDate::fromString(
            query.value(3).toString(),
            Qt::ISODate
        );
        result.append(item);
    }

    return result;
}

QList<FoodCandidate> InventoryManager::getExpiringItems(
    const QDate& currentDate,
    int days) const
{
    QList<FoodCandidate> result;

    if (!database.isOpen() || days < 0)
    {
        return result;
    }

    QSqlQuery query(database);
    query.prepare(
        "SELECT name, quantity, unit, expiryDate "
        "FROM foods WHERE expiryDate IS NOT NULL "
        "AND expiryDate <> '' "
        "AND expiryDate >= :currentDate "
        "AND expiryDate <= :endDate "
        "ORDER BY id"
    );

    query.bindValue(
        ":currentDate",
        currentDate.toString(Qt::ISODate)
    );
    query.bindValue(
        ":endDate",
        currentDate.addDays(days).toString(Qt::ISODate)
    );

    if (!query.exec())
    {
        qDebug() << "Get expiring foods failed:"
                 << query.lastError().text();
        return result;
    }

    while (query.next())
    {
        FoodCandidate item;
        item.name = query.value(0).toString();
        item.quantity = query.value(1).toDouble();
        item.unit = query.value(2).toString();
        item.expiryDate = QDate::fromString(
            query.value(3).toString(),
            Qt::ISODate
        );
        result.append(item);
    }

    return result;
}
