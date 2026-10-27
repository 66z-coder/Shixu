
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
    // All-or-nothing batch insert. Unknown weight is zero; errors leave no partial batch.
    bool addFoods(const QList<FoodCandidate>& foods, QString* error = nullptr);

    QList<FoodCandidate> getAllItems() const;

    QList<FoodCandidate> getExpiredItems(
        const QDate& currentDate) const;

    QList<FoodCandidate> getExpiringItems(
        const QDate& currentDate,
        int days = 3) const;

private:
    QSqlDatabase database;
    QString initializationError_;
};

#endif // INVENTORY_MANAGER_H
