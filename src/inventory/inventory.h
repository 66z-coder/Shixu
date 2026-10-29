
#ifndef INVENTORY_MANAGER_H
#define INVENTORY_MANAGER_H

#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include <functional>

#include "model/FoodCandidate.h"

namespace Shixu::Contracts::V1 {
    class InventoryContractAdapter;
}


class InventoryManager
{
public:
    InventoryManager();
    ~InventoryManager();

    void addFood(const FoodCandidate& candidate);
    // All-or-nothing batch insert. Unknown weight is zero; errors leave no partial batch.
    bool addFoods(const QList<FoodCandidate>& foods, QString* error = nullptr);
    // Integration supplies A's canonicalizer; mutate names and revision atomically.
    bool normalizeNames(const std::function<QString(const QString&)>& normalize, QString* error);

    QList<FoodCandidate> getAllItems() const;

    QList<FoodCandidate> getExpiredItems(
        const QDate& currentDate) const;

    QList<FoodCandidate> getExpiringItems(
        const QDate& currentDate,
        int days = 3) const;

private:
    friend class Shixu::Contracts::V1::InventoryContractAdapter;

    QSqlDatabase database;
    QString initializationError_;
};

#endif // INVENTORY_MANAGER_H
