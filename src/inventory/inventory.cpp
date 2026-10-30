#include <QDir>
#include "inventory.h"
#include "model/FoodCategory.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSet>
#include <QStringList>
#include <cmath>

namespace {
bool prepareFood(FoodCandidate &food) {
    food.name = food.name.trimmed(); food.unit = food.unit.trimmed();
    if (food.name.isEmpty() || food.unit.isEmpty() || !std::isfinite(food.quantity) || food.quantity <= 0 ||
        !std::isfinite(food.gramsPerUnit) || food.gramsPerUnit < 0 ||
        !std::isfinite(food.netContentPerUnit) || food.netContentPerUnit < 0 ||
        !QStringList{"g", "kg", "ml", "L"}.contains(food.netContentUnit)) return false;
    const QString unit = food.unit.normalized(QString::NormalizationForm_KC).toLower();
    double factor = 0;
    if (unit == "g" || unit == "克") factor = 1;
    else if (unit == "kg" || unit == "公斤" || unit == "千克") factor = 1000;
    else if (unit == "mg" || unit == "毫克") factor = 0.001;
    else if (unit == "斤") factor = 500;
    const bool counted = QStringList{"个","盒","袋","瓶","包","罐","根","颗","枚","只","支","把","份"}.contains(unit);
    if (counted) {
        if (food.netContentPerUnit > 0) {
            food.gramsPerUnit = food.netContentUnit == "g" ? food.netContentPerUnit
                : food.netContentUnit == "kg" ? food.netContentPerUnit * 1000.0 : 0.0;
        } else if (food.gramsPerUnit > 0) {
            food.netContentPerUnit = food.gramsPerUnit; food.netContentUnit = "g";
        }
    } else { food.netContentPerUnit = 0; food.netContentUnit = "g"; }
    if (!std::isfinite(food.quantity * food.netContentPerUnit)) return false;
    if (factor > 0) { food.gramsPerUnit = 0; food.totalWeightGrams = food.quantity * factor; }
    else if (counted) food.totalWeightGrams = food.quantity * food.gramsPerUnit;
    else { food.gramsPerUnit = 0; food.totalWeightGrams = 0; }
    return std::isfinite(food.totalWeightGrams);
}
}

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
    for (const QString& column : {QStringLiteral("gramsPerUnit"), QStringLiteral("totalWeightGrams"), QStringLiteral("netContentPerUnit")}) {
        if (!columns.contains(column) &&
            !query.exec("ALTER TABLE foods ADD COLUMN " + column + " REAL NOT NULL DEFAULT 0")) {
            fail(query.lastError().text()); return;
        }
    }
    if (!columns.contains(QStringLiteral("category")) &&
        !query.exec("ALTER TABLE foods ADD COLUMN category TEXT NOT NULL DEFAULT ''")) {
        fail(query.lastError().text()); return;
    }
    if (!columns.contains(QStringLiteral("netContentUnit")) &&
        !query.exec("ALTER TABLE foods ADD COLUMN netContentUnit TEXT NOT NULL DEFAULT 'g'")) {
        fail(query.lastError().text()); return;
    }
    // Old records without a label stay editable under the neutral category.
    if (!query.exec("UPDATE foods SET category='其他' WHERE category IS NULL OR category=''")) {
        fail(query.lastError().text()); return;
    }



        if (!query.exec(
        "CREATE TABLE IF NOT EXISTS shixu_inventory_meta ("
        "id INTEGER PRIMARY KEY CHECK (id = 1),"
        "revision INTEGER NOT NULL)"
    )) {
        fail(query.lastError().text());
        return;
    }

    if (!query.exec(
        "INSERT OR IGNORE INTO shixu_inventory_meta (id, revision) "
        "VALUES (1, 0)"
    )) {
        fail(query.lastError().text());
        return;
    }



        if (!query.exec(
        "CREATE TABLE IF NOT EXISTS shixu_consumption_events ("
        "event_id TEXT PRIMARY KEY,"
        "request_data TEXT NOT NULL,"
        "result_data TEXT NOT NULL)"
    )) {
        fail(query.lastError().text());
        return;
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

bool InventoryManager::normalizeNames(const std::function<QString(const QString&)>& normalize, QString* error)
{
    if(error) error->clear();
    auto fail=[&](const QString& message){database.rollback();if(error)*error=message;return false;};
    if(!initializationError_.isEmpty() || !database.transaction())
        return fail(QStringLiteral("无法读取旧库存名称。"));
    QSqlQuery query(database);
    if(!query.exec("SELECT id,name FROM foods WHERE quantity>0"))return fail(query.lastError().text());
    QList<QPair<qint64,QString>> changes;
    while(query.next()){
        auto old=query.value(1).toString(), name=normalize(old);
        if(name.isEmpty())return fail(QStringLiteral("库存名称无法规范化。"));
        if(name!=old)changes.append({query.value(0).toLongLong(),name});
    }
    query.finish();
    for(const auto& item:changes){
        query.prepare("UPDATE foods SET name=? WHERE id=?");query.addBindValue(item.second);query.addBindValue(item.first);
        if(!query.exec())return fail(query.lastError().text());
    }
    if(!changes.isEmpty()&&!query.exec("UPDATE shixu_inventory_meta SET revision=revision+1 WHERE id=1"))return fail(query.lastError().text());
    if(!database.commit())return fail(database.lastError().text());
    return true;
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
        if (!std::isfinite(food.totalWeightGrams) || food.totalWeightGrams < 0 || !prepareFood(food))
            return fail("食材名称、单位、数量或重量无效");
        if (food.category.isEmpty()) food.category = QStringLiteral("其他");
        if (!foodCategories().contains(food.category)) return fail("食材分类无效");
        prepared.append(food);
    }

    if (!database.transaction()) return fail("无法开始保存：" + database.lastError().text());
    auto rollback = [&](const QString& message) {
        database.rollback();
        return fail(message);
    };
    QSqlQuery query(database);
    if (!query.prepare("INSERT INTO foods (name,quantity,unit,expiryDate,gramsPerUnit,totalWeightGrams,category,netContentPerUnit,netContentUnit) "
                       "VALUES (:name,:quantity,:unit,:expiry,:perUnit,:total,:category,:netContent,:netUnit)"))
        return rollback("无法准备保存：" + query.lastError().text());
    for (const FoodCandidate& food : prepared) {
        query.bindValue(":name", food.name);
        query.bindValue(":quantity", food.quantity);
        query.bindValue(":unit", food.unit);
        query.bindValue(":expiry", food.expiryDate.isValid()
            ? QVariant(food.expiryDate.toString(Qt::ISODate)) : QVariant());
        query.bindValue(":perUnit", food.gramsPerUnit);
        query.bindValue(":total", food.totalWeightGrams);
        query.bindValue(":category", food.category);
        query.bindValue(":netContent", food.netContentPerUnit);
        query.bindValue(":netUnit", food.netContentUnit);
        if (!query.exec()) return rollback("保存失败：" + query.lastError().text());
        query.finish();
    }



        QSqlQuery revisionQuery(database);

    if (!revisionQuery.exec(
        "UPDATE shixu_inventory_meta "
        "SET revision = revision + 1 "
        "WHERE id = 1"
    )) {
        return rollback(
            "更新库存版本失败：" + revisionQuery.lastError().text()
        );
    }

    if (revisionQuery.numRowsAffected() != 1) {
        return rollback("库存版本记录不存在");
    }





    if (!database.commit()) return rollback("提交失败：" + database.lastError().text());
    return true;
}

bool InventoryManager::updateFood(const FoodCandidate &candidate, QString *error) {
    if (error) error->clear();
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    FoodCandidate food = candidate;
    if (food.batchId <= 0 || !foodCategories().contains(food.category) || !prepareFood(food))
        return fail(QStringLiteral("请检查食材名称、分类、数量、单位和重量。"));
    if (!initializationError_.isEmpty() || !database.isOpen() || !database.transaction())
        return fail(QStringLiteral("无法开始保存库存修改。"));
    auto rollback = [&](const QString &message) { database.rollback(); return fail(message); };
    QSqlQuery query(database);
    query.prepare("UPDATE foods SET name=?,quantity=?,unit=?,expiryDate=?,gramsPerUnit=?,totalWeightGrams=?,category=?,netContentPerUnit=?,netContentUnit=? WHERE id=? AND quantity>0");
    query.addBindValue(food.name); query.addBindValue(food.quantity); query.addBindValue(food.unit);
    query.addBindValue(food.expiryDate.isValid() ? QVariant(food.expiryDate.toString(Qt::ISODate)) : QVariant());
    query.addBindValue(food.gramsPerUnit); query.addBindValue(food.totalWeightGrams);
    query.addBindValue(food.category); query.addBindValue(food.netContentPerUnit);
    query.addBindValue(food.netContentUnit); query.addBindValue(food.batchId);
    if (!query.exec()) return rollback(QStringLiteral("保存失败：") + query.lastError().text());
    if (query.numRowsAffected() != 1) return rollback(QStringLiteral("该库存批次已不存在，请刷新后重试。"));
    if (!query.exec("UPDATE shixu_inventory_meta SET revision=revision+1 WHERE id=1") || query.numRowsAffected() != 1)
        return rollback(QStringLiteral("无法更新库存版本。"));
    if (!database.commit()) return rollback(QStringLiteral("无法提交库存修改。"));
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams, id, category, netContentPerUnit, netContentUnit "
        "FROM foods WHERE quantity > 0 ORDER BY id"

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
        item.batchId = query.value(6).toLongLong(); item.category = query.value(7).toString();
        item.netContentPerUnit = query.value(8).toDouble(); item.netContentUnit = query.value(9).toString();
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams, id, category, netContentPerUnit, netContentUnit "
        "FROM foods WHERE quantity > 0 AND expiryDate IS NOT NULL "
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
        item.batchId = query.value(6).toLongLong(); item.category = query.value(7).toString();
        item.netContentPerUnit = query.value(8).toDouble(); item.netContentUnit = query.value(9).toString();
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
        "SELECT name, quantity, unit, expiryDate, gramsPerUnit, totalWeightGrams, id, category, netContentPerUnit, netContentUnit "
        "FROM foods WHERE quantity > 0 AND expiryDate IS NOT NULL "
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
        item.batchId = query.value(6).toLongLong(); item.category = query.value(7).toString();
        item.netContentPerUnit = query.value(8).toDouble(); item.netContentUnit = query.value(9).toString();
        result.append(item);
    }

    return result;
}
