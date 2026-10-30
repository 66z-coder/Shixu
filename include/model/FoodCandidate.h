
#pragma once

#include <QString>
#include <QDate>

struct FoodCandidate
{
    QString name;           // 食材名称
    double quantity = 1.0;  // 食材数量
    QString unit = "个";    // 单位
    QDate expiryDate;       // 用户确认的到期日期
    double gramsPerUnit = 0.0; // 计数单位的用户填写单件净重(g)；0 表示未填写
    double totalWeightGrams = 0.0; // 总净重(g)；0 表示未知，不能当作无库存
    qint64 batchId = 0; // 库存批次标识；录入时为 0
    QString category; // 空值表示尚未分类
    // g/kg/斤按单位计算总重；计数单位=quantity*gramsPerUnit；ml/L不假设密度。

};
