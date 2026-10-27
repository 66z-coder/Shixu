#include <QDir>
#include "inventory.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSet>
#include <QStringList>
#include <cmath>

InventoryManager::InventoryManager()
{
    database = QSqlDatabase::addDatabase("QSQLITE", "ShixuInventoryConnection");
    database.setDatabaseName("shixu.db");
    qDebug() << "Database path:" << database.databaseName();
    qDebug() << "Working directory:" << QDir::currentPath();




    if (!database.open()) {
        initializationError_ = database.lastError().text();
        return;
    }
    // Migrate in place: legacy rows keep their original values and unknown weights.
    if (!database.transaction()) {
        initializationError_ = database.lastError().text();
        return;
    }
    QSqlQuery query(database);
    auto fail = [this](const QString& error) {
        initializationError_ = error;
        database.rollback();
    };
    if (!query.exec("CREATE TABLE IF NOT EXISTS foods ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL,"
                    "quantity REAL NOT NULL, unit TEXT NOT NULL, expiryDate TEXT,"
                    "gramsPerUnit REAL NOT NULL DEFAULT 0,"
                    "totalWeightGrams REAL NOT NULL DEFAULT 0)")) {
        fail(query.lastError().text()); return;
    }
    QSet<QString> columns;
    if (!query.exec("PRAGMA table_info(foods)")) {
        fail(query.lastError().text()); return;
    }
    while (query.next()) columns.insert(query.value(1).toString());
    query.finish();
    for (const QString& column : {QStringLiteral("gramsPerUnit"), QStringLiteral("totalWeightGrams")}) {
        if (!columns.contains(column) &&
            !query.exec("ALTER TABLE foods ADD COLUMN " + column + " REAL NOT NULL DEFAULT 0")) {
            fail(query.lastError().text()); return;
        }
    }
    if (!database.commit()) fail(database.lastError().text());
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
    QString error;
    if (!addFoods({candidate}, &error)) qWarning() << "Add food failed:" << error;
}

bool InventoryManager::addFoods(const QList<FoodCandidate>& foods, QString* error)
{
    if (error) error->clear();
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    if (!initializationError_.isEmpty())
        return fail("库存初始化失败：" + initializationError_);
    if (!database.isOpen()) return fail("库存数据库未打开");
    if (foods.isEmpty()) return fail("没有需要保存的食材");

    QList<FoodCandidate> prepared;
    for (FoodCandidate food : foods) {
        food.name = food.name.trimmed();
        food.unit = food.unit.trimmed();
        if (food.name.isEmpty() || food.unit.isEmpty() ||
            !std::isfinite(food.quantity) || food.quantity <= 0 ||
            !std::isfinite(food.gramsPerUnit) || food.gramsPerUnit < 0 ||
            !std::isfinite(food.totalWeightGrams) || food.totalWeightGrams < 0)
            return fail("食材名称、单位、数量或重量无效");
        const QString unit = food.unit.normalized(QString::NormalizationForm_KC).toLower();
        double factor = 0;
        if (unit == "g" || unit == "克") factor = 1;
        else if (unit == "kg" || unit == "公斤" || unit == "千克") factor = 1000;
        else if (unit == "mg" || unit == "毫克") factor = 0.001;
        else if (unit == "斤") factor = 500;
        const bool counted = QStringList{"个","盒","袋","瓶","包","罐","根","颗",
                                        "枚","只","支","把","份"}.contains(unit);
        if (factor > 0) {
            food.gramsPerUnit = 0;
            food.totalWeightGrams = food.quantity * factor;
        } else if (counted) {
            food.totalWeightGrams = food.quantity * food.gramsPerUnit;
        } else {
            food.gramsPerUnit = 0;
            food.totalWeightGrams = 0; // No density or package-size assumptions.
        }
        if (!std::isfinite(food.totalWeightGrams)) return fail("总重量超出有效范围");
        prepared.append(food);
    }

    if (!database.transaction()) return fail("无法开始保存：" + database.lastError().text());
    auto rollback = [&](const QString& message) {
        database.rollback();
        return fail(message);
    };
    QSqlQuery query(database);
    if (!query.prepare("INSERT INTO foods (name,quantity,unit,expiryDate,gramsPerUnit,totalWeightGrams) "
                       "VALUES (:name,:quantity,:unit,:expiry,:perUnit,:total)"))
        return rollback("无法准备保存：" + query.lastError().text());
    for (const FoodCandidate& food : prepared) {
        query.bindValue(":name", food.name);
        query.bindValue(":quantity", food.quantity);
        query.bindValue(":unit", food.unit);
        query.bindValue(":expiry", food.expiryDate.isValid()
            ? QVariant(food.expiryDate.toString(Qt::ISODate)) : QVariant());
        query.bindValue(":perUnit", food.gramsPerUnit);
        query.bindValue(":total", food.totalWeightGrams);
        if (!query.exec()) return rollback("保存失败：" + query.lastError().text());
        query.finish();
    }
    if (!database.commit()) return rollback("提交失败：" + database.lastError().text());
    return true;
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams FROM foods ORDER BY id"
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

        item.gramsPerUnit = query.value(4).toDouble();
        item.totalWeightGrams = query.value(5).toDouble();
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams "
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
        item.gramsPerUnit = query.value(4).toDouble();
        item.totalWeightGrams = query.value(5).toDouble();
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams "
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
        item.gramsPerUnit = query.value(4).toDouble();
        item.totalWeightGrams = query.value(5).toDouble();
        result.append(item);
    }

    return result;
}
