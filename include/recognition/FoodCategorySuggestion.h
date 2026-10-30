#pragma once
#include <QStringList>

inline QString suggestFoodCategory(const QString &name) {
    const QList<QPair<QString, QStringList>> rules = {
        {QStringLiteral("饮品"), {QStringLiteral("果汁"), QStringLiteral("饮料"), QStringLiteral("咖啡"), QStringLiteral("茶饮")}},
        {QStringLiteral("蛋奶"), {QStringLiteral("鸡蛋"), QStringLiteral("鸭蛋"), QStringLiteral("牛奶"), QStringLiteral("酸奶"), QStringLiteral("奶酪"), QStringLiteral("芝士")}},
        {QStringLiteral("水产"), {QStringLiteral("鱼"), QStringLiteral("虾"), QStringLiteral("蟹"), QStringLiteral("贝"), QStringLiteral("鱿鱼")}},
        {QStringLiteral("肉禽"), {QStringLiteral("猪肉"), QStringLiteral("牛肉"), QStringLiteral("羊肉"), QStringLiteral("鸡肉"), QStringLiteral("鸡胸"), QStringLiteral("鸡腿"), QStringLiteral("鸭肉"), QStringLiteral("排骨"), QStringLiteral("五花肉")}},
        {QStringLiteral("水果"), {QStringLiteral("苹果"), QStringLiteral("香蕉"), QStringLiteral("橙"), QStringLiteral("梨"), QStringLiteral("葡萄"), QStringLiteral("草莓"), QStringLiteral("西瓜"), QStringLiteral("芒果")}},
        {QStringLiteral("蔬菜"), {QStringLiteral("番茄"), QStringLiteral("西红柿"), QStringLiteral("白菜"), QStringLiteral("青菜"), QStringLiteral("生菜"), QStringLiteral("菠菜"), QStringLiteral("芹菜"), QStringLiteral("西兰花"), QStringLiteral("胡萝卜"), QStringLiteral("黄瓜"), QStringLiteral("土豆"), QStringLiteral("冬瓜"), QStringLiteral("青椒"), QStringLiteral("洋葱"), QStringLiteral("茄子")}},
        {QStringLiteral("主食"), {QStringLiteral("大米"), QStringLiteral("米饭"), QStringLiteral("面条"), QStringLiteral("面包"), QStringLiteral("馒头"), QStringLiteral("面粉")}},
        {QStringLiteral("调味品"), {QStringLiteral("食用油"), QStringLiteral("酱油"), QStringLiteral("食盐"), QStringLiteral("盐"), QStringLiteral("醋"), QStringLiteral("蚝油"), QStringLiteral("白糖")}}
    };
    for (const auto &rule : rules)
        for (const auto &word : rule.second)
            if (name.contains(word, Qt::CaseInsensitive)) return rule.first;
    if (name.trimmed() == QStringLiteral("肉")) return QStringLiteral("肉禽");
    return QStringLiteral("其他");
}
