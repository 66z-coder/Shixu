#pragma once

#include <QString>
#include <QDate>

struct FoodCandidate
{
    QString name;
    double quantity = 1.0;
    QString unit = "个";
    QDate expiryDate;
};