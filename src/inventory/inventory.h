
#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <QDate>
#include <QList>
#include <QSqlDatabase>

#include "model/FoodCandidate.h"

class InventoryManager
{
public:
    InventoryManager();
    ~InventoryManager();

    void addFood(const FoodCandidate& candidate);

    QList<FoodCandidate> getAllItems() const;

    QList<FoodCandidate> getExpiredItems(
        const QDate& currentDate) const;

    QList<FoodCandidate> getExpiringItems(
        const QDate& currentDate,
        int days = 3) const;

private:
    QSqlDatabase database;
};

#endif // INVENTORY_MANAGER_H
