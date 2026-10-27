#pragma once
#include <QDate>
#include <QStringList>

enum class ShelfLifeUnit { Days, Months, Years };
struct DateRecognitionResult
{
    QDate productionDate;
    QDate expiryDate;
    int shelfLife = 0;
    ShelfLifeUnit shelfLifeUnit = ShelfLifeUnit::Days;
    bool requiresManualResolution = false;
    QStringList messages;
};
