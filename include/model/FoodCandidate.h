
#pragma once

#include <QString>
#include <QDate>

struct FoodCandidate
{
    QString name;           // 食材名称
    double quantity = 1.0;  // 食材数量
    QString unit = "个";    // 单位
    QDate expiryDate;       // 用户手动填写的到期日期
};
