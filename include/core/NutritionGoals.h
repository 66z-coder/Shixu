#pragma once
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QString>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

// Order: calories (kcal), protein, carbohydrates and fat (g).
using NutritionGoals = std::array<std::optional<double>, 4>;
class NutritionGoalsStore {
  public:
    explicit NutritionGoalsStore(QString path) : m_path(std::move(path)) {}
    bool load(NutritionGoals &goals, QString &error) const {
        error.clear();
        NutritionGoals result{};
        if (!QFile::exists(m_path)) { goals = result; return true; }
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("营养目标无法读取，请检查文件权限。"); return false;
        }
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
        if (file.error() != QFileDevice::NoError || parse.error != QJsonParseError::NoError || !doc.isObject() ||
            doc.object().value("version").toInt() != 1 || !doc.object().value("goals").isArray()) {
            error = QStringLiteral("营养目标文件无法读取，请保留原文件。"); return false;
        }
        const auto array = doc.object().value("goals").toArray();
        if (array.size() != 4) { error = QStringLiteral("营养目标文件格式有误，请保留原文件。"); return false; }
        for (int i = 0; i < 4; ++i) {
            if (array[i].isNull()) continue;
            if (!array[i].isDouble() || !std::isfinite(array[i].toDouble()) || array[i].toDouble() <= 0) {
                error = QStringLiteral("营养目标文件内容有误，请保留原文件。"); return false;
            }
            result[i] = array[i].toDouble();
        }
        goals = result;
        return true;
    }
    bool save(const NutritionGoals &goals, QString &error) const {
        error.clear();
        QJsonArray array;
        for (const auto &goal : goals) {
            if (goal && (!std::isfinite(*goal) || *goal <= 0)) {
                error = QStringLiteral("目标必须是大于零的有效数字。"); return false;
            }
            array.append(goal ? QJsonValue(*goal) : QJsonValue(QJsonValue::Null));
        }
        QLockFile lock(m_path + ".lock");
        if (!lock.tryLock(0)) { error = QStringLiteral("营养目标正在使用或无法保存，请稍后重试。"); return false; }
        NutritionGoals previous{};
        if (!load(previous, error)) return false;
        QSaveFile file(m_path);
        file.setDirectWriteFallback(false);
        const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"goals", array}}).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            error = QStringLiteral("营养目标保存失败，原目标已保留。"); return false;
        }
        return true;
    }
  private:
    QString m_path;
};
