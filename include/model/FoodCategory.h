#pragma once
#include <QStringList>

inline QStringList foodCategories() {
    return {QStringLiteral("蔬菜"), QStringLiteral("水果"), QStringLiteral("肉禽"),
            QStringLiteral("水产"), QStringLiteral("蛋奶"), QStringLiteral("饮品"),
            QStringLiteral("主食"), QStringLiteral("调味品"), QStringLiteral("其他")};
}
