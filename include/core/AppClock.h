#pragma once
#include <QDateTime>
#include <QTimeZone>
namespace Shixu {
inline QDateTime appNow() { return QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Shanghai")); }
} // namespace Shixu
